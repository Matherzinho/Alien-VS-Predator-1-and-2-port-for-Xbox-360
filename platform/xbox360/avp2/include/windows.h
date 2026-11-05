/* avp360: o pedaco da API Win32 que o LithTech 2.2 usa, para compilar no Xbox 360 (xsdk/newlib).
 * Tipos e macros aqui; as funcoes estao em win32_xbox.cpp. */
#ifndef AVP360_WINDOWS_H
#define AVP360_WINDOWS_H
/* O windows.h de verdade define _WINDOWS_; o mfcstub do LithTech so declara POINT/RECT sem ele. */
#define _WINDOWS_

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WINAPI
#define APIENTRY
#define CALLBACK
#define PASCAL
#define FAR
#define NEAR
#define IN
#define OUT
#define CONST const
#define VOID void
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#ifndef NULL
#define NULL 0
#endif

typedef int BOOL;
typedef unsigned char BYTE, UCHAR, BOOLEAN;
typedef unsigned short WORD, USHORT, WCHAR;
/* Como no Win32 (e no SDK do LithTech, ltbasetypes.h): DWORD e unsigned long, que no ABI de 32 bits do Xbox 360
 * tem 4 bytes, igual ao x86. */
typedef unsigned long DWORD, ULONG;
typedef long LONG;
typedef uint32_t UINT32;
typedef int32_t INT32;
/* O LithTech assume long e ponteiros de 32 bits. */
typedef char avp360_assert_ilp32[(sizeof(long) == 4 && sizeof(void *) == 4) ? 1 : -1];
typedef int INT;
typedef unsigned int UINT;
typedef short SHORT;
typedef char CHAR, TCHAR;
typedef float FLOAT;
typedef int64_t LONGLONG, __int64_t_ms;
typedef uint64_t ULONGLONG, DWORD64, QWORD;
typedef intptr_t INT_PTR, LONG_PTR;
typedef uintptr_t UINT_PTR, ULONG_PTR, DWORD_PTR, SIZE_T;
/* Codigo como o stdlithdefs.h testa "#ifndef BYTE" antes de definir BYTE como macro. */
#define BYTE BYTE
#define WORD WORD
#define DWORD DWORD
typedef void *PVOID, *LPVOID, *HANDLE, *HINSTANCE, *HMODULE, *HWND, *HDC, *HKEY, *HMENU, *HICON, *HCURSOR,
	*HBRUSH, *HFONT, *HBITMAP, *HGDIOBJ, *HGLOBAL, *HPALETTE, *HMONITOR, *HRGN, *HPEN, *FARPROC, *HACCEL;
typedef const void *LPCVOID;
typedef char *LPSTR, *PSTR, *LPTSTR;
typedef const char *LPCSTR, *PCSTR, *LPCTSTR;
typedef WCHAR *LPWSTR;
typedef const WCHAR *LPCWSTR;
typedef BYTE *LPBYTE, *PBYTE;
typedef DWORD *LPDWORD, *PDWORD;
typedef WORD *LPWORD;
typedef LONG *LPLONG, *PLONG;
typedef BOOL *LPBOOL;
typedef int *LPINT;
typedef UINT_PTR WPARAM;
typedef LONG_PTR LPARAM, LRESULT;
typedef LONG HRESULT;
typedef DWORD COLORREF;
typedef WORD ATOM;

#define MAX_PATH 260
#define _MAX_PATH 260
#define _MAX_FNAME 256
#define _MAX_EXT 256
#define _MAX_DIR 256
#define _MAX_DRIVE 3
#define INFINITE 0xFFFFFFFFu
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define WAIT_OBJECT_0 0
#define WAIT_TIMEOUT 258
#define S_OK ((HRESULT)0)
#define S_FALSE ((HRESULT)1)
#define E_FAIL ((HRESULT)0x80004005)
#define E_OUTOFMEMORY ((HRESULT)0x8007000E)
#define E_NOTIMPL ((HRESULT)0x80004001)
#define E_INVALIDARG ((HRESULT)0x80070057)
#define E_NOINTERFACE ((HRESULT)0x80004002)
#define SUCCEEDED(h) ((HRESULT)(h) >= 0)
#define FAILED(h) ((HRESULT)(h) < 0)
#define MAKEWORD(a, b) ((WORD)(((BYTE)(a)) | ((WORD)((BYTE)(b))) << 8))
#define MAKELONG(a, b) ((LONG)(((WORD)(a)) | ((DWORD)((WORD)(b))) << 16))
#define LOWORD(l) ((WORD)((DWORD_PTR)(l) & 0xffff))
#define HIWORD(l) ((WORD)((DWORD_PTR)(l) >> 16))
#define LOBYTE(w) ((BYTE)((DWORD_PTR)(w) & 0xff))
#define HIBYTE(w) ((BYTE)((DWORD_PTR)(w) >> 8))
#define RGB(r, g, b) ((COLORREF)(((BYTE)(r) | ((WORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16)))
#define ZeroMemory(p, n) memset((p), 0, (n))
#define FillMemory(p, n, v) memset((p), (v), (n))
#define CopyMemory(d, s, n) memcpy((d), (s), (n))
#define MoveMemory(d, s, n) memmove((d), (s), (n))
/* min/max: macros em C; em C++ as do <algorithm> (macros quebram a STL do GCC). */
#ifndef __cplusplus
#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif
#endif
#define UNREFERENCED_PARAMETER(p) (void)(p)
#define _T(x) x
#define TEXT(x) x
#define OutputDebugStringA OutputDebugString

/* Com os nomes de tag do Win32: o jogo e o SDK repetem alguns destes typedefs (typedef struct tagPOINT POINT). */
typedef struct tagRECT { LONG left, top, right, bottom; } RECT, *LPRECT, *PRECT;
typedef const RECT *LPCRECT;
typedef struct tagPOINT { LONG x, y; } POINT, *LPPOINT;
typedef struct tagSIZE { LONG cx, cy; } SIZE, *LPSIZE;
/* Big-endian: a metade alta vem primeiro na memoria. */
typedef union _LARGE_INTEGER { struct { LONG HighPart; DWORD LowPart; }; struct { LONG HighPart; DWORD LowPart; } u; LONGLONG QuadPart; } LARGE_INTEGER, *PLARGE_INTEGER;
typedef union _ULARGE_INTEGER { struct { DWORD LowPart; DWORD HighPart; } u; ULONGLONG QuadPart; } ULARGE_INTEGER;
typedef struct _FILETIME { DWORD dwLowDateTime, dwHighDateTime; } FILETIME, *LPFILETIME;
typedef struct _SYSTEMTIME { WORD wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds; } SYSTEMTIME, *LPSYSTEMTIME;
typedef struct _GUID { DWORD Data1; WORD Data2, Data3; BYTE Data4[8]; } GUID, IID, CLSID;
typedef const GUID &REFGUID_;
#ifdef __cplusplus
typedef const GUID &REFGUID;
typedef const IID &REFIID;
typedef const CLSID &REFCLSID;
#else
typedef const GUID *REFGUID;
typedef const IID *REFIID;
typedef const CLSID *REFCLSID;
#endif
typedef struct { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; POINT pt; } MSG, *LPMSG;
typedef struct { void *opaque[6]; } CRITICAL_SECTION, *LPCRITICAL_SECTION;
typedef struct { DWORD nLength; LPVOID lpSecurityDescriptor; BOOL bInheritHandle; } SECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;
typedef struct {
	DWORD dwFileAttributes; FILETIME ftCreationTime, ftLastAccessTime, ftLastWriteTime;
	DWORD nFileSizeHigh, nFileSizeLow, dwReserved0, dwReserved1; CHAR cFileName[MAX_PATH]; CHAR cAlternateFileName[14];
} WIN32_FIND_DATA, *LPWIN32_FIND_DATA, WIN32_FIND_DATAA;
typedef struct { DWORD dwOSVersionInfoSize, dwMajorVersion, dwMinorVersion, dwBuildNumber, dwPlatformId; CHAR szCSDVersion[128]; } OSVERSIONINFO;
typedef struct { DWORD dwLength, dwMemoryLoad; SIZE_T dwTotalPhys, dwAvailPhys, dwTotalPageFile, dwAvailPageFile, dwTotalVirtual, dwAvailVirtual; } MEMORYSTATUS;
typedef DWORD (WINAPI *LPTHREAD_START_ROUTINE)(LPVOID);
typedef LRESULT (CALLBACK *WNDPROC)(HWND, UINT, WPARAM, LPARAM);

#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define FILE_SHARE_READ 1
#define FILE_SHARE_WRITE 2
#define CREATE_NEW 1
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define FILE_ATTRIBUTE_READONLY 0x01
#define FILE_ATTRIBUTE_HIDDEN 0x02
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#define FILE_ATTRIBUTE_NORMAL 0x80
#define FILE_BEGIN 0
#define FILE_CURRENT 1
#define FILE_END 2
#define FILE_FLAG_SEQUENTIAL_SCAN 0x08000000
#define FILE_FLAG_RANDOM_ACCESS 0x10000000
#define THREAD_PRIORITY_NORMAL 0
#define THREAD_PRIORITY_ABOVE_NORMAL 1
#define THREAD_PRIORITY_HIGHEST 2
#define THREAD_PRIORITY_BELOW_NORMAL -1
#define THREAD_PRIORITY_LOWEST -2
#define THREAD_PRIORITY_IDLE -15
#define THREAD_PRIORITY_TIME_CRITICAL 15
#define MB_OK 0
#define MB_ICONERROR 0x10
#define MB_ICONSTOP 0x10
#define MB_ICONWARNING 0x30
#define MB_ICONEXCLAMATION 0x30
#define MB_YESNO 4
#define MB_TASKMODAL 0x2000
#define IDYES 6
#define IDNO 7
#define IDOK 1
#define PM_REMOVE 1
#define PM_NOREMOVE 0
#define WM_QUIT 0x12
#define WM_DESTROY 0x02
#define WM_PAINT 0x0F
#define WM_ERASEBKGND 0x14
#define WM_ACTIVATEAPP 0x1C
#define WM_NCACTIVATE 0x86
#define WM_KEYDOWN 0x100
#define WM_KEYUP 0x101
#define WM_CHAR 0x102
#define WM_SYSCOMMAND 0x112
#define SW_HIDE 0
#define SW_SHOWNORMAL 1
#define SW_SHOW 5
#define SW_MINIMIZE 6
#define SW_RESTORE 9
#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_RETURN 0x0D
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define VK_PAUSE 0x13
#define VK_CAPITAL 0x14
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_INSERT 0x2D
#define VK_DELETE 0x2E
#define VK_F1 0x70
#define VK_F2 0x71
#define VK_F3 0x72
#define VK_F4 0x73
#define VK_F5 0x74
#define VK_F6 0x75
#define VK_F7 0x76
#define VK_F8 0x77
#define VK_F9 0x78
#define VK_F10 0x79
#define VK_F11 0x7A
#define VK_F12 0x7B
#define VK_LBUTTON 0x01
#define VK_RBUTTON 0x02
#define VK_MBUTTON 0x04
SHORT GetAsyncKeyState(int);
BOOL GetKeyboardState(LPBYTE);
int ToAscii(UINT, UINT, const BYTE *, LPWORD, UINT);
HDC GetDC(HWND);
int ReleaseDC(HWND, HDC);
HGDIOBJ SelectObject(HDC, HGDIOBJ);
BOOL GetTextExtentPoint32(HDC, LPCSTR, int, LPSIZE);
BOOL TextOut(HDC, int, int, LPCSTR, int);
COLORREF SetTextColor(HDC, COLORREF);
COLORREF SetBkColor(HDC, COLORREF);
int SetBkMode(HDC, int);
#define TRANSPARENT 1
#define OPAQUE 2
SHORT GetKeyState(int);
typedef void *HKL;
HKL GetKeyboardLayout(DWORD);
#define SW_MAXIMIZE 3
#define MB_ABORTRETRYIGNORE 2
#define MB_RETRYCANCEL 5
#define MB_OKCANCEL 1
#define IDCANCEL 2
#define IDABORT 3
#define IDRETRY 4
#define IDIGNORE 5
HWND FindWindow(LPCSTR, LPCSTR);
BOOL DestroyWindow(HWND);
#define DRIVE_UNKNOWN 0
#define DRIVE_REMOVABLE 2
#define DRIVE_FIXED 3
#define DRIVE_CDROM 5
UINT GetDriveType(LPCSTR);
BOOL ClipCursor(const RECT *);
#define WM_MOUSEMOVE 0x200
#define WM_LBUTTONDOWN 0x201
#define WM_LBUTTONUP 0x202
#define WM_LBUTTONDBLCLK 0x203
#define WM_RBUTTONDOWN 0x204
#define WM_RBUTTONUP 0x205
#define WM_RBUTTONDBLCLK 0x206
#define GWL_WNDPROC (-4)
#define GWL_STYLE (-16)
#define GWL_EXSTYLE (-20)
#define WS_VISIBLE 0x10000000L
#define WS_POPUP 0x80000000L
#define HWND_TOPMOST ((HWND)(intptr_t)-1)
#define HWND_NOTOPMOST ((HWND)(intptr_t)-2)
#define SWP_NOSIZE 0x1
#define SWP_NOMOVE 0x2
#define SWP_NOZORDER 0x4
#define SWP_FRAMECHANGED 0x20
#define SWP_SHOWWINDOW 0x40
LONG GetWindowLong(HWND, int);
LONG SetWindowLong(HWND, int, LONG);
BOOL SetWindowPos(HWND, HWND, int, int, int, int, UINT);
LRESULT CallWindowProc(WNDPROC, HWND, UINT, WPARAM, LPARAM);
/* sys/timeb.h do MSVC */
struct _timeb { time_t time; unsigned short millitm; short timezone, dstflag; };
void _ftime(struct _timeb *);
int MapWindowPoints(HWND, HWND, LPPOINT, UINT);
BOOL DeleteObject(HGDIOBJ);
UINT MapVirtualKey(UINT, UINT);
UINT MapVirtualKeyEx(UINT, UINT, HKL);
int _rmdir(const char *);
typedef struct { HANDLE hProcess, hThread; DWORD dwProcessId, dwThreadId; } PROCESS_INFORMATION, *LPPROCESS_INFORMATION;
typedef struct { DWORD cb; LPSTR lpReserved, lpDesktop, lpTitle; DWORD dwX, dwY, dwXSize, dwYSize, dwXCountChars,
	dwYCountChars, dwFillAttribute, dwFlags; WORD wShowWindow, cbReserved2; LPBYTE lpReserved2;
	HANDLE hStdInput, hStdOutput, hStdError; } STARTUPINFO, *LPSTARTUPINFO;
BOOL CreateProcess(LPCSTR, LPSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID, LPCSTR,
	LPSTARTUPINFO, LPPROCESS_INFORMATION);
#define NORMAL_PRIORITY_CLASS 0x20
#define VK_NUMPAD0 0x60
#define VK_NUMPAD1 0x61
#define VK_NUMPAD2 0x62
#define VK_NUMPAD3 0x63
#define VK_NUMPAD4 0x64
#define VK_NUMPAD5 0x65
#define VK_NUMPAD6 0x66
#define VK_NUMPAD7 0x67
#define VK_NUMPAD8 0x68
#define VK_NUMPAD9 0x69
#define VK_MULTIPLY 0x6A
#define VK_ADD 0x6B
#define VK_SEPARATOR 0x6C
#define VK_SUBTRACT 0x6D
#define VK_DECIMAL 0x6E
#define VK_DIVIDE 0x6F
#define VK_NUMLOCK 0x90
#define VK_SCROLL 0x91
#define VK_LSHIFT 0xA0
#define VK_RSHIFT 0xA1
#define VK_LCONTROL 0xA2
#define VK_RCONTROL 0xA3
#define VK_LMENU 0xA4
#define VK_RMENU 0xA5
#define VK_SNAPSHOT 0x2C
#define VK_HELP 0x2F
#define VK_LWIN 0x5B
#define VK_RWIN 0x5C
#define VK_APPS 0x5D
#define VK_F13 0x7C
#define VK_F14 0x7D
#define VK_F15 0x7E
#define VK_F16 0x7F
#define VK_OEM_1 0xBA
#define VK_OEM_PLUS 0xBB
#define VK_OEM_COMMA 0xBC
#define VK_OEM_MINUS 0xBD
#define VK_OEM_PERIOD 0xBE
#define VK_OEM_2 0xBF
#define VK_OEM_3 0xC0
#define VK_OEM_4 0xDB
#define VK_OEM_5 0xDC
#define VK_OEM_6 0xDD
#define VK_OEM_7 0xDE
HWND GetFocus(void);
int GetWindowText(HWND, LPSTR, int);
#define SEM_FAILCRITICALERRORS 0x0001
#define SEM_NOOPENFILEERRORBOX 0x8000
UINT SetErrorMode(UINT);
typedef int HFILE;
#define HFILE_ERROR ((HFILE)-1)
#define OF_EXIST 0x4000
#define OF_READ 0
typedef struct _OFSTRUCT { BYTE cBytes, fFixedDisk; WORD nErrCode, Reserved1, Reserved2; CHAR szPathName[128]; } OFSTRUCT, *LPOFSTRUCT;
HFILE OpenFile(LPCSTR, LPOFSTRUCT, UINT);
/* Arquivos .ini (autoexec.cfg e afins): implementados sobre game:\ em win32_xbox.cpp. */
DWORD GetPrivateProfileString(LPCSTR, LPCSTR, LPCSTR, LPSTR, DWORD, LPCSTR);
BOOL WritePrivateProfileString(LPCSTR, LPCSTR, LPCSTR, LPCSTR);
UINT GetPrivateProfileInt(LPCSTR, LPCSTR, INT, LPCSTR);
DWORD GetLogicalDrives(void);

/* Kernel */
void Sleep(DWORD ms);
DWORD GetTickCount(void);
DWORD timeGetTime(void);
BOOL QueryPerformanceCounter(LARGE_INTEGER *c);
BOOL QueryPerformanceFrequency(LARGE_INTEGER *f);
void InitializeCriticalSection(CRITICAL_SECTION *cs);
void DeleteCriticalSection(CRITICAL_SECTION *cs);
void EnterCriticalSection(CRITICAL_SECTION *cs);
void LeaveCriticalSection(CRITICAL_SECTION *cs);
LONG InterlockedIncrement(LONG volatile *p);
LONG InterlockedDecrement(LONG volatile *p);
LONG InterlockedExchange(LONG volatile *p, LONG v);
HANDLE CreateThread(LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD);
BOOL SetThreadPriority(HANDLE, int);
int GetThreadPriority(HANDLE);
DWORD ResumeThread(HANDLE);
DWORD SuspendThread(HANDLE);
BOOL TerminateThread(HANDLE, DWORD);
BOOL GetExitCodeThread(HANDLE, LPDWORD);
HANDLE GetCurrentThread(void);
DWORD GetCurrentThreadId(void);
HANDLE GetCurrentProcess(void);
DWORD GetCurrentProcessId(void);
HANDLE CreateEvent(LPSECURITY_ATTRIBUTES, BOOL manual, BOOL initial, LPCSTR);
HANDLE CreateMutex(LPSECURITY_ATTRIBUTES, BOOL, LPCSTR);
HANDLE CreateSemaphore(LPSECURITY_ATTRIBUTES, LONG, LONG, LPCSTR);
BOOL ReleaseSemaphore(HANDLE, LONG, LPLONG);
BOOL ReleaseMutex(HANDLE);
BOOL SetEvent(HANDLE);
BOOL ResetEvent(HANDLE);
BOOL PulseEvent(HANDLE);
DWORD WaitForSingleObjectEx(HANDLE, DWORD, BOOL);
#define CREATE_SUSPENDED 0x4
#define WAIT_ABANDONED 0x80
#define WAIT_FAILED 0xFFFFFFFFu
#define WAIT_IO_COMPLETION 0xC0
void DebugBreak(void);
DWORD WaitForSingleObject(HANDLE, DWORD);
DWORD WaitForMultipleObjects(DWORD, const HANDLE *, BOOL, DWORD);
BOOL CloseHandle(HANDLE);
DWORD GetLastError(void);
void SetLastError(DWORD);
void OutputDebugString(LPCSTR);
int MessageBox(HWND, LPCSTR text, LPCSTR caption, UINT);
void GetLocalTime(LPSYSTEMTIME);
void GetSystemTime(LPSYSTEMTIME);
void GlobalMemoryStatus(MEMORYSTATUS *);
BOOL GetVersionEx(OSVERSIONINFO *);
DWORD GetModuleFileName(HMODULE, LPSTR, DWORD);
HMODULE GetModuleHandle(LPCSTR);
#define GetModuleHandleA GetModuleHandle
HMODULE LoadLibrary(LPCSTR);
FARPROC GetProcAddress(HMODULE, LPCSTR);
BOOL FreeLibrary(HMODULE);
LPVOID VirtualAlloc(LPVOID, SIZE_T, DWORD, DWORD);
BOOL VirtualFree(LPVOID, SIZE_T, DWORD);
#define MEM_COMMIT 0x1000
#define MEM_RESERVE 0x2000
#define MEM_RELEASE 0x8000
#define PAGE_READWRITE 4

/* Arquivos */
HANDLE CreateFile(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
BOOL ReadFile(HANDLE, LPVOID, DWORD, LPDWORD, void *);
BOOL WriteFile(HANDLE, LPCVOID, DWORD, LPDWORD, void *);
DWORD SetFilePointer(HANDLE, LONG, PLONG, DWORD);
DWORD GetFileSize(HANDLE, LPDWORD);
BOOL DeleteFile(LPCSTR);
BOOL CreateDirectory(LPCSTR, LPSECURITY_ATTRIBUTES);
BOOL RemoveDirectory(LPCSTR);
DWORD GetFileAttributes(LPCSTR);
BOOL SetFileAttributes(LPCSTR, DWORD);
HANDLE FindFirstFile(LPCSTR, LPWIN32_FIND_DATA);
BOOL FindNextFile(HANDLE, LPWIN32_FIND_DATA);
BOOL FindClose(HANDLE);
DWORD GetCurrentDirectory(DWORD, LPSTR);
BOOL SetCurrentDirectory(LPCSTR);
DWORD GetFullPathName(LPCSTR, DWORD, LPSTR, LPSTR *);
BOOL GetFileTime(HANDLE, LPFILETIME, LPFILETIME, LPFILETIME);
LONG CompareFileTime(const FILETIME *, const FILETIME *);
DWORD GetTempPath(DWORD, LPSTR);
UINT GetTempFileName(LPCSTR, LPCSTR, UINT, LPSTR);
#define INVALID_FILE_SIZE 0xFFFFFFFFu
#define INVALID_FILE_ATTRIBUTES 0xFFFFFFFFu

/* Janela/mensagens: o console nao tem; existem para compilar. */
BOOL PeekMessage(LPMSG, HWND, UINT, UINT, UINT);
BOOL TranslateMessage(const MSG *);
LRESULT DispatchMessage(const MSG *);
void PostQuitMessage(int);
BOOL ShowCursor(BOOL);
BOOL SetCursorPos(int, int);
BOOL GetCursorPos(LPPOINT);
BOOL GetClientRect(HWND, LPRECT);
BOOL GetWindowRect(HWND, LPRECT);
BOOL IsIconic(HWND);
HWND GetForegroundWindow(void);
HWND SetFocus(HWND);
BOOL ShowWindow(HWND, int);
/* Janela do renderizador (d3d.ren): no console a "janela" e a tela inteira. */
HWND GetDesktopWindow(void);
BOOL ClientToScreen(HWND, LPPOINT);
BOOL AdjustWindowRect(LPRECT, DWORD, BOOL);
#define SWP_NOOWNERZORDER 0x200
/* Capturas de tela em .bmp (d3d_surface.cpp) */
#pragma pack(push, 2)
typedef struct tagBITMAPFILEHEADER { WORD bfType; DWORD bfSize; WORD bfReserved1, bfReserved2; DWORD bfOffBits; } BITMAPFILEHEADER;
#pragma pack(pop)
typedef struct tagBITMAPINFOHEADER {
	DWORD biSize; LONG biWidth, biHeight; WORD biPlanes, biBitCount; DWORD biCompression, biSizeImage;
	LONG biXPelsPerMeter, biYPelsPerMeter; DWORD biClrUsed, biClrImportant;
} BITMAPINFOHEADER;
#define BI_RGB 0
/* Cursor do Windows (lt_cursor_impl.cpp): no console nao ha; o jogo desenha o seu. */
HCURSOR SetCursor(HCURSOR);
HANDLE LoadImage(HINSTANCE, LPCSTR, UINT, int, int, UINT);
#define IMAGE_CURSOR 2
#define LR_DEFAULTCOLOR 0

typedef struct {
	WORD wProcessorArchitecture, wReserved; DWORD dwPageSize; LPVOID lpMinimumApplicationAddress,
	lpMaximumApplicationAddress; DWORD_PTR dwActiveProcessorMask; DWORD dwNumberOfProcessors, dwProcessorType,
	dwAllocationGranularity; WORD wProcessorLevel, wProcessorRevision;
} SYSTEM_INFO, *LPSYSTEM_INFO;
void GetSystemInfo(LPSYSTEM_INFO);
typedef struct { PVOID BaseAddress, AllocationBase; DWORD AllocationProtect; SIZE_T RegionSize; DWORD State, Protect, Type; } MEMORY_BASIC_INFORMATION;
SIZE_T VirtualQuery(LPCVOID, MEMORY_BASIC_INFORMATION *, SIZE_T);
HANDLE GetProcessHeap(void);
UINT HeapCompact(HANDLE, DWORD);
HANDLE LocalFree(HANDLE);
void GetSystemTimeAsFileTime(LPFILETIME);
#define FORMAT_MESSAGE_ALLOCATE_BUFFER 0x100
#define FORMAT_MESSAGE_FROM_STRING 0x400
#define FORMAT_MESSAGE_FROM_SYSTEM 0x1000
#define FORMAT_MESSAGE_FROM_HMODULE 0x800
#define FORMAT_MESSAGE_ARGUMENT_ARRAY 0x2000
#define LANG_NEUTRAL 0
#define SUBLANG_DEFAULT 1
#define MAKELANGID(p, s) ((((WORD)(s)) << 10) | (WORD)(p))
#define FORMAT_MESSAGE_IGNORE_INSERTS 0x200
DWORD FormatMessage(DWORD, LPCVOID, DWORD, DWORD, LPSTR, DWORD, va_list *);
int wvsprintf(LPSTR, LPCSTR, va_list);

/* Recursos (.rc) de cres.dll: no console nao ha recursos de DLL; LoadString devolve 0. */
typedef void *HRSRC;
#define MAKEINTRESOURCE(i) ((LPSTR)(ULONG_PTR)(WORD)(i))
#define RT_RCDATA MAKEINTRESOURCE(10)
#define RT_STRING MAKEINTRESOURCE(6)
HRSRC FindResource(HMODULE, LPCSTR, LPCSTR);
HGLOBAL LoadResource(HMODULE, HRSRC);
LPVOID LockResource(HGLOBAL);
DWORD SizeofResource(HMODULE, HRSRC);
int LoadString(HINSTANCE, UINT, LPSTR, int);

/* Excecoes estruturadas: sem SEH no PowerPC/GCC. */
typedef struct _EXCEPTION_POINTERS { void *ExceptionRecord, *ContextRecord; } EXCEPTION_POINTERS, *PEXCEPTION_POINTERS;
#define __try if (1)
#define __except(x) else
#define __finally
#define EXCEPTION_EXECUTE_HANDLER 1
#define EXCEPTION_CONTINUE_SEARCH 0

/* new handler do MSVC */
typedef int (*_PNH)(size_t);
_PNH _set_new_handler(_PNH);

/* findfirst/findnext do io.h */
struct _finddata_t { unsigned attrib; long time_create, time_access, time_write; unsigned long size; char name[260]; };
intptr_t _findfirst(const char *, struct _finddata_t *);
int _findnext(intptr_t, struct _finddata_t *);
int _findclose(intptr_t);
#define _A_SUBDIR 0x10
#define _A_NORMAL 0x00
#define _A_RDONLY 0x01
#define _A_HIDDEN 0x02
#define _S_IFDIR 0040000
#define _S_IFREG 0100000

/* mmsystem.h */
typedef DWORD FOURCC;
#define mmioFOURCC(a, b, c, d) ((DWORD)(BYTE)(a) | ((DWORD)(BYTE)(b) << 8) | ((DWORD)(BYTE)(c) << 16) | ((DWORD)(BYTE)(d) << 24))
#define MAKEFOURCC mmioFOURCC
#define WAVE_FORMAT_PCM 1
typedef struct { WORD wFormatTag, nChannels; DWORD nSamplesPerSec, nAvgBytesPerSec; WORD nBlockAlign, wBitsPerSample, cbSize; } WAVEFORMATEX, *LPWAVEFORMATEX;
typedef struct { WORD wFormatTag, nChannels; DWORD nSamplesPerSec, nAvgBytesPerSec; WORD nBlockAlign; } WAVEFORMAT;
typedef struct { WAVEFORMAT wf; WORD wBitsPerSample; } PCMWAVEFORMAT;
typedef WAVEFORMAT *LPWAVEFORMAT;
typedef void *HWAVEOUT, *HMIDIOUT, *HDRVR;
typedef HWAVEOUT *LPHWAVEOUT;
typedef HMIDIOUT *LPHMIDIOUT;
typedef UINT MMRESULT;
#define WAVE_MAPPER ((UINT)-1)

/* tchar.h */
#define _tcscmp strcmp
#define _tcsicmp strcasecmp
#define _tcslen strlen
#define _tcscpy strcpy
#define _tcsinc(p) ((p) + 1)
#ifndef _tcsninc
#define _tcsninc(p, n) ((p) + (n))
#endif
#define _tcschr strchr
#define _tcsrchr strrchr

/* Strings do MSVC */
#define stricmp strcasecmp
#define _stricmp strcasecmp
#define strnicmp strncasecmp
#define _strnicmp strncasecmp
#define strcmpi strcasecmp
#define _strcmpi strcasecmp
#define _snprintf snprintf
#define _vsnprintf vsnprintf
#define lstrcpy strcpy
#define lstrcpyn(d, s, n) (strncpy((d), (s), (n) - 1), (d)[(n) - 1] = 0, (d))
#define lstrlen strlen
#define lstrcmp strcmp
#define lstrcmpi strcasecmp
#define lstrcat strcat
#define wsprintf sprintf
char *_strupr(char *);
char *_strlwr(char *);
#define strupr _strupr
#define strlwr _strlwr
char *_itoa(int, char *, int);
char *_ltoa(long, char *, int);
#define itoa _itoa
#define ltoa _ltoa
char *_strdup(const char *);
#define _getcwd(b, n) GetCurrentDirectory((n), (b))
void _splitpath(const char *, char *, char *, char *, char *);
void _makepath(char *, const char *, const char *, const char *, const char *);
int _mkdir(const char *);
#define _alloca __builtin_alloca
#define alloca __builtin_alloca

#ifdef __cplusplus
}
#endif

#endif
