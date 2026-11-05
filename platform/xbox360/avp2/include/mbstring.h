/* avp360: mbstring.h. O AvP2 usa so texto de um byte (Windows-1252). */
#ifndef AVP360_MBSTRING_H
#define AVP360_MBSTRING_H
#include "windows.h"
#define _mbstrlen(s) strlen((const char *)(s))
#define _mbslen(s) strlen((const char *)(s))
#define _mbsncpy(d, s, n) ((unsigned char *)strncpy((char *)(d), (const char *)(s), (n)))
#define _mbscpy(d, s) ((unsigned char *)strcpy((char *)(d), (const char *)(s)))
#define _mbsicmp(a, b) strcasecmp((const char *)(a), (const char *)(b))
#define _mbscmp(a, b) strcmp((const char *)(a), (const char *)(b))
#define _mbsinc(p) ((p) + 1)
#define _mbsnbcnt(s, n) (n)
#define _mbsnbcmp(a, b, n) strncmp((const char *)(a), (const char *)(b), (n))
#define _mbsnbicmp(a, b, n) strncasecmp((const char *)(a), (const char *)(b), (n))
#define _mbsnbcpy(d, s, n) ((unsigned char *)strncpy((char *)(d), (const char *)(s), (n)))
#define _mbschr(s, c) ((unsigned char *)strchr((const char *)(s), (c)))
#define _mbsrchr(s, c) ((unsigned char *)strrchr((const char *)(s), (c)))


#define _tcsninc(p, n) ((p) + (n))
#endif
