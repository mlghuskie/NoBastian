#pragma once
#include "Include.h"
#include "FileLogger.h"
#include "../Client/Util.h"
using namespace std;

namespace asmjs
{
	class ABServer
	{
		public:

		string PipeName;
		HANDLE hPipe;
		HANDLE hProcess;
		FileLogger* pFl;

		ABServer(string pipeName = "\\\\.\\pipe\\ABPipe1") : PipeName(pipeName) {};

		void CreatePipe()
		{
			hPipe = CreateNamedPipeA
			(
				PipeName.c_str(), 
				PIPE_ACCESS_DUPLEX, 
				PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
				PIPE_UNLIMITED_INSTANCES,
				MAX_PACKET_SIZE, 
				MAX_PACKET_SIZE,
				0, 
				null
			);
			if (hPipe == INVALID_HANDLE_VALUE) throw exception("CreateNamedPipeA failed");
		}

		void ClosePipe()
		{
			DisconnectNamedPipe(hPipe);
			CloseHandle(hPipe);
		}

		void AwaitClient()
		{
			BOOL hasClientSuccessifullyConnected = ConnectNamedPipe(hPipe, null) ? true : (GetLastError() == ERROR_PIPE_CONNECTED);
			if (!hasClientSuccessifullyConnected) throw exception("ConnectNamedPipe failed");
		}

		void WritePipeRaw(void* data, uint64_t size)
		{
			if (!WriteFile(hPipe, data, size, null, null)) throw exception("WriteFile failed");
		}

		void ReadPipeRaw(void* data, uint64_t size = MAX_PACKET_SIZE)
		{
			if (!ReadFile(hPipe, data, size, null, null)) throw exception("ReadFile failed");
		}

		template <class T> void WritePipe(T& data)
		{
			WritePipeRaw(&data, sizeof(T));
		}

		template <class T> void	ReadPipe(T& data)
		{
			ReadPipeRaw(&buff, sizeof(T));
		}

		void Pong(ABStatus status = S_Success)
		{
			ABResponse res;
			res.Status = status;
			WritePipe(res);
		}

		void AccuireProcessAccess(uint64_t processId, ACCESS_MASK desiredAccess)
		{
			ABStatus s = S_Success;
			hProcess = Util::GetProcessHandle(processId);
			if (!hProcess)
			{
				pFl->Log("No handles available, opening the process..");
				hProcess = OpenProcess(desiredAccess, null, processId);
				pFl->Log("hProcess: 0x%X", hProcess);
				if (!hProcess)
				{
					s = S_OpenProcessFailed;
				}
				if (!Util::HandleHasAccess(hProcess, desiredAccess)) s = S_HandleHasBeenStripped;
			}
			Pong(s);
		}

		void RpmRaw(uint64_t address, uint64_t size)
		{
			auto sz = offsetof(ResRpm, Data) + size;
			byte* response = new byte[sz];
			if (!ReadProcessMemory(hProcess, (void*)address, &((ResRpm*)response)->Data, size, null))
			{
				((ABResponse*)response)->Status = S_Fail;
			}
			WritePipeRaw(response, sz);
			delete[] response;
		}

		void WpmRaw(uint64_t address, void* data, uint64_t size)
		{
			SIZE_T bw;

			if (!WriteProcessMemory(hProcess, (void*)address, data, size, &bw))
			{
				pFl->Log("WriteProcessMemory failed. GetLastError: 0x%X", GetLastError());
			}
			else
			{
				pFl->Log("WriteProcessMemory success, bytesWritten: 0x%X", bw);
			}
		}

		void VirtualAlloc(uint64_t address, uint64_t size, DWORD allocationType, DWORD protection)
		{
			auto allocatedAddr = VirtualAllocEx(hProcess, (void*)address, size, allocationType, protection);
			ResVa res;
			res.Status = allocatedAddr != nullptr ? S_Success : S_Fail;
			res.Address = (uint64_t)allocatedAddr;
			WritePipe(res);
		}

		void VirtualProtect(uint64_t address, uint64_t size, DWORD newProtection)
		{
			DWORD oldProtection;
			ResVp res;
			auto status = VirtualProtectEx(hProcess, (void*)address, size, newProtection, &oldProtection);
			res.Status = (ABStatus)(status);
			res.OldProtection = oldProtection;
			WritePipe(res);
		}

		void GetProcessModuleBase(string moduleName)
		{
			auto ret = Util::GetProcessModuleBase(hProcess, moduleName);
			ResModuleBase res;
			res.Status = ret ? S_Success : S_Fail;
			res.ModuleBase = ret;
			WritePipe(res);
		}
	};
}