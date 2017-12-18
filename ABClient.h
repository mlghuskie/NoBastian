#pragma once
#include "Include.h"
using namespace std;

namespace asmjs
{
	class ABClient
	{
		public:

		string PipeName;
		HANDLE hPipe;

		ABClient(string pipeName = "\\\\.\\pipe\\ABPipe1") : PipeName(pipeName) {};

		void Connect()
		{
			hPipe = CreateFileA
			(
				PipeName.c_str(),
				GENERIC_READ | GENERIC_WRITE,
				null,
				null,
				OPEN_EXISTING,
				null,
				null
			);
			if (hPipe == INVALID_HANDLE_VALUE) throw exception("Couldn't connect to pipe: CreateFileA failed");
		}

		void WritePipeRaw(void* data, uint64_t size)
		{
			if(!WriteFile(hPipe, data, size, null, null)) throw exception("Couldn't write data to pipe: WriteFile failed");
		}

		void ReadPipeRaw(void* data, uint64_t size)
		{
			if (!ReadFile(hPipe, data, size, null, null)) throw exception("Couldn't read data from pipe: ReadFile failed");
		}

		template <class T> void WritePipe(T& data)
		{
			WritePipeRaw(&data, sizeof(T));
		}

		template <class T> void ReadPipe(T& data)
		{
			ReadPipeRaw(&data, sizeof(T));
		}

		template <class T_In, class T_Out> void Request(T_In& in, T_Out& out)
		{
			WritePipe(in);
			ReadPipe(out);
		}

		void Ping()
		{
			ABRequest req = { C_Ping };
			ABResponse res;
			Request(req, res);
			if (!res.Status) throw exception("Pinging failed");
		}

		void UnloadModule()
		{
			ABRequest req = { C_UnloadModule };
			WritePipe(req);
		}

		void AccuireProcessHandle(uint64_t processId, ACCESS_MASK desiredAccess)
		{
			ReqAccuireProcessHandle req;
			req.Command = C_AccuireProcessHandle;
			req.DesiredAccess = desiredAccess;
			req.ProcessId = processId;
			ABResponse res;
			Request(req, res);
			if (!res.Status) throw exception("Couldn't accuire process handle");
		}

		void SetProcessHandle(HANDLE hProcess)
		{
			ReqSetProcessHandle req;
			req.Command = C_SetProcessHandle;
			req.hProcess = hProcess;
			ABResponse res;
			Request(req, res);
			if (!res.Status) throw exception("Couldn't set process handle");
		}

		uint64_t GetProcessModuleBase(string module)
		{
			auto sz = sizeof(ABRequest::Command) + module.length() + 0x1;
			byte* buff = new byte[sz];
			((ABRequest*)buff)->Command = C_GetModuleBase;
			memcpy(buff + sizeof(ABRequest::Command), module.c_str(), module.length() + 0x1);
			WritePipeRaw(buff, sz);
			delete[] buff;
			ResModuleBase res;
			ReadPipe(res);
			if (!res.Status) throw exception("Couldn't get module base failed");
			return res.ModuleBase;
		}

		void WpmRaw(uint64_t address, void* data, uint64_t size)
		{
			auto buffSize = offsetof(ReqWpm, Data) + size;
			byte* buff = new byte[buffSize];
			((ReqWpm*)buff)->Command = C_WPM;
			((ReqWpm*)buff)->Address = address;
			((ReqWpm*)buff)->DataSize = size;
			memcpy((&((ReqWpm*)buff)->Data), data, size);
			WritePipeRaw(buff, buffSize);
			delete[] buff;
		}

		void RpmRaw(uint64_t address, void* data, uint64_t size)
		{
			auto buffSize = offsetof(ResRpm, Data) + size;
			auto buff = new byte[buffSize];
			ReqRpm req;
			req.Command = C_RPM;
			req.Address = address;
			req.DataSize = size;
			WritePipe(req);
			ReadPipeRaw(buff, buffSize);
			memcpy(data, buff + offsetof(ResRpm, Data), size);
			delete[] buff;
		}

		DWORD VirtualProtect(uint64_t address, uint64_t size, DWORD protection)
		{
			ReqVp req;
			ResVp res;
			
			req.Command = C_VirtualProtect;
			req.Address = address;
			req.Size = size;
			req.NewProtection = protection;

			WritePipe(req);
			ReadPipe(res);
			if (!res.Status) throw exception("VirtualProtect failed");
			return res.OldProtection;
		}

		uint64_t VirtualAlloc(uint64_t address, uint64_t size, DWORD allocationType, DWORD protection)
		{
			ReqVa req;
			ResVa res;
			req.Command = C_VirtualAlloc;
			req.Address = address;
			req.Size = size;
			req.AllocationType = allocationType;
			req.Protection = protection;
			Request(req, res);
			if (!res.Status) throw exception("VirtualAlloc failed");
			return res.Address;
		}

		template <class T> void Wpm(uint64_t address, T& data)
		{
			WpmRaw(address, &data, sizeof(T));
		}

		template <class T> void Rpm(uint64_t address, T& data)
		{
			RpmRaw(address, &data, sizeof(T));
		}

		void Disconnect()
		{
			DisconnectNamedPipe(hPipe);
			CloseHandle(hPipe);
		}
	};
}