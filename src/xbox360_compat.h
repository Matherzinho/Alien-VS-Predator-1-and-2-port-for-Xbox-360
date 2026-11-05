#ifndef XBOX360_COMPAT_H
#define XBOX360_COMPAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#ifndef CALLBACK
#define CALLBACK
#endif
#ifndef WINAPI
#define WINAPI
#endif
#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef __cdecl
#define __cdecl
#endif
#ifndef __stdcall
#define __stdcall
#endif
#ifndef _inline
#define _inline inline
#endif

typedef int                 BOOL;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef unsigned long       DWORD;
typedef long                LONG;
typedef unsigned long       ULONG;
typedef int                 INT;
typedef unsigned int        UINT;
typedef float               FLOAT;
typedef void               *LPVOID;
typedef const void         *LPCVOID;
typedef char               *LPSTR;
typedef const char         *LPCSTR;
typedef char               *LPTSTR;
typedef const char         *LPCTSTR;
typedef DWORD              *LPDWORD;
typedef LONG               *PLONG;
typedef LONG               *LPLONG;
typedef BYTE               *LPBYTE;
typedef void               *HANDLE;
typedef void               *HINSTANCE;
typedef void               *HWND;
typedef void               *HDC;
typedef void               *HMENU;
typedef long                HRESULT;
typedef unsigned int        WPARAM;
typedef long                LPARAM;
typedef long                LRESULT;

#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define INVALID_FILE_SIZE    ((DWORD)0xFFFFFFFF)

#define GENERIC_READ         0x80000000UL
#define GENERIC_WRITE        0x40000000UL

#define FILE_SHARE_READ      0x00000001UL
#define FILE_SHARE_WRITE     0x00000002UL

#define CREATE_NEW           1
#define CREATE_ALWAYS        2
#define OPEN_EXISTING        3
#define OPEN_ALWAYS          4
#define TRUNCATE_EXISTING    5

#define FILE_ATTRIBUTE_READONLY  0x00000001UL
#define FILE_ATTRIBUTE_HIDDEN    0x00000002UL
#define FILE_ATTRIBUTE_SYSTEM    0x00000004UL
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010UL
#define FILE_ATTRIBUTE_ARCHIVE   0x00000020UL
#define FILE_ATTRIBUTE_NORMAL    0x00000080UL
#define FILE_FLAG_RANDOM_ACCESS  0x10000000UL
#define FILE_FLAG_SEQUENTIAL_SCAN 0x08000000UL

#define FILE_BEGIN           0
#define FILE_CURRENT         1
#define FILE_END             2

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

typedef union _LARGE_INTEGER {
    struct {
        DWORD LowPart;
        LONG  HighPart;
    };
    struct {
        DWORD LowPart;
        LONG  HighPart;
    } u;
    int64_t QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef struct _FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

typedef struct _SYSTEMTIME {
    WORD wYear;
    WORD wMonth;
    WORD wDayOfWeek;
    WORD wDay;
    WORD wHour;
    WORD wMinute;
    WORD wSecond;
    WORD wMilliseconds;
} SYSTEMTIME, *PSYSTEMTIME, *LPSYSTEMTIME;

typedef struct _WIN32_FIND_DATAA {
    DWORD    dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD    nFileSizeHigh;
    DWORD    nFileSizeLow;
    DWORD    dwReserved0;
    DWORD    dwReserved1;
    char     cFileName[MAX_PATH];
    char     cAlternateFileName[14];
} WIN32_FIND_DATAA, *PWIN32_FIND_DATAA, *LPWIN32_FIND_DATAA, WIN32_FIND_DATA;

typedef struct _RECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT, *PRECT, *LPRECT;

typedef struct _POINT {
    LONG x;
    LONG y;
} POINT, *PPOINT, *LPPOINT;

typedef struct tagPALETTEENTRY {
    BYTE peRed;
    BYTE peGreen;
    BYTE peBlue;
    BYTE peFlags;
} PALETTEENTRY, *PPALETTEENTRY, *LPPALETTEENTRY;

typedef struct _MEMORYSTATUS {
    DWORD dwLength;
    DWORD dwMemoryLoad;
    DWORD dwTotalPhys;
    DWORD dwAvailPhys;
    DWORD dwTotalPageFile;
    DWORD dwAvailPageFile;
    DWORD dwTotalVirtual;
    DWORD dwAvailVirtual;
} MEMORYSTATUS, *LPMEMORYSTATUS;

/* Path translation for Xbox 360 (maps relative/D:\ paths to game:\avp1\ or game:\avp2\) */
void   xbox360_set_game_root(const char *root_dir);
const char *xbox360_get_game_root(void);
void   xbox360_translate_path(const char *in_path, char *out_path, size_t out_size);
FILE  *xbox360_fopen(const char *path, const char *mode);
#ifndef XBOX360_COMPAT_IMPLEMENTATION
#undef fopen
#define fopen xbox360_fopen
#endif

/* Win32 File I/O Compatibility Layer */
HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                   LPVOID lpSecurityAttributes, DWORD dwCreationDisposition,
                   DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
#define CreateFile CreateFileA

BOOL   ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
                LPDWORD lpNumberOfBytesRead, LPVOID lpOverlapped);
BOOL   WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
                 LPDWORD lpNumberOfBytesWritten, LPVOID lpOverlapped);
DWORD  SetFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh,
                      DWORD dwMoveMethod);
DWORD  GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh);
BOOL   SetEndOfFile(HANDLE hFile);
BOOL   CloseHandle(HANDLE hObject);
BOOL   DeleteFileA(LPCSTR lpFileName);
#define DeleteFile DeleteFileA
BOOL   MoveFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName);
#define MoveFile MoveFileA
DWORD  GetFileAttributesA(LPCSTR lpFileName);
#define GetFileAttributes GetFileAttributesA
BOOL   CreateDirectoryA(LPCSTR lpPathName, LPVOID lpSecurityAttributes);
#define CreateDirectory CreateDirectoryA

HANDLE FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData);
#define FindFirstFile FindFirstFileA
BOOL   FindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData);
#define FindNextFile FindNextFileA
BOOL   FindClose(HANDLE hFindFile);

/* Win32 Time / System / Debug Compatibility Layer */
DWORD  timeGetTime(void);
DWORD  GetTickCount(void);
BOOL   QueryPerformanceCounter(LARGE_INTEGER *lpPerformanceCount);
BOOL   QueryPerformanceFrequency(LARGE_INTEGER *lpFrequency);
void   Sleep(DWORD dwMilliseconds);
void   GetLocalTime(LPSYSTEMTIME lpSystemTime);
void   GetSystemTime(LPSYSTEMTIME lpSystemTime);
BOOL   SystemTimeToFileTime(const SYSTEMTIME *lpSystemTime, LPFILETIME lpFileTime);
LONG   CompareFileTime(const FILETIME *lpFileTime1, const FILETIME *lpFileTime2);
void   GlobalMemoryStatus(LPMEMORYSTATUS lpBuffer);
void   OutputDebugStringA(LPCSTR lpOutputString);
#define OutputDebugString OutputDebugStringA
DWORD  GetLastError(void);

#define FORMAT_MESSAGE_ALLOCATE_BUFFER 0x00000100
#define FORMAT_MESSAGE_FROM_SYSTEM     0x00001000
#define LANG_NEUTRAL                   0x00
#define SUBLANG_DEFAULT                0x01
#define MAKELANGID(p, s)               ((((WORD)(s)) << 10) | (WORD)(p))
DWORD  FormatMessageA(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId,
                      DWORD dwLanguageId, LPSTR lpBuffer, DWORD nSize, LPVOID Arguments);
#define FormatMessage FormatMessageA
LPVOID LocalFree(LPVOID hMem);

/* C Runtime String Helpers */
#ifndef _stricmp
#define _stricmp strcasecmp
#endif
#ifndef strcmpi
#define strcmpi strcasecmp
#endif
#ifndef stricmp
#define stricmp strcasecmp
#endif
#ifndef _strnicmp
#define _strnicmp strncasecmp
#endif
#ifndef strnicmp
#define strnicmp strncasecmp
#endif

static inline size_t _mbclen(const unsigned char *s) {
    return (s && *s) ? 1 : 0;
}

char *_strupr(char *str);
char *_strlwr(char *str);

#ifdef __cplusplus
}
#endif

#endif /* XBOX360_COMPAT_H */
