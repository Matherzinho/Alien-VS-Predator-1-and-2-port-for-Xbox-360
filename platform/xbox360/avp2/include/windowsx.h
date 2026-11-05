/* avp360: windowsx.h. So as macros de mensagem que o AvP2 usa; no console nao ha janela. */
#ifndef AVP360_WINDOWSX_H
#define AVP360_WINDOWSX_H
#include "windows.h"
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#define HANDLE_MSG(hwnd, message, fn) case (message): return 0;
#endif
