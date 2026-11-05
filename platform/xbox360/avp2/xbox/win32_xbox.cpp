// avp360: a parte da API Win32 que o LithTech 2.2 e o AvP2 chamam, sobre o xsdk (pthreads, stdio, path.c).
//
// - Modulos: cshell.dll e object.lto estao linkados no .xex (cada um como objeto isolado, ver CMakeLists.txt);
//   LoadLibrary os acha pelo nome base e GetProcAddress por uma tabela. Qualquer outra DLL (ltmsg.dll, cres.dll) e
//   lida do disco so pelos recursos: LoadString/FindResource leem a secao .rsrc do PE x86 original.
// - Handles: arquivos, eventos e threads sao objetos XHandle.
// - Janela, GDI, cursor, teclado do Windows: o console nao tem; respondem como uma janela sem foco de teclado.
#include <windows.h>
#include <objbase.h>
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <dirent.h>
// xsdk/xam.h (nao incluido: o kernel.h do xsdk tem tipos com os mesmos nomes do windows.h).
extern "C" int xsdk_launch(const char *path, const void *data, size_t size_bytes);

// ---------------------------------------------------------------- erros e tempo

static DWORD g_LastError = 0;
extern "C" DWORD GetLastError(void) { return g_LastError; }
extern "C" void SetLastError(DWORD e) { g_LastError = e; }

static uint64_t now_us(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
}
static uint64_t g_StartUs = now_us();

extern "C" DWORD timeGetTime(void) { return (DWORD)((now_us() - g_StartUs) / 1000u); }
extern "C" DWORD GetTickCount(void) { return timeGetTime(); }
extern "C" BOOL QueryPerformanceFrequency(LARGE_INTEGER *f) { f->QuadPart = 1000000; return TRUE; }
extern "C" BOOL QueryPerformanceCounter(LARGE_INTEGER *c) { c->QuadPart = (LONGLONG)now_us(); return TRUE; }
extern "C" void Sleep(DWORD ms)
{
	struct timespec ts = { (time_t)(ms / 1000), (long)(ms % 1000) * 1000000L };
	nanosleep(&ts, NULL);
}
extern "C" void _ftime(struct _timeb *tb)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	tb->time = tv.tv_sec;
	tb->millitm = (unsigned short)(tv.tv_usec / 1000);
	tb->timezone = 0;
	tb->dstflag = 0;
}

// ---------------------------------------------------------------- depuracao

extern "C" void OutputDebugString(LPCSTR s) { fputs(s ? s : "", stderr); }
extern "C" void DebugBreak(void) { fputs("avp2: DebugBreak\n", stderr); }
extern "C" int MessageBox(HWND, LPCSTR text, LPCSTR caption, UINT)
{
	fprintf(stderr, "[%s] %s\n", caption ? caption : "", text ? text : "");
	return IDOK;
}
extern "C" UINT SetErrorMode(UINT) { return 0; }
extern "C" DWORD FormatMessage(DWORD, LPCVOID, DWORD id, DWORD, LPSTR buf, DWORD size, va_list *)
{
	if (!buf || !size)
		return 0;
	snprintf(buf, size, "erro %lu", (unsigned long)id);
	return (DWORD)strlen(buf);
}
extern "C" HANDLE LocalFree(HANDLE h) { free(h); return NULL; }
extern "C" HANDLE GetProcessHeap(void) { return (HANDLE)1; }
extern "C" UINT HeapCompact(HANDLE, DWORD) { return 0; }
extern "C" _PNH _set_new_handler(_PNH) { return NULL; }

void *DefStdlithAlloc(DWORD size) { return malloc(size ? size : 1); }
void DefStdlithFree(void *ptr) { free(ptr); }

extern "C" HRESULT CoInitialize(LPVOID) { return 0; }
extern "C" void CoUninitialize(void) {}

// ---------------------------------------------------------------- handles

enum { XH_FILE = 1, XH_EVENT, XH_THREAD, XH_MODULE };

struct XHandle
{
	int kind;
};

struct XFile : XHandle
{
	FILE *fp;
};

struct XEvent : XHandle
{
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	BOOL manual, signaled;
};

struct XThread : XHandle
{
	pthread_t thread;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	LPTHREAD_START_ROUTINE fn;
	LPVOID arg;
	BOOL suspended, done;
	DWORD exitCode;
};

static XEvent *new_event(BOOL manual, BOOL initial)
{
	XEvent *e = new XEvent;
	e->kind = XH_EVENT;
	pthread_mutex_init(&e->mutex, NULL);
	pthread_cond_init(&e->cond, NULL);
	e->manual = manual;
	e->signaled = initial;
	return e;
}

extern "C" HANDLE CreateEvent(LPSECURITY_ATTRIBUTES, BOOL manual, BOOL initial, LPCSTR)
{
	return new_event(manual, initial);
}

static BOOL event_set(HANDLE h, BOOL value, BOOL pulse)
{
	XEvent *e = (XEvent *)h;
	if (!e || e->kind != XH_EVENT)
		return FALSE;
	pthread_mutex_lock(&e->mutex);
	e->signaled = value;
	if (value || pulse)
		pthread_cond_broadcast(&e->cond);
	if (pulse)
		e->signaled = FALSE;
	pthread_mutex_unlock(&e->mutex);
	return TRUE;
}
extern "C" BOOL SetEvent(HANDLE h) { return event_set(h, TRUE, FALSE); }
extern "C" BOOL ResetEvent(HANDLE h) { return event_set(h, FALSE, FALSE); }
extern "C" BOOL PulseEvent(HANDLE h) { return event_set(h, TRUE, TRUE); }

static void *thread_main(void *p)
{
	XThread *t = (XThread *)p;
	pthread_mutex_lock(&t->mutex);
	while (t->suspended)
		pthread_cond_wait(&t->cond, &t->mutex);
	pthread_mutex_unlock(&t->mutex);

	DWORD code = t->fn(t->arg);

	pthread_mutex_lock(&t->mutex);
	t->exitCode = code;
	t->done = TRUE;
	pthread_cond_broadcast(&t->cond);
	pthread_mutex_unlock(&t->mutex);
	return NULL;
}

extern "C" HANDLE CreateThread(LPSECURITY_ATTRIBUTES, SIZE_T stack, LPTHREAD_START_ROUTINE fn, LPVOID arg, DWORD flags,
	LPDWORD id)
{
	XThread *t = new XThread;
	t->kind = XH_THREAD;
	pthread_mutex_init(&t->mutex, NULL);
	pthread_cond_init(&t->cond, NULL);
	t->fn = fn;
	t->arg = arg;
	t->suspended = (flags & CREATE_SUSPENDED) != 0;
	t->done = FALSE;
	t->exitCode = 0;

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, stack > 256 * 1024 ? stack : 256 * 1024);
	int rc = pthread_create(&t->thread, &attr, thread_main, t);
	pthread_attr_destroy(&attr);
	if (rc != 0)
	{
		delete t;
		return NULL;
	}
	pthread_detach(t->thread);
	if (id)
		*id = (DWORD)(uintptr_t)t;
	return t;
}

extern "C" DWORD ResumeThread(HANDLE h)
{
	XThread *t = (XThread *)h;
	if (!t || t->kind != XH_THREAD)
		return (DWORD)-1;
	pthread_mutex_lock(&t->mutex);
	DWORD prev = t->suspended ? 1 : 0;
	t->suspended = FALSE;
	pthread_cond_broadcast(&t->cond);
	pthread_mutex_unlock(&t->mutex);
	return prev;
}

extern "C" BOOL SetThreadPriority(HANDLE, int) { return TRUE; }

// Verdadeiro se o objeto esta sinalizado; consome o sinal de eventos automaticos.
static BOOL try_acquire(HANDLE h)
{
	XHandle *x = (XHandle *)h;
	if (!x)
		return FALSE;
	if (x->kind == XH_EVENT)
	{
		XEvent *e = (XEvent *)x;
		pthread_mutex_lock(&e->mutex);
		BOOL ok = e->signaled;
		if (ok && !e->manual)
			e->signaled = FALSE;
		pthread_mutex_unlock(&e->mutex);
		return ok;
	}
	if (x->kind == XH_THREAD)
	{
		XThread *t = (XThread *)x;
		pthread_mutex_lock(&t->mutex);
		BOOL ok = t->done;
		pthread_mutex_unlock(&t->mutex);
		return ok;
	}
	return TRUE;
}

extern "C" DWORD WaitForSingleObject(HANDLE h, DWORD ms)
{
	XHandle *x = (XHandle *)h;
	if (x && x->kind == XH_EVENT)
	{
		XEvent *e = (XEvent *)x;
		struct timespec until;
		clock_gettime(CLOCK_REALTIME, &until);
		if (ms != INFINITE)
		{
			until.tv_sec += ms / 1000;
			until.tv_nsec += (long)(ms % 1000) * 1000000L;
			if (until.tv_nsec >= 1000000000L)
			{
				until.tv_sec++;
				until.tv_nsec -= 1000000000L;
			}
		}
		pthread_mutex_lock(&e->mutex);
		int rc = 0;
		while (!e->signaled && rc == 0)
			rc = (ms == INFINITE) ? pthread_cond_wait(&e->cond, &e->mutex)
				: pthread_cond_timedwait(&e->cond, &e->mutex, &until);
		BOOL ok = e->signaled;
		if (ok && !e->manual)
			e->signaled = FALSE;
		pthread_mutex_unlock(&e->mutex);
		return ok ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
	}
	if (x && x->kind == XH_THREAD)
	{
		XThread *t = (XThread *)x;
		DWORD start = timeGetTime();
		while (!try_acquire(h))
		{
			if (ms != INFINITE && timeGetTime() - start >= ms)
				return WAIT_TIMEOUT;
			Sleep(1);
		}
		(void)t;
		return WAIT_OBJECT_0;
	}
	return WAIT_OBJECT_0;
}

extern "C" DWORD WaitForMultipleObjects(DWORD n, const HANDLE *hs, BOOL all, DWORD ms)
{
	DWORD start = timeGetTime();
	for (;;)
	{
		if (all)
		{
			BOOL ok = TRUE;
			for (DWORD i = 0; i < n && ok; i++)
				ok = try_acquire(hs[i]);
			if (ok)
				return WAIT_OBJECT_0;
		}
		else
		{
			for (DWORD i = 0; i < n; i++)
				if (try_acquire(hs[i]))
					return WAIT_OBJECT_0 + i;
		}
		if (ms != INFINITE && timeGetTime() - start >= ms)
			return WAIT_TIMEOUT;
		Sleep(1);
	}
}

// ---------------------------------------------------------------- secoes criticas

extern "C" void InitializeCriticalSection(CRITICAL_SECTION *cs)
{
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_t *m = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(m, &attr);
	pthread_mutexattr_destroy(&attr);
	cs->opaque[0] = m;
}
extern "C" void DeleteCriticalSection(CRITICAL_SECTION *cs)
{
	if (cs->opaque[0])
	{
		pthread_mutex_destroy((pthread_mutex_t *)cs->opaque[0]);
		free(cs->opaque[0]);
		cs->opaque[0] = NULL;
	}
}
extern "C" void EnterCriticalSection(CRITICAL_SECTION *cs) { pthread_mutex_lock((pthread_mutex_t *)cs->opaque[0]); }
extern "C" void LeaveCriticalSection(CRITICAL_SECTION *cs) { pthread_mutex_unlock((pthread_mutex_t *)cs->opaque[0]); }

// ---------------------------------------------------------------- arquivos

// O LithTech usa '\'; o path.c do xsdk entende '/' e caminhos relativos (game:\).
static void to_posix(const char *in, char *out, size_t size)
{
	size_t i = 0;
	for (; in && in[i] && i + 1 < size; i++)
		out[i] = (in[i] == '\\') ? '/' : in[i];
	out[i] = 0;
	if (i > 2 && out[0] == '.' && out[1] == '/')
		memmove(out, out + 2, i - 1);
}

extern "C" HANDLE CreateFile(LPCSTR name, DWORD access, DWORD, LPSECURITY_ATTRIBUTES, DWORD disposition, DWORD, HANDLE)
{
	char path[MAX_PATH];
	to_posix(name, path, sizeof path);
	const char *mode = "rb";
	if (access & GENERIC_WRITE)
	{
		if (disposition == OPEN_EXISTING)
			mode = "r+b";
		else if (disposition == OPEN_ALWAYS)
		{
			struct stat st;
			mode = stat(path, &st) == 0 ? "r+b" : "w+b";
		}
		else
			mode = "w+b";
	}
	FILE *fp = fopen(path, mode);
	if (!fp)
	{
		g_LastError = 2;	// ERROR_FILE_NOT_FOUND
		return INVALID_HANDLE_VALUE;
	}
	XFile *f = new XFile;
	f->kind = XH_FILE;
	f->fp = fp;
	return f;
}

extern "C" BOOL ReadFile(HANDLE h, LPVOID buf, DWORD n, LPDWORD got, void *)
{
	XFile *f = (XFile *)h;
	if (!f || f == INVALID_HANDLE_VALUE || f->kind != XH_FILE)
		return FALSE;
	size_t r = fread(buf, 1, n, f->fp);
	if (got)
		*got = (DWORD)r;
	return TRUE;
}

extern "C" BOOL WriteFile(HANDLE h, LPCVOID buf, DWORD n, LPDWORD put, void *)
{
	XFile *f = (XFile *)h;
	if (!f || f == INVALID_HANDLE_VALUE || f->kind != XH_FILE)
		return FALSE;
	size_t w = fwrite(buf, 1, n, f->fp);
	if (put)
		*put = (DWORD)w;
	return w == n;
}

extern "C" DWORD SetFilePointer(HANDLE h, LONG dist, PLONG, DWORD method)
{
	XFile *f = (XFile *)h;
	if (!f || f == INVALID_HANDLE_VALUE || f->kind != XH_FILE)
		return (DWORD)-1;
	int whence = method == FILE_CURRENT ? SEEK_CUR : method == FILE_END ? SEEK_END : SEEK_SET;
	if (fseek(f->fp, dist, whence) != 0)
		return (DWORD)-1;
	return (DWORD)ftell(f->fp);
}

extern "C" DWORD GetFileSize(HANDLE h, LPDWORD high)
{
	XFile *f = (XFile *)h;
	if (high)
		*high = 0;
	if (!f || f == INVALID_HANDLE_VALUE || f->kind != XH_FILE)
		return INVALID_FILE_SIZE;
	long cur = ftell(f->fp);
	fseek(f->fp, 0, SEEK_END);
	long size = ftell(f->fp);
	fseek(f->fp, cur, SEEK_SET);
	return (DWORD)size;
}

extern "C" BOOL CloseHandle(HANDLE h)
{
	XHandle *x = (XHandle *)h;
	if (!x || h == INVALID_HANDLE_VALUE)
		return FALSE;
	if (x->kind == XH_FILE)
	{
		fclose(((XFile *)x)->fp);
		delete (XFile *)x;
	}
	else if (x->kind == XH_EVENT)
	{
		XEvent *e = (XEvent *)x;
		pthread_cond_destroy(&e->cond);
		pthread_mutex_destroy(&e->mutex);
		delete e;
	}
	// Threads: a estrutura fica viva ate o fim (a thread desanexada ainda pode escreve-la).
	return TRUE;
}

extern "C" HFILE OpenFile(LPCSTR name, LPOFSTRUCT of, UINT style)
{
	char path[MAX_PATH];
	to_posix(name, path, sizeof path);
	if (of)
	{
		memset(of, 0, sizeof *of);
		strncpy(of->szPathName, name ? name : "", sizeof of->szPathName - 1);
	}
	struct stat st;
	if (stat(path, &st) != 0)
		return HFILE_ERROR;
	if (style & OF_EXIST)
		return 1;
	FILE *fp = fopen(path, "rb");
	if (!fp)
		return HFILE_ERROR;
	fclose(fp);
	return 1;
}

extern "C" BOOL CreateDirectory(LPCSTR name, LPSECURITY_ATTRIBUTES)
{
	char path[MAX_PATH];
	to_posix(name, path, sizeof path);
	return mkdir(path, 0777) == 0 || errno == EEXIST;
}

extern "C" int _rmdir(const char *name)
{
	char path[MAX_PATH];
	to_posix(name, path, sizeof path);
	return remove(path);	// a libc do xsdk nao tem rmdir
}

static char g_CurDir[MAX_PATH] = "";
extern "C" DWORD GetCurrentDirectory(DWORD size, LPSTR buf)
{
	const char *dir = g_CurDir[0] ? g_CurDir : "game:\\";
	if (buf && size)
	{
		strncpy(buf, dir, size - 1);
		buf[size - 1] = 0;
	}
	return (DWORD)strlen(dir);
}
extern "C" BOOL SetCurrentDirectory(LPCSTR dir)
{
	char path[MAX_PATH];
	to_posix(dir, path, sizeof path);
	if (chdir(path) != 0)
		return FALSE;
	strncpy(g_CurDir, dir, sizeof g_CurDir - 1);
	return TRUE;
}

// O temp do LithTech recebe as copias de object.lto/cshell.dll tiradas do .rez (que o LoadLibrary daqui ignora).
extern "C" DWORD GetTempPath(DWORD size, LPSTR buf)
{
	static const char kTemp[] = "cache:\\";
	if (buf && size > sizeof kTemp)
		strcpy(buf, kTemp);
	return sizeof kTemp - 1;
}
extern "C" UINT GetTempFileName(LPCSTR dir, LPCSTR prefix, UINT unique, LPSTR out)
{
	static UINT counter = 0;
	UINT n = unique ? unique : ++counter;
	sprintf(out, "%s%.3s%04X.tmp", dir ? dir : "", prefix ? prefix : "", n & 0xFFFF);
	return n;
}
extern "C" UINT GetDriveType(LPCSTR) { return DRIVE_FIXED; }

// _findfirst/_findnext (io.h) sobre opendir/fnmatch.
struct XFind
{
	DIR *dir;
	char pattern[MAX_PATH];
	char base[MAX_PATH];
};

// Curinga do Windows (* e ?), sem diferenciar maiusculas. A libc do xsdk nao tem fnmatch.
static bool wild_match(const char *pat, const char *s)
{
	if (*pat == 0)
		return *s == 0;
	if (*pat == '*')
		return wild_match(pat + 1, s) || (*s && wild_match(pat, s + 1));
	if (*s && (*pat == '?' || tolower((unsigned char)*pat) == tolower((unsigned char)*s)))
		return wild_match(pat + 1, s + 1);
	return false;
}

static int find_next(XFind *f, struct _finddata_t *data)
{
	struct dirent *ent;
	while ((ent = readdir(f->dir)) != NULL)
	{
		if (!wild_match(f->pattern, ent->d_name))
			continue;
		char full[MAX_PATH * 2];
		snprintf(full, sizeof full, "%s%s", f->base, ent->d_name);
		struct stat st;
		memset(data, 0, sizeof *data);
		if (stat(full, &st) == 0)
		{
			data->attrib = S_ISDIR(st.st_mode) ? _A_SUBDIR : _A_NORMAL;
			data->size = (unsigned long)st.st_size;
			data->time_write = (long)st.st_mtime;
		}
		strncpy(data->name, ent->d_name, sizeof data->name - 1);
		return 0;
	}
	return -1;
}

extern "C" intptr_t _findfirst(const char *spec, struct _finddata_t *data)
{
	char path[MAX_PATH];
	to_posix(spec, path, sizeof path);
	XFind *f = new XFind;
	const char *slash = strrchr(path, '/');
	if (slash)
	{
		size_t n = (size_t)(slash - path) + 1;
		memcpy(f->base, path, n);
		f->base[n] = 0;
		strcpy(f->pattern, slash + 1);
	}
	else
	{
		f->base[0] = 0;
		strcpy(f->pattern, path);
	}
	if (strcmp(f->pattern, "*.*") == 0)
		strcpy(f->pattern, "*");
	f->dir = opendir(f->base[0] ? f->base : ".");
	if (!f->dir || find_next(f, data) != 0)
	{
		if (f->dir)
			closedir(f->dir);
		delete f;
		return -1;
	}
	return (intptr_t)f;
}
extern "C" int _findnext(intptr_t h, struct _finddata_t *data) { return find_next((XFind *)h, data); }
extern "C" int _findclose(intptr_t h)
{
	XFind *f = (XFind *)h;
	if (!f || h == -1)
		return -1;
	closedir(f->dir);
	delete f;
	return 0;
}

// ---------------------------------------------------------------- .ini

static BOOL ini_read_all(const char *file, char **out)
{
	char path[MAX_PATH];
	to_posix(file, path, sizeof path);
	FILE *fp = fopen(path, "rb");
	if (!fp)
		return FALSE;
	fseek(fp, 0, SEEK_END);
	long n = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	*out = (char *)malloc((size_t)n + 1);
	n = (long)fread(*out, 1, (size_t)n, fp);
	(*out)[n] = 0;
	fclose(fp);
	return TRUE;
}

// Acha "key=" dentro de "[section]"; devolve o inicio e o fim do valor (ou da linha toda com bLine).
static BOOL ini_find(char *text, const char *section, const char *key, char **pStart, char **pEnd, char **pSecEnd)
{
	BOOL inSection = FALSE;
	*pSecEnd = NULL;
	for (char *line = text; line && *line;)
	{
		char *next = strchr(line, '\n');
		char *eol = next ? next : line + strlen(line);
		char *p = line;
		while (p < eol && isspace((unsigned char)*p))
			p++;
		if (*p == '[')
		{
			if (inSection)
			{
				*pSecEnd = line;
				return FALSE;
			}
			char *close = strchr(p, ']');
			inSection = close && close < eol && (size_t)(close - p - 1) == strlen(section) &&
				strncasecmp(p + 1, section, strlen(section)) == 0;
		}
		else if (inSection && key)
		{
			char *eq = (char *)memchr(p, '=', (size_t)(eol - p));
			if (eq)
			{
				char *k = eq;
				while (k > p && isspace((unsigned char)k[-1]))
					k--;
				if ((size_t)(k - p) == strlen(key) && strncasecmp(p, key, strlen(key)) == 0)
				{
					char *v = eq + 1;
					char *e = eol;
					while (e > v && (e[-1] == '\r' || isspace((unsigned char)e[-1])))
						e--;
					*pStart = v;
					*pEnd = e;
					return TRUE;
				}
			}
		}
		line = next ? next + 1 : NULL;
	}
	if (inSection)
		*pSecEnd = text + strlen(text);
	return FALSE;
}

extern "C" DWORD GetPrivateProfileString(LPCSTR section, LPCSTR key, LPCSTR def, LPSTR out, DWORD size, LPCSTR file)
{
	if (!out || !size)
		return 0;
	const char *value = def ? def : "";
	size_t len = strlen(value);
	char *text = NULL, *s, *e, *secEnd;
	if (section && key && ini_read_all(file, &text) && ini_find(text, section, key, &s, &e, &secEnd))
	{
		value = s;
		len = (size_t)(e - s);
	}
	if (len >= size)
		len = size - 1;
	memcpy(out, value, len);
	out[len] = 0;
	free(text);
	return (DWORD)len;
}

extern "C" UINT GetPrivateProfileInt(LPCSTR section, LPCSTR key, INT def, LPCSTR file)
{
	char buf[32];
	if (!GetPrivateProfileString(section, key, "", buf, sizeof buf, file))
		return (UINT)def;
	return (UINT)atoi(buf);
}

extern "C" BOOL WritePrivateProfileString(LPCSTR section, LPCSTR key, LPCSTR value, LPCSTR file)
{
	if (!section)
		return FALSE;
	char *text = NULL, *s = NULL, *e = NULL, *secEnd = NULL;
	if (!ini_read_all(file, &text))
		text = strdup("");
	char path[MAX_PATH];
	to_posix(file, path, sizeof path);
	FILE *fp = fopen(path, "wb");
	if (!fp)
	{
		free(text);
		return FALSE;
	}
	BOOL found = key && ini_find(text, section, key, &s, &e, &secEnd);
	if (found)
	{
		fwrite(text, 1, (size_t)(s - text), fp);
		if (value)
			fputs(value, fp);
		fputs(e, fp);
	}
	else if (key && value)
	{
		if (secEnd)
		{
			fwrite(text, 1, (size_t)(secEnd - text), fp);
			fprintf(fp, "%s=%s\r\n", key, value);
			fputs(secEnd, fp);
		}
		else
		{
			fputs(text, fp);
			fprintf(fp, "\r\n[%s]\r\n%s=%s\r\n", section, key, value);
		}
	}
	else
	{
		fputs(text, fp);
	}
	fclose(fp);
	free(text);
	return TRUE;
}

// ---------------------------------------------------------------- modulos

// Exportacoes dos modulos linkados no .xex (prefixadas pelo objcopy, ver avp2_module no CMakeLists.txt).
extern "C" {
void cshell_GetClientShellFunctions(void *, void *);
int cshell_GetClientShellVersion(void);
void cshell_SetInstanceHandle(void *);
void object_GetServerShellFunctions(void *, void *);
int object_GetServerShellVersion(void);
void object_SetInstanceHandle(void *);
void *object_ObjectDLLSetup(int *, void *, int *);
void d3dren_RenderDLLSetup(void *);
void *d3dren_GetSupportedModes(void);
void d3dren_FreeModeList(void *);
}

struct XExport
{
	const char *name;
	FARPROC fn;
};

static const XExport kCShellExports[] = {
	{ "GetClientShellFunctions", (FARPROC)cshell_GetClientShellFunctions },
	{ "GetClientShellVersion", (FARPROC)cshell_GetClientShellVersion },
	{ "SetInstanceHandle", (FARPROC)cshell_SetInstanceHandle },
	{ NULL, NULL },
};
static const XExport kObjectExports[] = {
	{ "GetServerShellFunctions", (FARPROC)object_GetServerShellFunctions },
	{ "GetServerShellVersion", (FARPROC)object_GetServerShellVersion },
	{ "SetInstanceHandle", (FARPROC)object_SetInstanceHandle },
	{ "ObjectDLLSetup", (FARPROC)object_ObjectDLLSetup },
	{ NULL, NULL },
};
static const XExport kRenderExports[] = {
	{ "RenderDLLSetup", (FARPROC)d3dren_RenderDLLSetup },
	{ "GetSupportedModes", (FARPROC)d3dren_GetSupportedModes },
	{ "FreeModeList", (FARPROC)d3dren_FreeModeList },
	{ NULL, NULL },
};

struct XModule : XHandle
{
	const XExport *exports;	// modulo linkado
	unsigned char *image;	// DLL lida do disco (so recursos)
	long size;
	char name[64];
};

static const char *base_name(const char *path)
{
	const char *b = path;
	for (const char *p = path; *p; p++)
		if (*p == '\\' || *p == '/' || *p == ':')
			b = p + 1;
	return b;
}

extern "C" HMODULE LoadLibrary(LPCSTR name)
{
	if (!name)
		return NULL;
	const char *base = base_name(name);
	XModule *m = new XModule;
	m->kind = XH_MODULE;
	m->exports = NULL;
	m->image = NULL;
	m->size = 0;
	strncpy(m->name, base, sizeof m->name - 1);
	m->name[sizeof m->name - 1] = 0;

	if (strcasecmp(base, "cshell.dll") == 0)
	{
		m->exports = kCShellExports;
		return (HMODULE)m;
	}
	if (strcasecmp(base, "object.lto") == 0)
	{
		m->exports = kObjectExports;
		return (HMODULE)m;
	}
	if (strcasecmp(base, "d3d.ren") == 0)
	{
		m->exports = kRenderExports;
		return (HMODULE)m;
	}

	// Outras DLLs: so os recursos (ltmsg.dll, cres.dll...). Primeiro o caminho dado, depois o nome na pasta do jogo.
	char path[MAX_PATH];
	to_posix(name, path, sizeof path);
	FILE *fp = fopen(path, "rb");
	if (!fp)
		fp = fopen(base, "rb");
	if (!fp)
	{
		delete m;
		g_LastError = 126;	// ERROR_MOD_NOT_FOUND
		return NULL;
	}
	fseek(fp, 0, SEEK_END);
	m->size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	m->image = (unsigned char *)malloc((size_t)m->size);
	if (!m->image || fread(m->image, 1, (size_t)m->size, fp) != (size_t)m->size)
	{
		fclose(fp);
		free(m->image);
		delete m;
		return NULL;
	}
	fclose(fp);
	return (HMODULE)m;
}

extern "C" FARPROC GetProcAddress(HMODULE h, LPCSTR name)
{
	XModule *m = (XModule *)h;
	if (!m || m->kind != XH_MODULE || !m->exports || !name)
		return NULL;
	for (const XExport *e = m->exports; e->name; e++)
		if (strcmp(e->name, name) == 0)
			return e->fn;
	return NULL;
}

extern "C" BOOL FreeLibrary(HMODULE h)
{
	XModule *m = (XModule *)h;
	if (!m || m->kind != XH_MODULE)
		return FALSE;
	free(m->image);
	delete m;
	return TRUE;
}

// ---- recursos de um PE x86 (little-endian)

static uint32_t le32(const unsigned char *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t le16(const unsigned char *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

struct PEView
{
	const unsigned char *img;
	long size;
	uint32_t rsrcRva, rsrcOff;	// secao de recursos
	uint32_t secRva, secRaw, secSize;
};

static BOOL pe_open(XModule *m, PEView *v)
{
	if (!m || m->kind != XH_MODULE || !m->image || m->size < 0x40)
		return FALSE;
	const unsigned char *img = m->image;
	uint32_t pe = le32(img + 0x3C);
	if ((long)pe + 0xF8 > m->size || le32(img + pe) != 0x00004550)
		return FALSE;
	uint16_t nSections = le16(img + pe + 6);
	uint16_t optSize = le16(img + pe + 20);
	const unsigned char *opt = img + pe + 24;
	uint32_t rsrcRva = le32(opt + 96 + 2 * 8);	// DataDirectory[2] (PE32)
	const unsigned char *sec = opt + optSize;
	for (int i = 0; i < nSections; i++, sec += 40)
	{
		uint32_t va = le32(sec + 12), vsize = le32(sec + 8), raw = le32(sec + 20), rawSize = le32(sec + 16);
		uint32_t span = vsize > rawSize ? vsize : rawSize;
		if (rsrcRva >= va && rsrcRva < va + span)
		{
			v->img = img;
			v->size = m->size;
			v->rsrcRva = rsrcRva;
			v->rsrcOff = raw + (rsrcRva - va);
			v->secRva = va;
			v->secRaw = raw;
			v->secSize = rawSize;
			return TRUE;
		}
	}
	return FALSE;
}

static const unsigned char *pe_rva(const PEView *v, uint32_t rva)
{
	if (rva < v->secRva || rva - v->secRva >= v->secSize)
		return NULL;
	uint32_t off = v->secRaw + (rva - v->secRva);
	return (long)off < v->size ? v->img + off : NULL;
}

// Procura uma entrada num diretorio de recursos por id (ou nome, se pName) ou a primeira (id < 0).
static BOOL pe_dir_find(const PEView *v, uint32_t dirOff, int id, const char *pName, uint32_t *pOut)
{
	const unsigned char *dir = v->img + v->rsrcOff + dirOff;
	if ((long)(v->rsrcOff + dirOff + 16) > v->size)
		return FALSE;
	int nNamed = le16(dir + 12), nId = le16(dir + 14);
	const unsigned char *ent = dir + 16;
	for (int i = 0; i < nNamed + nId; i++, ent += 8)
	{
		uint32_t nameField = le32(ent), target = le32(ent + 4);
		BOOL match = FALSE;
		if (id < 0 && !pName)
			match = TRUE;
		else if (pName && (nameField & 0x80000000u))
		{
			const unsigned char *s = v->img + v->rsrcOff + (nameField & 0x7FFFFFFFu);
			int len = le16(s);
			match = (int)strlen(pName) == len;
			for (int c = 0; match && c < len; c++)
				match = toupper(le16(s + 2 + 2 * c)) == toupper((unsigned char)pName[c]);
		}
		else if (!pName && !(nameField & 0x80000000u))
		{
			match = (int)nameField == id;
		}
		if (match)
		{
			*pOut = target;
			return TRUE;
		}
	}
	return FALSE;
}

// type/name como no Win32: inteiro (MAKEINTRESOURCE) ou texto ("#123" tambem vale).
static BOOL pe_find(const PEView *v, LPCSTR type, LPCSTR name, const unsigned char **pData, uint32_t *pSize)
{
	uint32_t t, n, l;
	int typeId = -1, nameId = -1;
	const char *typeName = NULL, *nameName = NULL;
	if ((uintptr_t)type < 0x10000)
		typeId = (int)(uintptr_t)type;
	else if (type[0] == '#')
		typeId = atoi(type + 1);
	else
		typeName = type;
	if ((uintptr_t)name < 0x10000)
		nameId = (int)(uintptr_t)name;
	else if (name[0] == '#')
		nameId = atoi(name + 1);
	else
		nameName = name;

	if (!pe_dir_find(v, 0, typeId, typeName, &t) || !(t & 0x80000000u))
		return FALSE;
	if (!pe_dir_find(v, t & 0x7FFFFFFFu, nameId, nameName, &n) || !(n & 0x80000000u))
		return FALSE;
	if (!pe_dir_find(v, n & 0x7FFFFFFFu, -1, NULL, &l) || (l & 0x80000000u))
		return FALSE;
	const unsigned char *entry = v->img + v->rsrcOff + l;
	const unsigned char *data = pe_rva(v, le32(entry));
	if (!data)
		return FALSE;
	*pData = data;
	*pSize = le32(entry + 4);
	return TRUE;
}

struct XResource
{
	const unsigned char *data;
	uint32_t size;
};

extern "C" HRSRC FindResource(HMODULE h, LPCSTR name, LPCSTR type)
{
	PEView v;
	XResource r;
	if (!pe_open((XModule *)h, &v) || !pe_find(&v, type, name, &r.data, &r.size))
		return NULL;
	XResource *res = new XResource(r);	// vive enquanto o modulo (como no Win32, sem FreeResource)
	return (HRSRC)res;
}
extern "C" HGLOBAL LoadResource(HMODULE, HRSRC r) { return (HGLOBAL)r; }
extern "C" LPVOID LockResource(HGLOBAL r) { return r ? (LPVOID)((XResource *)r)->data : NULL; }
extern "C" DWORD SizeofResource(HMODULE, HRSRC r) { return r ? ((XResource *)r)->size : 0; }

// Tabelas de strings: o bloco (id/16)+1 tem 16 strings UTF-16 com prefixo de tamanho. Os textos do AvP2 sao
// Latin-1; caracteres fora dele viram '?'.
extern "C" int LoadString(HINSTANCE h, UINT id, LPSTR buf, int size)
{
	if (!buf || size <= 0)
		return 0;
	buf[0] = 0;
	PEView v;
	const unsigned char *data;
	uint32_t dataSize;
	if (!pe_open((XModule *)h, &v) || !pe_find(&v, RT_STRING, MAKEINTRESOURCE((id >> 4) + 1), &data, &dataSize))
		return 0;
	const unsigned char *p = data, *end = data + dataSize;
	for (UINT i = 0; i < (id & 15) && p + 2 <= end; i++)
		p += 2 + 2 * le16(p);
	if (p + 2 > end)
		return 0;
	int len = le16(p);
	p += 2;
	int n = 0;
	for (; n < len && n < size - 1 && p + 2 * n + 2 <= end; n++)
	{
		uint16_t c = le16(p + 2 * n);
		buf[n] = (char)(c < 256 ? c : '?');
	}
	buf[n] = 0;
	return n;
}

// ---------------------------------------------------------------- processo

extern "C" BOOL CreateProcess(LPCSTR, LPSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID, LPCSTR,
	LPSTARTUPINFO, LPPROCESS_INFORMATION)
{
	return FALSE;
}

extern "C" void avp360_return_to_menu(void)
{
	xsdk_launch("game:\\AvP360.xex", NULL, 0);
}

// ---------------------------------------------------------------- mensagens, janela, GDI, teclado

static BOOL g_QuitPosted = FALSE;
static int g_QuitCode = 0;
extern "C" void PostQuitMessage(int code)
{
	g_QuitPosted = TRUE;
	g_QuitCode = code;
}
extern "C" BOOL PeekMessage(LPMSG msg, HWND, UINT, UINT, UINT remove)
{
	if (!g_QuitPosted)
		return FALSE;
	memset(msg, 0, sizeof *msg);
	msg->message = WM_QUIT;
	msg->wParam = (WPARAM)g_QuitCode;
	if (remove & PM_REMOVE)
		g_QuitPosted = FALSE;
	return TRUE;
}
extern "C" BOOL TranslateMessage(const MSG *) { return FALSE; }
extern "C" LRESULT DispatchMessage(const MSG *) { return 0; }
extern "C" LRESULT CallWindowProc(WNDPROC fn, HWND hwnd, UINT msg, WPARAM w, LPARAM l)
{
	return fn ? fn(hwnd, msg, w, l) : 0;
}

#define XBOX_SCREEN_W 1280
#define XBOX_SCREEN_H 720
static LONG g_WndLong[32];
extern "C" LONG GetWindowLong(HWND, int i) { return g_WndLong[(i + 32) & 31]; }
extern "C" LONG SetWindowLong(HWND, int i, LONG v)
{
	LONG old = g_WndLong[(i + 32) & 31];
	g_WndLong[(i + 32) & 31] = v;
	return old;
}
extern "C" BOOL SetWindowPos(HWND, HWND, int, int, int, int, UINT) { return TRUE; }
extern "C" BOOL GetWindowRect(HWND, LPRECT r)
{
	r->left = r->top = 0;
	r->right = XBOX_SCREEN_W;
	r->bottom = XBOX_SCREEN_H;
	return TRUE;
}
extern "C" BOOL GetClientRect(HWND h, LPRECT r) { return GetWindowRect(h, r); }
extern "C" int MapWindowPoints(HWND, HWND, LPPOINT, UINT) { return 0; }
extern "C" int GetWindowText(HWND, LPSTR buf, int size)
{
	if (buf && size > 0)
		buf[0] = 0;
	return 0;
}
extern "C" BOOL ShowWindow(HWND, int) { return TRUE; }
extern "C" BOOL DestroyWindow(HWND) { return TRUE; }
extern "C" HWND FindWindow(LPCSTR, LPCSTR) { return NULL; }
extern "C" HWND GetFocus(void) { return (HWND)1; }
extern "C" HWND SetFocus(HWND h) { return h; }
extern "C" HWND GetForegroundWindow(void) { return (HWND)1; }
extern "C" BOOL IsIconic(HWND) { return FALSE; }
extern "C" HWND GetDesktopWindow(void) { return (HWND)1; }
extern "C" BOOL ClientToScreen(HWND, LPPOINT) { return TRUE; }
extern "C" BOOL AdjustWindowRect(LPRECT, DWORD, BOOL) { return TRUE; }
// Sem ddraw.dll: o d3d.ren cai no DirectDrawEnumerate, que e o do d3d7_xenos.cpp.
extern "C" HMODULE GetModuleHandle(LPCSTR) { return NULL; }

extern "C" HDC GetDC(HWND) { return (HDC)1; }
extern "C" int ReleaseDC(HWND, HDC) { return 1; }
extern "C" BOOL DeleteObject(HGDIOBJ) { return TRUE; }

extern "C" BOOL ClipCursor(const RECT *) { return TRUE; }
extern "C" BOOL ShowCursor(BOOL show) { return show ? 0 : -1; }
extern "C" HCURSOR SetCursor(HCURSOR) { return NULL; }
extern "C" HANDLE LoadImage(HINSTANCE, LPCSTR, UINT, int, int, UINT) { return NULL; }
extern "C" BOOL SetCursorPos(int, int) { return TRUE; }
extern "C" BOOL GetCursorPos(LPPOINT p)
{
	p->x = XBOX_SCREEN_W / 2;
	p->y = XBOX_SCREEN_H / 2;
	return TRUE;
}

// Nao ha teclado do Windows: o controle entra pelo gerenciador de entrada do motor (input_xbox.cpp).
extern "C" SHORT GetAsyncKeyState(int) { return 0; }
extern "C" SHORT GetKeyState(int) { return 0; }
extern "C" BOOL GetKeyboardState(LPBYTE keys)
{
	memset(keys, 0, 256);
	return TRUE;
}
extern "C" HKL GetKeyboardLayout(DWORD) { return NULL; }
extern "C" UINT MapVirtualKeyEx(UINT, UINT, HKL) { return 0; }
extern "C" int ToAscii(UINT, UINT, const BYTE *, LPWORD, UINT) { return 0; }

// ---------------------------------------------------------------- CRT do MSVC

extern "C" char *_strupr(char *s)
{
	for (char *p = s; p && *p; p++)
		*p = (char)toupper((unsigned char)*p);
	return s;
}
extern "C" char *_strlwr(char *s)
{
	for (char *p = s; p && *p; p++)
		*p = (char)tolower((unsigned char)*p);
	return s;
}
extern "C" char *_ltoa(long v, char *buf, int radix)
{
	char tmp[40];
	int i = 0;
	unsigned long u = (radix == 10 && v < 0) ? (unsigned long)(-v) : (unsigned long)v;
	do
	{
		int d = (int)(u % (unsigned)radix);
		tmp[i++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
		u /= (unsigned)radix;
	} while (u);
	char *p = buf;
	if (radix == 10 && v < 0)
		*p++ = '-';
	while (i)
		*p++ = tmp[--i];
	*p = 0;
	return buf;
}
extern "C" char *_itoa(int v, char *buf, int radix) { return _ltoa(v, buf, radix); }

extern "C" void _splitpath(const char *path, char *drive, char *dir, char *fname, char *ext)
{
	const char *p = path;
	const char *colon = strchr(p, ':');
	if (drive)
	{
		drive[0] = 0;
		if (colon)
		{
			size_t n = (size_t)(colon - p) + 1;
			memcpy(drive, p, n);
			drive[n] = 0;
		}
	}
	if (colon)
		p = colon + 1;
	const char *lastSlash = NULL;
	for (const char *q = p; *q; q++)
		if (*q == '\\' || *q == '/')
			lastSlash = q;
	const char *name = lastSlash ? lastSlash + 1 : p;
	if (dir)
	{
		size_t n = (size_t)(name - p);
		memcpy(dir, p, n);
		dir[n] = 0;
	}
	const char *dot = strrchr(name, '.');
	if (!dot)
		dot = name + strlen(name);
	if (fname)
	{
		size_t n = (size_t)(dot - name);
		memcpy(fname, name, n);
		fname[n] = 0;
	}
	if (ext)
		strcpy(ext, dot);
}
