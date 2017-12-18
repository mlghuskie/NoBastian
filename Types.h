#pragma once
#include "Include.h"

#define MAX_PACKET_SIZE 0x10000

namespace asmjs
{
	enum ABCommand : uint64_t
	{
		C_Invalid = 0x0,
		C_Ping = 0x1,
		C_AccuireProcessHandle = 0x2,
		C_RPM = 0x3,
		C_WPM = 0x4,
		C_VirtualAlloc = 0x5,
		C_SetProcessHandle = 0x6,
		C_GetModuleBase = 0x7,
		C_UnloadModule = 0x8,
		C_VirtualProtect = 0x9
	};

	enum ABStatus : signed char
	{
		S_Success = 0x1,
		S_Fail = 0x0,
		S_OpenProcessFailed = -0x1,
		S_HandleHasBeenStripped = -0x2
	};

	struct ABRequest
	{
		ABCommand Command;
	};

	struct ABResponse
	{
		ABStatus Status;
	};

	struct ReqSetProcessHandle : ABRequest
	{
		HANDLE hProcess;
	};

	struct ReqAccuireProcessHandle : ABRequest
	{
		uint64_t ProcessId;
		ACCESS_MASK DesiredAccess;
	};

	struct ResModuleBase : ABResponse
	{
		uint64_t ModuleBase;
	};

	struct ReqRpm : ABRequest
	{
		uint64_t Address;
		uint64_t DataSize;
	};

	struct ResRpm : ABResponse
	{
		byte Data[];
	};

	struct ReqWpm : ABRequest
	{
		uint64_t Address;
		uint64_t DataSize;
		byte Data[];
	};

	struct ReqVp : ABRequest
	{
		uint64_t Address;
		uint64_t Size;
		DWORD NewProtection;
	};

	struct ResVp : ABResponse
	{
		DWORD OldProtection;
	};

	struct ReqVa : ABRequest
	{
		uint64_t Address;
		uint64_t Size;
		DWORD AllocationType;
		DWORD Protection;
	};

	struct ResVa : ABResponse
	{
		uint64_t Address;
	};
}