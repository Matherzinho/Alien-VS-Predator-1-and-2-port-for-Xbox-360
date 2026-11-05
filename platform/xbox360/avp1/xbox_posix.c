/* avp360: o que files.c pede do POSIX e o newlib do xsdk nao tem. O FATX do console nao diferencia maiusculas. */
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

int ogl_use_multisample_filter_hint = 0;
int ogl_use_texture_filter_anisotropic = 1;

int access(const char *path, int mode)
{
	struct stat st;
	(void)mode;
	if (stat(path, &st) == 0) return 0;
	errno = ENOENT;
	return -1;
}

char *realpath(const char *path, char *resolved)
{
	static char buf[260];
	char *out = resolved ? resolved : buf;
	snprintf(out, 260, "%s", path);
	return out;
}

/* Glob simples (* e ?), sem diferenciar maiusculas, como os nomes do FATX. */
static int glob_match(const char *p, const char *s)
{
	for (; *p; p++, s++) {
		if (*p == '*') {
			while (p[1] == '*') p++;
			if (!p[1]) return 1;
			for (; *s; s++)
				if (glob_match(p + 1, s)) return 1;
			return 0;
		}
		if (!*s) return 0;
		if (*p != '?' && tolower((unsigned char)*p) != tolower((unsigned char)*s)) return 0;
	}
	return *s == 0;
}

int fnmatch(const char *pattern, const char *string, int flags)
{
	(void)flags;
	return glob_match(pattern, string) ? 0 : 1;
}
