/*
 * Aliens vs. Predator 1 & 2 — Xbox 360 Port
 * Win32 & Xbox Classic Compatibility Layer over xsdk + standard C stdio
 */

#define XBOX360_COMPAT_IMPLEMENTATION
#include "xbox360_compat.h"

static char g_game_root[128] = "game:\\avp1\\";

void xbox360_set_game_root(const char *root_dir) {
    if (!root_dir || !root_dir[0]) {
        strcpy(g_game_root, "game:\\");
        return;
    }
    strncpy(g_game_root, root_dir, sizeof(g_game_root) - 2);
    g_game_root[sizeof(g_game_root) - 2] = '\0';
    size_t n = strlen(g_game_root);
    if (n > 0 && g_game_root[n - 1] != '\\' && g_game_root[n - 1] != '/')
        strcat(g_game_root, "\\");
}

const char *xbox360_get_game_root(void) {
    return g_game_root;
}

void xbox360_translate_path(const char *in_path, char *out_path, size_t out_size) {
    if (!out_path || out_size == 0) return;
    if (!in_path || !in_path[0]) {
        out_path[0] = '\0';
        return;
    }

    /* Skip leading "./" or ".\" */
    while (in_path[0] == '.' && (in_path[1] == '/' || in_path[1] == '\\')) {
        in_path += 2;
    }

    /* Check if path already has a device prefix like "game:\" or "cache:\" */
    const char *colon = strchr(in_path, ':');
    if (colon) {
        size_t prefix_len = (size_t)(colon - in_path);
        if (prefix_len == 1 && (in_path[0] == 'd' || in_path[0] == 'D' ||
                                in_path[0] == 'c' || in_path[0] == 'C')) {
            /* Xbox Classic D:\ or Win32 C:\ -> map to current game root */
            in_path = colon + 1;
            while (*in_path == '\\' || *in_path == '/') in_path++;
            snprintf(out_path, out_size, "%s%s", g_game_root, in_path);
        } else {
            /* Already "game:\..." or "cache:\..." */
            strncpy(out_path, in_path, out_size - 1);
            out_path[out_size - 1] = '\0';
        }
    } else {
        while (*in_path == '\\' || *in_path == '/') in_path++;
        snprintf(out_path, out_size, "%s%s", g_game_root, in_path);
    }

    /* Normalize forward slashes to Xbox 360 backslashes */
    for (size_t i = 0; out_path[i]; i++) {
        if (out_path[i] == '/') out_path[i] = '\\';
    }
}

static FILE *open_with_case_fallback(const char *translated_path, const char *mode) {
    FILE *fp = fopen(translated_path, mode);
    if (fp) return fp;

    char alt[MAX_PATH];
    strncpy(alt, translated_path, sizeof(alt) - 1);
    alt[sizeof(alt) - 1] = '\0';
    char *last_sep = strrchr(alt, '\\');
    char *fname = last_sep ? (last_sep + 1) : alt;

    /* 1. Try all-lowercase filename */
    for (char *p = fname; *p; p++) *p = (char)tolower((unsigned char)*p);
    fp = fopen(alt, mode);
    if (fp) return fp;

    /* 2. Try TitleCase stem + UPPERCASE extension (e.g. Derelict.RIF, Tex1.FFL) */
    if (fname[0]) {
        fname[0] = (char)toupper((unsigned char)fname[0]);
        char *dot = strrchr(fname, '.');
        if (dot) {
            for (char *p = dot + 1; *p; p++) *p = (char)toupper((unsigned char)*p);
        }
        fp = fopen(alt, mode);
        if (fp) return fp;

        /* 3. Try lowercase stem + UPPERCASE extension (e.g. alien_hud.RIF) */
        fname[0] = (char)tolower((unsigned char)fname[0]);
        fp = fopen(alt, mode);
        if (fp) return fp;

        /* 4. Try TitleCase stem + lowercase extension (e.g. Track 1.wav) */
        fname[0] = (char)toupper((unsigned char)fname[0]);
        if (dot) {
            for (char *p = dot + 1; *p; p++) *p = (char)tolower((unsigned char)*p);
        }
        fp = fopen(alt, mode);
        if (fp) return fp;
    }

    /* 5. Try all-uppercase filename */
    for (char *p = fname; *p; p++) *p = (char)toupper((unsigned char)*p);
    fp = fopen(alt, mode);
    if (fp) return fp;

    /* 6. Try lowercasing everything after device prefix (e.g. game:\...) */
    strncpy(alt, translated_path, sizeof(alt) - 1);
    alt[sizeof(alt) - 1] = '\0';
    char *colon = strchr(alt, ':');
    char *rel = colon ? (colon + 1) : alt;
    for (char *p = rel; *p; p++) *p = (char)tolower((unsigned char)*p);
    fp = fopen(alt, mode);
    return fp;
}

FILE *xbox360_fopen(const char *path, const char *mode) {
    if (!path || !mode) return NULL;
    char translated[MAX_PATH];
    xbox360_translate_path(path, translated, sizeof(translated));
    return open_with_case_fallback(translated, mode);
}

HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                   LPVOID lpSecurityAttributes, DWORD dwCreationDisposition,
                   DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    (void)dwShareMode;
    (void)lpSecurityAttributes;
    (void)dwFlagsAndAttributes;
    (void)hTemplateFile;

    if (!lpFileName) return INVALID_HANDLE_VALUE;

    char path[MAX_PATH];
    xbox360_translate_path(lpFileName, path, sizeof(path));

    const char *mode = "rb";
    bool write_req = (dwDesiredAccess & GENERIC_WRITE) != 0;
    bool read_req  = (dwDesiredAccess & GENERIC_READ) != 0;

    if (write_req) {
        if (dwCreationDisposition == CREATE_ALWAYS || dwCreationDisposition == TRUNCATE_EXISTING)
            mode = read_req ? "w+b" : "wb";
        else if (dwCreationDisposition == OPEN_ALWAYS)
            mode = read_req ? "a+b" : "ab";
        else
            mode = read_req ? "r+b" : "wb";
    } else {
        mode = "rb";
    }

    FILE *fp = open_with_case_fallback(path, mode);
    if (!fp) return INVALID_HANDLE_VALUE;
    return (HANDLE)fp;
}

BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
              LPDWORD lpNumberOfBytesRead, LPVOID lpOverlapped) {
    (void)lpOverlapped;
    if (lpNumberOfBytesRead) *lpNumberOfBytesRead = 0;
    if (!hFile || hFile == INVALID_HANDLE_VALUE || !lpBuffer) return FALSE;

    size_t got = fread(lpBuffer, 1, (size_t)nNumberOfBytesToRead, (FILE *)hFile);
    if (lpNumberOfBytesRead) *lpNumberOfBytesRead = (DWORD)got;
    return TRUE;
}

BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
               LPDWORD lpNumberOfBytesWritten, LPVOID lpOverlapped) {
    (void)lpOverlapped;
    if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = 0;
    if (!hFile || hFile == INVALID_HANDLE_VALUE || !lpBuffer) return FALSE;

    size_t put = fwrite(lpBuffer, 1, (size_t)nNumberOfBytesToWrite, (FILE *)hFile);
    if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = (DWORD)put;
    return (put == (size_t)nNumberOfBytesToWrite) ? TRUE : FALSE;
}

DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh,
                     DWORD dwMoveMethod) {
    (void)lpDistanceToMoveHigh;
    if (!hFile || hFile == INVALID_HANDLE_VALUE) return INVALID_FILE_SIZE;

    int origin = SEEK_SET;
    if (dwMoveMethod == FILE_CURRENT) origin = SEEK_CUR;
    else if (dwMoveMethod == FILE_END) origin = SEEK_END;

    if (fseek((FILE *)hFile, lDistanceToMove, origin) != 0)
        return INVALID_FILE_SIZE;

    long pos = ftell((FILE *)hFile);
    return (pos < 0) ? INVALID_FILE_SIZE : (DWORD)pos;
}

DWORD GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh) {
    if (lpFileSizeHigh) *lpFileSizeHigh = 0;
    if (!hFile || hFile == INVALID_HANDLE_VALUE) return INVALID_FILE_SIZE;

    FILE *fp = (FILE *)hFile;
    long cur = ftell(fp);
    if (cur < 0) return INVALID_FILE_SIZE;
    if (fseek(fp, 0, SEEK_END) != 0) return INVALID_FILE_SIZE;
    long end = ftell(fp);
    fseek(fp, cur, SEEK_SET);
    return (end < 0) ? INVALID_FILE_SIZE : (DWORD)end;
}

BOOL SetEndOfFile(HANDLE hFile) {
    if (!hFile || hFile == INVALID_HANDLE_VALUE) return FALSE;
    fflush((FILE *)hFile);
    return TRUE;
}

BOOL CloseHandle(HANDLE hObject) {
    if (!hObject || hObject == INVALID_HANDLE_VALUE) return FALSE;
    fclose((FILE *)hObject);
    return TRUE;
}

BOOL DeleteFileA(LPCSTR lpFileName) {
    if (!lpFileName) return FALSE;
    char path[MAX_PATH];
    xbox360_translate_path(lpFileName, path, sizeof(path));
    return (remove(path) == 0) ? TRUE : FALSE;
}

BOOL MoveFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName) {
    if (!lpExistingFileName || !lpNewFileName) return FALSE;
    char src[MAX_PATH], dst[MAX_PATH];
    xbox360_translate_path(lpExistingFileName, src, sizeof(src));
    xbox360_translate_path(lpNewFileName, dst, sizeof(dst));
    return (rename(src, dst) == 0) ? TRUE : FALSE;
}

DWORD GetFileAttributesA(LPCSTR lpFileName) {
    if (!lpFileName) return 0xFFFFFFFFUL;
    char path[MAX_PATH];
    xbox360_translate_path(lpFileName, path, sizeof(path));
    FILE *fp = open_with_case_fallback(path, "rb");
    if (!fp) return 0xFFFFFFFFUL;
    fclose(fp);
    return FILE_ATTRIBUTE_NORMAL;
}

BOOL CreateDirectoryA(LPCSTR lpPathName, LPVOID lpSecurityAttributes) {
    (void)lpPathName;
    (void)lpSecurityAttributes;
    return TRUE;
}

HANDLE FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData) {
    (void)lpFileName;
    if (lpFindFileData) memset(lpFindFileData, 0, sizeof(*lpFindFileData));
    return INVALID_HANDLE_VALUE;
}

BOOL FindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData) {
    (void)hFindFile;
    (void)lpFindFileData;
    return FALSE;
}

BOOL FindClose(HANDLE hFindFile) {
    (void)hFindFile;
    return TRUE;
}

/* Xbox 360 Xenon 49.875 MHz timebase -> milliseconds */
DWORD timeGetTime(void) {
    uint64_t tb = __builtin_ppc_get_timebase();
    return (DWORD)(tb / 49875ULL);
}

DWORD GetTickCount(void) {
    return timeGetTime();
}

BOOL QueryPerformanceCounter(LARGE_INTEGER *lpPerformanceCount) {
    if (!lpPerformanceCount) return FALSE;
    lpPerformanceCount->QuadPart = (int64_t)__builtin_ppc_get_timebase();
    return TRUE;
}

BOOL QueryPerformanceFrequency(LARGE_INTEGER *lpFrequency) {
    if (!lpFrequency) return FALSE;
    lpFrequency->QuadPart = 49875000LL;
    return TRUE;
}

void Sleep(DWORD dwMilliseconds) {
    uint64_t start = __builtin_ppc_get_timebase();
    uint64_t wait_ticks = (uint64_t)dwMilliseconds * 49875ULL;
    while ((__builtin_ppc_get_timebase() - start) < wait_ticks) {
        __asm__ volatile("or 27,27,27");
    }
}

void GetLocalTime(LPSYSTEMTIME lpSystemTime) {
    if (!lpSystemTime) return;
    memset(lpSystemTime, 0, sizeof(*lpSystemTime));
    lpSystemTime->wYear  = 2026;
    lpSystemTime->wMonth = 1;
    lpSystemTime->wDay   = 1;
}

void GetSystemTime(LPSYSTEMTIME lpSystemTime) {
    GetLocalTime(lpSystemTime);
}

BOOL SystemTimeToFileTime(const SYSTEMTIME *lpSystemTime, LPFILETIME lpFileTime) {
    if (!lpFileTime) return FALSE;
    uint32_t t = timeGetTime();
    lpFileTime->dwLowDateTime  = t;
    lpFileTime->dwHighDateTime = lpSystemTime ? lpSystemTime->wYear : 2026;
    return TRUE;
}

LONG CompareFileTime(const FILETIME *lpFileTime1, const FILETIME *lpFileTime2) {
    if (!lpFileTime1 || !lpFileTime2) return 0;
    if (lpFileTime1->dwHighDateTime != lpFileTime2->dwHighDateTime)
        return (lpFileTime1->dwHighDateTime > lpFileTime2->dwHighDateTime) ? 1 : -1;
    if (lpFileTime1->dwLowDateTime != lpFileTime2->dwLowDateTime)
        return (lpFileTime1->dwLowDateTime > lpFileTime2->dwLowDateTime) ? 1 : -1;
    return 0;
}

void GlobalMemoryStatus(LPMEMORYSTATUS lpBuffer) {
    if (!lpBuffer) return;
    memset(lpBuffer, 0, sizeof(*lpBuffer));
    lpBuffer->dwLength        = sizeof(*lpBuffer);
    lpBuffer->dwTotalPhys     = 512U * 1024U * 1024U;
    lpBuffer->dwAvailPhys     = 384U * 1024U * 1024U;
    lpBuffer->dwTotalPageFile = 512U * 1024U * 1024U;
    lpBuffer->dwAvailPageFile = 384U * 1024U * 1024U;
    lpBuffer->dwTotalVirtual  = 512U * 1024U * 1024U;
    lpBuffer->dwAvailVirtual  = 384U * 1024U * 1024U;
}

void OutputDebugStringA(LPCSTR lpOutputString) {
    (void)lpOutputString;
}

DWORD GetLastError(void) {
    return 0;
}

DWORD FormatMessageA(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId,
                     DWORD dwLanguageId, LPSTR lpBuffer, DWORD nSize, LPVOID Arguments) {
    (void)dwFlags;
    (void)lpSource;
    (void)dwMessageId;
    (void)dwLanguageId;
    (void)nSize;
    (void)Arguments;
    if (lpBuffer && (dwFlags & FORMAT_MESSAGE_ALLOCATE_BUFFER)) {
        char *msg = (char *)malloc(32);
        if (msg) strcpy(msg, "Xbox 360 File I/O Error");
        *(char **)lpBuffer = msg;
        return 23;
    }
    return 0;
}

LPVOID LocalFree(LPVOID hMem) {
    if (hMem) free(hMem);
    return NULL;
}

char *_strupr(char *str) {
    if (!str) return NULL;
    for (char *p = str; *p; p++) *p = (char)toupper((unsigned char)*p);
    return str;
}

char *_strlwr(char *str) {
    if (!str) return NULL;
    for (char *p = str; *p; p++) *p = (char)tolower((unsigned char)*p);
    return str;
}
