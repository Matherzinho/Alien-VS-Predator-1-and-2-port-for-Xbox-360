/* avp360: le o controle (ver xpad.h). */
#include <string.h>
#include <xsdk/kernel.h>
#include "xpad.h"

void xpad_read(XPadState *out)
{
	memset(out, 0, sizeof *out);
	for (uint32_t user = 0; user < 4; user++) {
		INPUT_STATE s;
		memset(&s, 0, sizeof s);
		if (XamInputGetState(user, 0, &s) != 0)
			continue;
		out->connected = 1;
		out->buttons = s.gamepad.buttons;
		out->lt = s.gamepad.left_trigger;
		out->rt = s.gamepad.right_trigger;
		out->lx = s.gamepad.thumb_lx;
		out->ly = s.gamepad.thumb_ly;
		out->rx = s.gamepad.thumb_rx;
		out->ry = s.gamepad.thumb_ry;
		return;
	}
}
