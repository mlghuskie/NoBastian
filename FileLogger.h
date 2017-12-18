#pragma once
#include "Include.h"
using namespace std;

namespace asmjs
{
	class FileLogger
	{
		string LogFilePath;

		public:

		bool Enable;
		HANDLE hLogFile;
		string Prefix, Postfix;

		FileLogger(string logFilePath) : LogFilePath(logFilePath) {}

		void OpenOutput()
		{
			hLogFile = CreateFileA
			(
				LogFilePath.c_str(),
				GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				null, 
				CREATE_ALWAYS,
				FILE_ATTRIBUTE_NORMAL,
				null
			);
			if (hLogFile == INVALID_HANDLE_VALUE) throw exception("Couldn't open log file.");
		}

		void Log(const char* format...)
		{
			if (Enable)
			{
				char buff[1024];
				va_list va_alist;
				va_start(va_alist, format);
				vsprintf_s(buff, format, va_alist);
				va_end(va_alist);
				string newbuff;
				newbuff = Prefix + buff + Postfix;
				if (!WriteFile(hLogFile, newbuff.c_str(), newbuff.size(), null, null)) throw exception("WriteFile failed");
			}
		}
	};
}