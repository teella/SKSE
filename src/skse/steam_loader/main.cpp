#include <string>
#include <direct.h>
#include "skse/skse_version.h"
#include "skse/SafeWrite.h"
#include "common/IFileStream.h"
#include "loader_common/IdentifyEXE.h"
#include <tlhelp32.h>

// hack for VS2005, intrin.h can't be included with winnt.h
#if _MSC_VER <= 1400
#define _interlockedbittestandreset _interlockedbittestandreset_NAME_CHANGED_TO_AVOID_MSVC2005_ERROR
#define _interlockedbittestandset _interlockedbittestandset_NAME_CHANGED_TO_AVOID_MSVC2005_ERROR
#endif

#include <intrin.h>

IDebugLog	gLog("skse_steam_loader.log");
HANDLE		g_dllHandle;

static void OnAttach(void);

std::string GetCWD(void)
{
	char	cwd[4096];

	ASSERT(_getcwd(cwd, sizeof(cwd)));

	return cwd;
}

std::string GetAppPath(void)
{
	char	appPath[4096];

	ASSERT(GetModuleFileName(GetModuleHandle(NULL), appPath, sizeof(appPath)));

	return appPath;
}

BOOL WINAPI DllMain(HANDLE procHandle, DWORD reason, LPVOID reserved)
{
	if(reason == DLL_PROCESS_ATTACH)
	{
		g_dllHandle = procHandle;

		OnAttach();
	}

	return TRUE;
}

typedef int (__stdcall * _HookedWinMain)(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);
_HookedWinMain g_hookedWinMain = NULL;
std::string g_dllPath;

int __stdcall OnHook(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	_MESSAGE("OnHook: thread = %d", GetCurrentThreadId());

	// load main dll
	if(!LoadLibrary(g_dllPath.c_str()))
	{
		_ERROR("couldn't load dll (%08X)", GetLastError());
	}

	// call original winmain
	_MESSAGE("calling winmain");

	int result = g_hookedWinMain(hInstance, hPrevInstance, lpCmdLine, nCmdShow);

	_MESSAGE("returned from winmain (%d)", result);

	return result;
}

void DumpThreads(void)
{
	HANDLE	snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, GetCurrentProcessId());
	if(snapshot != INVALID_HANDLE_VALUE)
	{
		THREADENTRY32	info;

		info.dwSize = sizeof(info);

		if(Thread32First(snapshot, &info))
		{
			do 
			{
				if(info.th32OwnerProcessID == GetCurrentProcessId())
				{
					UInt32	eip = 0xFFFFFFFF;

					HANDLE	thread = OpenThread(THREAD_GET_CONTEXT, FALSE, info.th32ThreadID);
					if(thread)
					{
						CONTEXT	ctx;

						ctx.ContextFlags = CONTEXT_CONTROL;

						GetThreadContext(thread, &ctx);

						eip = ctx.Eip;

						CloseHandle(thread);
					}

					_MESSAGE("thread %d pri %d eip %08X%s",
						info.th32ThreadID,
						info.tpBasePri,
						eip,
						(info.th32ThreadID == GetCurrentThreadId()) ? " current thread" : "");
				}

				info.dwSize = sizeof(info);
			}
			while(Thread32Next(snapshot, &info));
		}

		CloseHandle(snapshot);
	}
}

bool hookInstalled = false;

void InstallHook(void * retaddr)
{
	if(hookInstalled)
		return;
	else
		hookInstalled = true;

	_MESSAGE("InstallHook: thread = %d retaddr = %08X", GetCurrentThreadId(), retaddr);

//	DumpThreads();

	std::string appPath = GetAppPath();
	_MESSAGE("appPath = %s", appPath.c_str());

	std::string		dllSuffix;
	ProcHookInfo	procHookInfo;

	if(!IdentifyEXE(appPath.c_str(), false, &dllSuffix, &procHookInfo))
	{
		_ERROR("unknown exe");
		return;
	}

	// build full path to our dll
	std::string	dllPath;

	g_dllPath = GetCWD() + "\\skse_" + dllSuffix + ".dll";

	_MESSAGE("dll = %s", g_dllPath.c_str());

	// hook winmain call
	UInt32	hookBaseAddr = procHookInfo.hookCallAddr;
	UInt32	hookBaseCallAddr = *((UInt32 *)(hookBaseAddr + 1));

	hookBaseCallAddr += 5 + hookBaseAddr;	// adjust for relcall

	_MESSAGE("old winmain = %08X", hookBaseCallAddr);

	g_hookedWinMain = (_HookedWinMain)hookBaseCallAddr;

	UInt32	newHookDst = ((UInt32)OnHook) - hookBaseAddr - 5;

	SafeWrite32(hookBaseAddr + 1, newHookDst);

	FlushInstructionCache(GetCurrentProcess(), NULL, 0);
}

typedef void (__stdcall * _GetSystemTimeAsFileTime)(LPFILETIME * fileTime);

_GetSystemTimeAsFileTime GetSystemTimeAsFileTime_Original = NULL;
_GetSystemTimeAsFileTime * _GetSystemTimeAsFileTime_IAT = NULL;

void __stdcall GetSystemTimeAsFileTime_Hook(LPFILETIME * fileTime)
{
	InstallHook(_ReturnAddress());

	GetSystemTimeAsFileTime_Original(fileTime);
}

void * GetIATAddr(UInt8 * base, const char * searchDllName, const char * searchImportName)
{
	IMAGE_DOS_HEADER		* dosHeader = (IMAGE_DOS_HEADER *)base;
	IMAGE_NT_HEADERS		* ntHeader = (IMAGE_NT_HEADERS *)(base + dosHeader->e_lfanew);
	IMAGE_IMPORT_DESCRIPTOR	* importTable =
		(IMAGE_IMPORT_DESCRIPTOR *)(base + ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

	for(; importTable->Characteristics; ++importTable)
	{
		const char	* dllName = (const char *)(base + importTable->Name);

		if(!_stricmp(dllName, searchDllName))
		{
			// found the dll

			IMAGE_THUNK_DATA	* thunkData = (IMAGE_THUNK_DATA *)(base + importTable->OriginalFirstThunk);
			UInt32				* iat = (UInt32 *)(base + importTable->FirstThunk);

			for(; thunkData->u1.Ordinal; ++thunkData, ++iat)
			{
				if(!IMAGE_SNAP_BY_ORDINAL(thunkData->u1.Ordinal))
				{
					IMAGE_IMPORT_BY_NAME	* importInfo = (IMAGE_IMPORT_BY_NAME *)(base + thunkData->u1.AddressOfData);

					if(!_stricmp((char *)importInfo->Name, searchImportName))
					{
						// found the import
						return iat;
					}
				}
			}

			return NULL;
		}
	}

	return NULL;
}

static void OnAttach(void)
{
	gLog.SetPrintLevel(IDebugLog::kLevel_Error);
	gLog.SetLogLevel(IDebugLog::kLevel_DebugMessage);

	FILETIME	now;
	GetSystemTimeAsFileTime(&now);

	_MESSAGE("skse loader %08X (steam) %08X%08X", PACKED_SKSE_VERSION, now.dwHighDateTime, now.dwLowDateTime);
	_MESSAGE("base addr = %08X", g_dllHandle);

	UInt32	oldProtect;

	_GetSystemTimeAsFileTime_IAT = (_GetSystemTimeAsFileTime *)GetIATAddr((UInt8 *)GetModuleHandle(NULL), "kernel32.dll", "GetSystemTimeAsFileTime");
	if(_GetSystemTimeAsFileTime_IAT)
	{
		_MESSAGE("GetSystemTimeAsFileTime IAT = %08X", _GetSystemTimeAsFileTime_IAT);

		VirtualProtect((void *)_GetSystemTimeAsFileTime_IAT, 4, PAGE_EXECUTE_READWRITE, &oldProtect);

		_MESSAGE("original GetSystemTimeAsFileTime = %08X", *_GetSystemTimeAsFileTime_IAT);
		GetSystemTimeAsFileTime_Original = *_GetSystemTimeAsFileTime_IAT;
		*_GetSystemTimeAsFileTime_IAT = GetSystemTimeAsFileTime_Hook;
		_MESSAGE("patched GetSystemTimeAsFileTime = %08X", *_GetSystemTimeAsFileTime_IAT);

		UInt32 junk;
		VirtualProtect((void *)_GetSystemTimeAsFileTime_IAT, 4, oldProtect, &junk);
	}
	else
	{
		_ERROR("couldn't read IAT");
	}
}
