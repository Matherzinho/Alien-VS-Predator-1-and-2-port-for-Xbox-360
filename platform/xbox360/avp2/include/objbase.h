/* avp360: o COM que o LithTech chama (CoInitialize para o DirectMusic). Sem COM no console: tudo devolve S_OK. */
#ifndef AVP360_OBJBASE_H
#define AVP360_OBJBASE_H
#include "windows.h"
#ifdef __cplusplus
extern "C" {
#endif
HRESULT CoInitialize(LPVOID);
HRESULT CoInitializeEx(LPVOID, DWORD);
void CoUninitialize(void);
#define COINIT_MULTITHREADED 0
#define COINIT_APARTMENTTHREADED 2
#ifdef __cplusplus
}
#endif
#endif
