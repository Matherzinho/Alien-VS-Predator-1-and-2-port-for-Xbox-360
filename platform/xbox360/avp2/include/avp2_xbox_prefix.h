/* avp360: incluido antes de todo arquivo do AvP2 (-include). Ajustes de plataforma para xsdk/PowerPC. */
#ifndef AVP2_XBOX_PREFIX_H
#define AVP2_XBOX_PREFIX_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

/* Convencoes de chamada e extensoes do MSVC que nao fazem sentido no PowerPC. */
#define __cdecl
#define __stdcall
#define __fastcall
#define _cdecl
#define _stdcall
#define __forceinline inline
#define __declspec(x)
#define __int64 long long
#define __int32 int
#define __int16 short
#define __int8 char

/* No MSVC isto vem de stdlib.h/string.h/math.h, que o LithTech inclui sem windows.h; aqui vem tudo de uma vez. */
#include "windows.h"
#include "winsock2.h"
#include "objbase.h"
#include <math.h>
#define _isnan isnan
#define _finite isfinite
#define _hypot hypot
#ifdef __cplusplus
#include <algorithm>
using std::min;
using std::max;
/* O VC6 deixava passar um temporario para um parametro T& (e const_cast de temporario). AVP360_LV faz o mesmo:
 * o temporario vive ate o fim da expressao inteira, como no VC6. */
template <class T> inline T &AVP360_LV(const T &t) { return const_cast<T &>(t); }
#endif
#ifdef __cplusplus
extern "C" {
#endif
extern int __argc;
extern char **__argv;
#ifdef __cplusplus
}
#endif

#endif
