#include "Include.h"
#include "ABClient.h"
#include "Util.h"
using namespace std;
using namespace asmjs;

ABClient abc;

void main()
{
	try
	{
		// bypass test

		printf("Testing...\n");
		printf("Connecting to server..\n");
		abc.Connect();
		printf("Connected. Pinging...\n");
		abc.Ping();
		string pn, modname, textToWrite;
		printf("Enter process name to test on\n");
		cin >> pn;
		abc.AccuireProcessHandle(Util::GetProcessIdsByName(pn)[0], 0x1478 /* LSASS set */);
		printf("Success, enter modname\n");
		cin >> modname;
		printf("Enter text to replace module base with\n");
		cin >> textToWrite;
		auto base = abc.GetProcessModuleBase(modname);								// getting module base
		printf("Module base: 0x%X\n", base);
		char textAtBase[3] = "  ";													// zero terminating the buffer
		printf("Reading text at base\n");
		abc.RpmRaw(base, textAtBase, 2);											// reading two characters from processbase (should be MZ)
		printf("TextAtBase: %s\n", textAtBase);										// displaying them
		auto oldProtection = abc.VirtualProtect(base, 2, PAGE_EXECUTE_READWRITE);	// unprotecting process base
		printf("OldProtection: 0x%X\n", oldProtection);								// displaying old protection
		printf("Writing entered value to module base\n");
		abc.WpmRaw(base, (void*)textToWrite.c_str(), 2);							// writing a new value to module base
		abc.RpmRaw(base, textAtBase, 2);											// reading the process base to see if we wrote memory correctly last time
		printf("Reading text at base: %s\n", textAtBase);							// displaying em
		printf("Allocating some data..\n");								
		auto p = abc.VirtualAlloc(null, 0x1000000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE); // allocating some memory
		printf("Allocated region: 0x%X", p);
	}
	catch (exception e)
	{
		printf("Exception occured: %s (0x%X)\n", e.what(), GetLastError());
	}

	getchar();
	getchar();
	getchar();
	getchar();
}