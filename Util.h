#include "Include.h"

#undef Module32First;
#undef Module32Next
#undef MODULEENTRY32
#undef Process32First
#undef Process32Next
#undef PROCESSENTRY32

#define A(s) Util::ToA(s)
#define W(s) Util::ToW(s)
#define LC(s) Util::ToLower(s)

using namespace std;

namespace asmjs
{
	class Util
	{
		public:

		static wstring ToW(string str)
		{
			wstring_convert<codecvt_utf8_utf16<wchar_t>> converter;
			return converter.from_bytes(str);
		}

		static string ToA(wstring wstr)
		{
			wstring_convert<codecvt_utf8_utf16<wchar_t>> converter;
			return converter.to_bytes(wstr);
		}

		static string ToLower(string str)
		{
			transform(str.begin(), str.end(), str.begin(), tolower);
			return str;
		}

		static wstring ToLower(wstring str)
		{
			transform(str.begin(), str.end(), str.begin(), tolower);
			return str;
		}

		static vector<uint64_t> GetProcessIdsByName(string name)
		{
			vector<uint64_t> res;
			HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, null);
			if(!snap) throw exception("CreateToolhelp32Snapshot failed");
			PROCESSENTRY32 entry;
			entry.dwSize = sizeof(entry);
			if (!Process32First(snap, &entry)) throw exception("Process32First failed");
			do
			{
				if (LC(entry.szExeFile) == LC(name))
				{
					res.push_back(entry.th32ProcessID);
				}
			} while (Process32Next(snap, &entry));
			CloseHandle(snap);
			return res;
		}

		static uint64_t GetProcessModuleBase(HANDLE hProcess, string moduleName)
		{
			return GetProcessModules(hProcess)[LC(moduleName)];
		}

		// Requires PROCESS_QUERY_INFORMATION access rights

		static uint64_t TlHelp32GetProcessModuleBase(uint64_t processId, string moduleName)
		{
			HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId);
			if (!hSnap) return false;
			uint64_t ret = 0;
			MODULEENTRY32 modEntry;
			modEntry.dwSize = sizeof(MODULEENTRY32);
			if (!Module32First(hSnap, &modEntry)) return false;
			do
			{
				if (LC(modEntry.szModule) == LC(moduleName))
				{
					ret = (uint64_t)modEntry.modBaseAddr;
					break;
				}
			} while (Module32Next(hSnap, &modEntry));
			CloseHandle(hSnap);
			return ret;
		}

		static DWORD GetModuleHandleArraySize(HANDLE hProcess)
		{
			DWORD szNeeded;
			auto res = K32EnumProcessModules(hProcess, null, null, &szNeeded);
			if (!res) throw exception("K32EnumProcessModules failed");
			return szNeeded;
		}

		static map<string, uint64_t> GetProcessModules(HANDLE hProcess)
		{
			map<string, uint64_t> res;
			if (DWORD szNeeded = GetModuleHandleArraySize(hProcess))
			{
				vector<uint64_t> modHandles;
				modHandles.resize(szNeeded);
				if (K32EnumProcessModules(hProcess, (HMODULE*)modHandles.data(), szNeeded, &szNeeded))
				{
					for (int i = 0; i != (szNeeded / sizeof(HMODULE)); i++)
					{
						char fn[1024];
						if (K32GetModuleBaseNameA(hProcess, (HMODULE)modHandles[i], fn, sizeof(fn)))
						{
							res[LC(fn)] = modHandles[i];
						}
					}
				}
				else
				{
					throw exception("K32EnumProcessModules failed");
				}
			}
			return res;
		}

		static ACCESS_MASK QueryHandleAccessMask(HANDLE handle)
		{
			typedef NTSTATUS (*NtQueryObjectFn)(
				_In_opt_  HANDLE                   Handle,
				_In_      OBJECT_INFORMATION_CLASS ObjectInformationClass,
				_Out_opt_ PVOID                    ObjectInformation,
				_In_      ULONG                    ObjectInformationLength,
				_Out_opt_ PULONG                   ReturnLength
			);

			auto NtQueryObject = (NtQueryObjectFn)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryObject");
			if (!NtQueryObject) throw exception("Couldn't get address of NtQueryObject");

			PUBLIC_OBJECT_BASIC_INFORMATION objInfo;
			if (!NT_SUCCESS(NtQueryObject(handle, ObjectBasicInformation, &objInfo, sizeof(objInfo), null))) throw exception("NtQueryObject failed");
			return objInfo.GrantedAccess;
		}

		static bool HandleHasAccess(HANDLE handle, ACCESS_MASK mask)
		{
			return (QueryHandleAccessMask(handle) & mask) == mask;
		}

		#define STATUS_INFO_LENGTH_MISMATCH 0xC0000004
		#define SystemHandleInformation 16

		typedef NTSTATUS(NTAPI* NtQuerySystemInformationFn)(ULONG, PVOID, ULONG, PULONG);

		typedef struct _SYSTEM_HANDLE
		{
			ULONG ProcessId;
			BYTE ObjectTypeNumber;
			BYTE Flags;
			USHORT Handle;
			PVOID Object;
			ACCESS_MASK GrantedAccess;
		} SYSTEM_HANDLE, *PSYSTEM_HANDLE;

		typedef struct _SYSTEM_HANDLE_INFORMATION
		{
			ULONG HandleCount;
			SYSTEM_HANDLE Handles[1];
		} SYSTEM_HANDLE_INFORMATION, *PSYSTEM_HANDLE_INFORMATION;

		// implementation 1, works well enough

		static HANDLE GetProcessHandle(uint64_t targetProcessId)
		{
			auto NtQuerySystemInformation = reinterpret_cast<NtQuerySystemInformationFn>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQuerySystemInformation"));
			NTSTATUS status;
			ULONG handleInfoSize = 0x10000;

			auto handleInfo = reinterpret_cast<PSYSTEM_HANDLE_INFORMATION>(malloc(handleInfoSize));

			while ((status = NtQuerySystemInformation(SystemHandleInformation, handleInfo, handleInfoSize, nullptr)) == STATUS_INFO_LENGTH_MISMATCH)
				handleInfo = reinterpret_cast<PSYSTEM_HANDLE_INFORMATION>(realloc(handleInfo, handleInfoSize *= 2));

			if (!NT_SUCCESS(status))
			{
				throw runtime_error("NtQuerySystemInformation failed!");
			}

			for (auto i = 0; i < handleInfo->HandleCount; i++)
			{
				auto handle = handleInfo->Handles[i];

				const auto process = reinterpret_cast<HANDLE>(handle.Handle);
				if (handle.ProcessId == GetCurrentProcessId() && GetProcessId(process) == targetProcessId)
					return process;
			}

			free(handleInfo);

			return nullptr;
		}

		// implmentation 2, crashes sometimes by some reason, so i don't use it

		static vector<HANDLE> QueryProcessHandles(uint64_t targetProcessId, ACCESS_MASK desiredAccess)
		{
			#define STATUS_UNSUCCESSFUL 0xC0000001
			#define SystemExtendedHandleInformation 64
			#define TI_Process 7

			typedef struct SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX
			{
				PVOID Object;
				HANDLE UniqueProcessId;
				HANDLE HandleValue;
				ACCESS_MASK GrantedAccess;
				USHORT CreatorBackTraceIndex;
				USHORT ObjectTypeIndex;
				ULONG HandleAttributes;
				ULONG Reserved;
			};

			typedef struct SYSTEM_HANDLE_INFORMATION_EX
			{
				ULONG_PTR NumberOfHandles;
				ULONG_PTR Reserved;
				SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX Handles[1];
			};

			auto NtQuerySystemInformation = (NtQuerySystemInformationFn)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQuerySystemInformation");
			if (!NtQuerySystemInformation) throw exception("Coudln't resolve NtQuerySystemInformation address");

			vector<HANDLE> res;
			ULONG size = 0;
			SYSTEM_HANDLE_INFORMATION_EX* handleInfo = nullptr;
			NTSTATUS status = -1;

			while (!NT_SUCCESS(status))
			{
				status = NtQuerySystemInformation((SYSTEM_INFORMATION_CLASS)SystemExtendedHandleInformation, handleInfo, size, &size);
				if (status == STATUS_INFO_LENGTH_MISMATCH)
				{
					if (handleInfo != nullptr) VirtualFree(handleInfo, 0, MEM_RELEASE);
					handleInfo = (SYSTEM_HANDLE_INFORMATION_EX*)VirtualAlloc(null, size, MEM_COMMIT, PAGE_READWRITE);
				}
			}

			for (ULONG i = 0; i < handleInfo->NumberOfHandles; i++)
			{
				auto handleEntry = &handleInfo->Handles[i];
				if (handleEntry->ObjectTypeIndex == TI_Process					 // Is it a process handle or not 
					&& GetProcessId(handleEntry->HandleValue) == targetProcessId // Does it describe target process or describes something else
					&& HandleHasAccess(handleEntry->HandleValue, desiredAccess))  // Does it have enough access rights
				{
					res.push_back(handleEntry->HandleValue);
				}
			}

			VirtualFree(handleInfo, 0, MEM_RELEASE);
			return res;
		}
	};
}