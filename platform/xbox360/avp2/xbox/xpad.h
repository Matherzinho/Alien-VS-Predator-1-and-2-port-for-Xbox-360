/* avp360: o controle do Xbox 360 para o AvP2 (xpad.c le com XamInputGetState; o kernel.h do xsdk nao convive com o
 * windows.h do shim, entao a interface so usa tipos do C). */
#ifndef AVP360_XPAD_H
#define AVP360_XPAD_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	XPAD_UP = 0x0001, XPAD_DOWN = 0x0002, XPAD_LEFT = 0x0004, XPAD_RIGHT = 0x0008,
	XPAD_START = 0x0010, XPAD_BACK = 0x0020, XPAD_LTHUMB = 0x0040, XPAD_RTHUMB = 0x0080,
	XPAD_LB = 0x0100, XPAD_RB = 0x0200, XPAD_A = 0x1000, XPAD_B = 0x2000, XPAD_X = 0x4000, XPAD_Y = 0x8000,
};

typedef struct {
	int connected;
	uint16_t buttons;
	uint8_t lt, rt;
	int16_t lx, ly, rx, ry;	/* y para cima e positivo */
} XPadState;

/* O primeiro controle conectado (0..3). */
void xpad_read(XPadState *state);

#ifdef __cplusplus
}
#endif
#endif
