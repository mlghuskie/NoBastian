#include "Include.h"
#include "ABServer.h"
#include "FileLogger.h"
using namespace asmjs;

FileLogger fl("C:\\ABLog.txt");
ABServer s;
HINSTANCE hThisModule;

DWORD MainThread(void*)
{
	fl.Enable = true;
	fl.Prefix = "[AB] ";
	fl.Postfix = "\n";
	fl.OpenOutput();
	fl.Log("Launching ABServer ..");
	s.pFl = &fl;

	try
	{
		while (true)
		{
			fl.Log("Creating pipe");
			s.CreatePipe();
			fl.Log("Pipe has been created, awaiting client");
			s.AwaitClient();
			fl.Log("Client has connected, awaiting requests");
			try
			{
				byte data[MAX_PACKET_SIZE];
				while (true)
				{
					s.ReadPipeRaw(data);
					auto cmd = ((ABRequest*)(data))->Command;
					fl.Log("Acepted request: r->Command: 0x%X", cmd);
					ReqAccuireProcessHandle* r1;
					ReqSetProcessHandle* r2;
					ReqRpm* r3;
					ReqWpm* r4;
					ReqVp* r5;
					ReqVa* r6;
					switch (cmd)
					{
						case C_Ping:
							fl.Log("Pinging back");
							s.Pong();
							break;

						case C_AccuireProcessHandle:
							r1 = (ReqAccuireProcessHandle*)data;
							fl.Log("Trying to accuire process handle with access 0x%X for process#%d", r1->DesiredAccess, r1->ProcessId);
							s.AccuireProcessAccess(r1->ProcessId, r1->DesiredAccess);
							break;

						case C_SetProcessHandle:
							r2 = (ReqSetProcessHandle*)data;
							fl.Log("Setting process handle: 0x%X", r2->hProcess);
							s.hProcess = r2->hProcess;
							s.Pong();
							break;

						case C_RPM:
							r3 = (ReqRpm*)data;
							fl.Log("Reading memory location: Address: 0x%X, Sz: 0x%X", r3->Address, r3->DataSize);
							s.RpmRaw(r3->Address, r3->DataSize);
							break;

						case C_WPM:
							r4 = (ReqWpm*)data;
							fl.Log("Writing memory location: Address: 0x%X, Sz: 0x%X", r4->Address, r4->DataSize);
							s.WpmRaw(r4->Address, (&((ReqWpm*)data)->Data), r4->DataSize);
							break;

						case C_VirtualProtect:
							r5 = (ReqVp*)data;
							fl.Log("Setting protection: Address: 0x%X, Sz: 0x%X, NewProtection: 0x%X", r5->Address, r5->Size, r5->NewProtection);
							s.VirtualProtect(r5->Address, r5->Size, r5->NewProtection);
							break;

						case C_VirtualAlloc:
							r6 = (ReqVa*)data;
							fl.Log("Allocating 0x%X bytes at 0x%X (AllocationType: 0x%X, Protection: 0x%X)", r6->Size, r6->Address, r6->AllocationType, r6->Protection);
							s.VirtualAlloc(r6->Address, r6->Size, r6->AllocationType, r6->Protection);
							break;

						case C_UnloadModule:
							fl.Log("Unloading ...");
							FreeLibrary(hThisModule);
							break;

						case C_GetModuleBase:
							string moduleName(((char*)(data) + sizeof(ABRequest::Command)));
							fl.Log("Fetching module information for %s", moduleName.c_str());
							s.GetProcessModuleBase(moduleName);
							break;
					}
				}
			}
			catch (exception e)
			{
				if (GetLastError() == ERROR_BROKEN_PIPE)
				{
					fl.Log("Client has closed the connection or pipe got broken");
				}
				else
				{
					fl.Log("Exception occured: %s (0x%X)", e.what(), GetLastError());
				}
				fl.Log("Closing pipe..");
				s.ClosePipe();
			}
		}
	}
	catch (exception e)
	{
		fl.Log("Fatal exception occured: %s (0x%X)", e.what(), GetLastError());
	};
	return 0;
}

BOOL WINAPI DllMain(HINSTANCE hModule, DWORD callReason, LPVOID reserved)
{
	static HANDLE hThread;
	hThisModule = hModule;
	if (callReason == DLL_PROCESS_ATTACH)
	{
		hThread = CreateThread(0, 0, MainThread, 0, 0, 0);
	}
	if (callReason == DLL_PROCESS_DETACH)
	{
		if (hThread)
		{
			TerminateThread(hThread, 0);
			s.ClosePipe();
			fl.Log("Server module is getting unloaded, cya");
		}
	}
	return TRUE;
}