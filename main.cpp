#include <Windows.h>
#include <Psapi.h>
#include <TlHelp32.h>
#include <iostream>
#include <tchar.h>

#include "Resource.hpp"

#ifdef _USING_V110_SDK71_
#define WINDOWS_XP_BUILD
#endif // _USING_V110_SDK71_

#define FFLAGS_FALLING 128
#define FFLAGS_GROUND 129
#define FFLAGS_AIR 130
#define FFLAGS_GROUNDCROUCH 131
#define FFLAGS_WATER 641
#define FFLAGS_WATERCROUCH 643

LPCTSTR szWindowTitle = _T("Left 4 Dead 2");
HWND hWnd = NULL;
HINSTANCE hInstance = NULL;
HANDLE hConsoleOutput = NULL;
HANDLE hProcess = NULL;
HANDLE hThread = NULL;
DWORD dwPiD = NULL;
DWORD dwAddress = NULL;
DWORD dwClientBase = NULL;
DWORD dwBaseEntity = NULL;
bool bIsEnabled = false;

BOOL SetConsoleIcon(HICON hIcon)
{
	typedef BOOL(WINAPI* SetConsoleIconFn)(HICON);
	static SetConsoleIconFn fnSetConsoleIcon = NULL;
	if (!fnSetConsoleIcon)
		fnSetConsoleIcon = (SetConsoleIconFn)GetProcAddress(GetModuleHandle(_T("kernel32")), "SetConsoleIcon");
	if (!fnSetConsoleIcon)
		return FALSE;
	return fnSetConsoleIcon(hIcon);
}

LPCTSTR FormatError(DWORD dwStatus)
{
	LPTSTR lpBuffer = NULL;
	if (!FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, dwStatus, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), (LPTSTR)&lpBuffer, 0, NULL))
		wsprintf(lpBuffer, _T("Error Code: %lu"), dwStatus);
	return lpBuffer;
}

DWORD GetModuleSize(LPCTSTR szModuleName, DWORD dwProcessId)
{
	MODULEENTRY32 cbModule = { sizeof(MODULEENTRY32) };
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwProcessId);
	if (hSnapshot == INVALID_HANDLE_VALUE)
		return NULL;

	if (Module32First(hSnapshot, &cbModule))
		while (Module32Next(hSnapshot, &cbModule))
			if (!lstrcmp(cbModule.szModule, szModuleName))
				break;

	CloseHandle(hSnapshot);
	return cbModule.modBaseSize;
}

DWORD GetModuleBase(LPCTSTR szModuleName, DWORD dwProcessId)
{
	MODULEENTRY32 cbModule = { sizeof(MODULEENTRY32) };
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwProcessId);
	if (hSnapshot == INVALID_HANDLE_VALUE)
		return NULL;

	if (Module32First(hSnapshot, &cbModule))
		while (Module32Next(hSnapshot, &cbModule))
			if (!lstrcmp(cbModule.szModule, szModuleName))
				break;

	CloseHandle(hSnapshot);
	return (DWORD)cbModule.modBaseAddr;
}

DWORD GetProcessIdFromName(LPCTSTR szProcName)
{
	PROCESSENTRY32 cbProcess = { sizeof(PROCESSENTRY32) };
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE)
		return NULL;

	if (Process32First(hSnapshot, &cbProcess))
		while (Process32Next(hSnapshot, &cbProcess))
			if (!lstrcmp(cbProcess.szExeFile, szProcName))
				break;

	CloseHandle(hSnapshot);
	return cbProcess.th32ProcessID;
}

VOID ThreadProc()
{
	BYTE kState[256];
	int fFlags = 0;
	bool bIsJumped = false;
	bool bIsPressed = false;
	bool bIsCrouched = false;

	while (bIsEnabled)
	{
		ReadProcessMemory(hProcess, (PBYTE*)(dwClientBase + 0x7964FC), &dwBaseEntity, sizeof(DWORD), NULL);
		ReadProcessMemory(hProcess, (PBYTE*)(dwBaseEntity + 0xF0), &fFlags, sizeof(int), NULL); // 0xF0
		
		if ((GetAsyncKeyState(VK_SPACE) & 0x8000) || (GetAsyncKeyState(VK_LMENU) & 0x8000))
		{
			if (fFlags == FFLAGS_GROUND || fFlags == FFLAGS_GROUNDCROUCH || fFlags == FFLAGS_WATER || fFlags == FFLAGS_WATERCROUCH)
			{
				if ((GetAsyncKeyState(VK_LMENU) & 0x8000) && !bIsCrouched)
				{
					GetKeyboardState((PBYTE)&kState);
					kState[VK_LCONTROL] |= 0x80;
					SetKeyboardState((PBYTE)&kState);
					PostMessage(hWnd, WM_KEYDOWN, VK_LCONTROL, ((LPARAM)(29 << 16)) | 0x00000001);
					bIsCrouched = true;
				}
				SendMessage(hWnd, WM_KEYDOWN, VK_SPACE, 0x390000);
				bIsJumped = true;
				bIsPressed = true;
			}
			else if (fFlags == FFLAGS_AIR || fFlags == FFLAGS_FALLING)
			{
				SendMessage(hWnd, WM_KEYUP, VK_SPACE, 0x390000);
				/* SMAC Bypass */
				if (bIsJumped)
				{
					Sleep(16);
					SendMessage(hWnd, WM_KEYDOWN, VK_SPACE, 0x390000);
					SendMessage(hWnd, WM_KEYUP, VK_SPACE, 0x390000);
					bIsJumped = false;
				}
				bIsPressed = true;
			}
		}
		else if (bIsPressed)
		{
			if (bIsCrouched)
			{
				GetKeyboardState((PBYTE)&kState);
				kState[VK_LCONTROL] &= ~0x80;
				SetKeyboardState((PBYTE)&kState);
				PostMessage(hWnd, WM_KEYUP, VK_LCONTROL, ((LPARAM)(29 << 16)) | 0xC0000001);
				bIsCrouched = false;
			}
			if (bIsJumped)
				SendMessage(hWnd, WM_KEYUP, VK_SPACE, 0x390000);
			bIsPressed = false;
		}
		Sleep(10);
	}
}

int _tmain(int argc, _TCHAR* argv[], _TCHAR* envp[])
{
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	hWnd = GetConsoleWindow();
	hInstance = GetModuleHandle(NULL);
	hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);

#ifdef WINDOWS_XP_BUILD
	SetConsoleTitle(_T("Pluton for Windows XP"));
#else
	SetConsoleTitle(_T("Pluton for Windows"));
#endif
	SetConsoleIcon(LoadIcon(hInstance, MAKEINTRESOURCE(ICON_L4D2)));

#ifndef WINDOWS_XP_BUILD
	CONSOLE_FONT_INFOEX cbFontInfo = { 0 };
	GetCurrentConsoleFontEx(hConsoleOutput, FALSE, &cbFontInfo);
	cbFontInfo.cbSize = sizeof(CONSOLE_FONT_INFOEX);
	cbFontInfo.nFont = 9;
	cbFontInfo.dwFontSize.X = 10;
	cbFontInfo.dwFontSize.Y = 16;
	cbFontInfo.FontFamily = 54;
	cbFontInfo.FontWeight = 400;
	wcscpy_s(cbFontInfo.FaceName, L"Consolas");
	SetCurrentConsoleFontEx(hConsoleOutput, FALSE, &cbFontInfo);

	COORD cbSize = { 100, 30 };
	SetConsoleScreenBufferSize(hConsoleOutput, cbSize);
	SMALL_RECT cbWindowInfo = { 0, 0, 100 - 1, 30 - 1 }; // Window Size
	SetConsoleWindowInfo(hConsoleOutput, TRUE, &cbWindowInfo);
	RECT cbRect = { 0, 0, 0, 0 };
	GetWindowRect(hWnd, &cbRect);
	LONG dwWidth = (GetSystemMetrics(SM_CXSCREEN) - cbRect.right) / 2;
	LONG dwHeight = (GetSystemMetrics(SM_CYSCREEN) - cbRect.bottom) / 4;

	SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
	SetLayeredWindowAttributes(hWnd, 0, 220, LWA_ALPHA);
#endif
	
	SetConsoleTextAttribute(hConsoleOutput, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY);
	
	_tprintf(_T(" __________.__          __                 \n"));
	_tprintf(_T(" \\______   \\  |  __ ___/  |_  ____   ____  \n"));
	_tprintf(_T("  |     ___/  | |  |  \\   __\\/  _ \\ /    \\ \n"));
	_tprintf(_T("  |    |   |  |_|  |  /|  | (  <_> )   |  \\\n"));
	_tprintf(_T("  |____|   |____/____/ |__|  \\____/|___|  /\n"));
	_tprintf(_T("                                        \\/ \n\n"));

	_tprintf(_T("INSERT   : Activate Pluton.\n"));
	_tprintf(_T("NUMPAD 0 : Auto Bhop Disables.\n"));
	_tprintf(_T("NUMPAD 1 : Auto Bhop Enables.\n"));
	
	if (!(hWnd = FindWindow(NULL, szWindowTitle)))
	{
		SetConsoleTextAttribute(hConsoleOutput, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
		_tprintf(_T("\n[*] Awaiting Left 4 Dead 2 launch..."));
		while (!(hWnd = FindWindow(NULL, szWindowTitle)))
			Sleep(50);
	}
	
	SetConsoleTextAttribute(hConsoleOutput, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
	_tprintf(_T("\n[+] Left 4 Dead 2 found!\n"));

	while (!(dwPiD = GetProcessIdFromName(_T("left4dead2.exe"))))
		Sleep(50);
	while (!(hProcess = OpenProcess(SYNCHRONIZE | PROCESS_VM_READ, FALSE, dwPiD)))
		Sleep(50);
	while (!(dwClientBase = GetModuleBase(_T("client.dll"), dwPiD)))
		Sleep(50);

	SetConsoleTextAttribute(hConsoleOutput, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
	_tprintf(_T("[*] Awaiting Pluton's activation...\n"));
	while (!GetAsyncKeyState(VK_INSERT))
		Sleep(50);

	SetConsoleTextAttribute(hConsoleOutput, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
	_tprintf(_T("[+] Pluton activated!\n"));
	SetConsoleTextAttribute(hConsoleOutput, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY);
	Beep(0x367, 250);

	while (FindWindow(NULL, szWindowTitle))
	{
		if ((GetAsyncKeyState(VK_NUMPAD0) & 0x8000) && bIsEnabled)
		{
			bIsEnabled = false;
			hThread = NULL;
			SetConsoleTextAttribute(hConsoleOutput, FOREGROUND_RED | FOREGROUND_INTENSITY);
			_tprintf(_T("[-] Auto Bhop Disabled!\n"));
			SetConsoleTextAttribute(hConsoleOutput, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY);
			if (!Beep(0x255, 250))
				Sleep(250);
		}

		if ((GetAsyncKeyState(VK_NUMPAD1) & 0x8000) && !bIsEnabled)
		{
			bIsEnabled = true;
			hThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)ThreadProc, NULL, NULL, NULL);
			if (!hThread)
			{
				SetConsoleTextAttribute(hConsoleOutput, FOREGROUND_RED | FOREGROUND_INTENSITY);
				_tprintf(_T("[-] Error: %ls\n"), FormatError(GetLastError()));
				SetConsoleTextAttribute(hConsoleOutput, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY);
				break;
			}
			SetConsoleTextAttribute(hConsoleOutput, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
			_tprintf(_T("[+] Auto Bhop Enabled!\n"));
			SetConsoleTextAttribute(hConsoleOutput, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY);
			if (!Beep(0x367, 250))
				Sleep(250);
		}
		Sleep(50);
	}
	_tprintf(_T("\n"));
	_tsystem(_T("pause"));
	return EXIT_SUCCESS;
}
