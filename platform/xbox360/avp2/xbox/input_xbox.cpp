// avp360: o lugar de kernel/sys/win/input.cpp (DirectInput) e net/sys/win/udpdriver.cpp (rede).
//
// Entrada: o controle do Xbox 360 aciona as acoes do jogo pelo nome (as do "AddAction Nome codigo" do autoexec.cfg),
// sem passar por teclas: assim o mapeamento nao depende dos binds de teclado do PC, que nem trazem todas as acoes.
// O analogico direito vira os eixos do mouse (Axis1/Axis2, codigos -1 e -2: os axisOffsets do motor).
// Nos menus (o jogo desliga a entrada: SetInputState(FALSE) na pausa e antes de um mapa) o controle vira teclas para o
// OnKeyDown do jogo: direcional/analogico = setas, A = Enter, B = Esc. Start e sempre Esc (abre e fecha o menu).
//
//   Analogico esquerdo: andar/desviar     Analogico direito: mirar     RT: atirar     LT: tiro alternativo
//   A: pular   B: agachar   X: usar   Y: proxima arma   RB: recarregar   LB: proxima visao
//   Direcional: cima arma anterior, baixo lanterna/sinalizador, esquerda MediComp, direita energia (Predador)
//   L3: andar nas paredes (Alien)   R3: zoom   Back: placar
//
// Rede: so o driver "local" (jogo de um jogador); udp_CreateDriver devolve nulo e o CNetMgr segue sem ele.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "bdefs.h"
#include "input.h"
#include "dsys_interface.h"
#include "xpad.h"

class CBaseDriver;
CBaseDriver *udp_CreateDriver() { return LTNULL; }

// ---------------------------------------------------------------- estado

struct XAction
{
	char name[48];
	int code;
};

static XAction g_Actions[256];
static int g_nActions;
static LTBOOL g_bInputInitted = LTFALSE;
static XPadState g_Pad;
static bool g_bMenu = true;
static DWORD g_LastReadMS;

// Mira, por segundo com o analogico no fim do curso. O jogo (Shared/PlayerMovement.cpp) gira offset * 90 graus e
// inclina offset * 45 graus: 1.7 = ~150 graus/s de giro, 2.0 = 90 graus/s de inclinacao.
static const float kYawSpeed = 1.7f, kPitchSpeed = 2.0f;
static const float kDeadZone = 7849.0f;	// XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE

// Os codigos do autoexec.cfg do AvP2, para quando ele ainda nao existe (o jogo o escreve ao sair).
static const XAction kDefaultActions[] = {
	{ "Forward", 0 }, { "Backward", 1 }, { "StrafeLeft", 5 }, { "StrafeRight", 6 }, { "Duck", 8 }, { "Jump", 9 },
	{ "Fire", 14 }, { "AltFire", 15 }, { "Reload", 16 }, { "Activate", 17 }, { "PrevWeapon", 18 },
	{ "NextWeapon", 19 }, { "ZoomIn", 20 }, { "TorchSelect", 24 }, { "WallWalkToggle", 26 }, { "EnergySift", 28 },
	{ "MediComp", 29 }, { "NextVision", 51 }, { "ScoreDisplay", 72 },
};

static int action_code(const char *name)
{
	for (int i = 0; i < g_nActions; i++)
		if (stricmp(g_Actions[i].name, name) == 0)
			return g_Actions[i].code;
	for (size_t i = 0; i < sizeof kDefaultActions / sizeof kDefaultActions[0]; i++)
		if (stricmp(kDefaultActions[i].name, name) == 0)
			return kDefaultActions[i].code;
	return 0x7fffffff;
}

static float stick(int16_t v)
{
	float f = (float)v;
	float s = f < 0 ? -1.0f : 1.0f;
	f = fabsf(f);
	if (f < kDeadZone)
		return 0.0f;
	f = (f - kDeadZone) / (32767.0f - kDeadZone);
	if (f > 1.0f)
		f = 1.0f;
	return s * f * f;	// curva quadratica: precisao perto do centro
}

// ---------------------------------------------------------------- teclas (menus)

static void push_key(DWORD vk, bool down)
{
	ClientGlob *g = &g_ClientGlob;
	if (down)
	{
		if (g->m_nKeyDowns < MAX_KEYBUFFER)
		{
			g->m_KeyDowns[g->m_nKeyDowns] = vk;
			g->m_KeyDownReps[g->m_nKeyDowns] = 1;
			g->m_nKeyDowns++;
		}
	}
	else if (g->m_nKeyUps < MAX_KEYBUFFER)
	{
		g->m_KeyUps[g->m_nKeyUps++] = vk;
	}
}

// Uma tecla do controle: aperto, soltura e repeticao (como o teclado: 400 ms, depois a cada 90 ms).
struct XKey
{
	DWORD vk;
	bool down;
	DWORD next;
};

static void key_state(XKey *k, bool on, bool repeat, DWORD now)
{
	if (on && !k->down)
	{
		push_key(k->vk, true);
		k->next = now + 400;
	}
	else if (on && repeat && now >= k->next)
	{
		push_key(k->vk, true);
		k->next = now + 90;
	}
	else if (!on && k->down)
	{
		push_key(k->vk, false);
	}
	k->down = on;
}

// Chamado pelo laco principal (client_xbox.cpp) uma vez por quadro, antes do CClientMgr::Update.
void avp360_input_frame(bool bMenu)
{
	static XKey up = { VK_UP }, down = { VK_DOWN }, left = { VK_LEFT }, right = { VK_RIGHT };
	static XKey enter = { VK_RETURN }, back = { VK_ESCAPE }, start = { VK_ESCAPE };
	const DWORD now = timeGetTime();

	xpad_read(&g_Pad);
	g_bMenu = bMenu;

	const uint16_t b = g_Pad.connected ? g_Pad.buttons : 0;
	key_state(&start, (b & XPAD_START) != 0, false, now);

	const bool m = bMenu && g_Pad.connected;
	key_state(&up, m && ((b & XPAD_UP) || g_Pad.ly > 20000), true, now);
	key_state(&down, m && ((b & XPAD_DOWN) || g_Pad.ly < -20000), true, now);
	key_state(&left, m && ((b & XPAD_LEFT) || g_Pad.lx < -20000), true, now);
	key_state(&right, m && ((b & XPAD_RIGHT) || g_Pad.lx > 20000), true, now);
	key_state(&enter, m && (b & XPAD_A), false, now);
	key_state(&back, m && (b & XPAD_B), false, now);
}

// ---------------------------------------------------------------- InputMgr

static LTBOOL xi_Init(InputMgr *, ConsoleState *) { g_bInputInitted = LTTRUE; return LTTRUE; }
static void xi_Term(InputMgr *) { g_bInputInitted = LTFALSE; }
static LTBOOL xi_IsInitted(InputMgr *) { return g_bInputInitted; }
static void xi_ListDevices(InputMgr *) { dsi_ConsolePrint("Xbox 360 Controller"); }
static long xi_PlayJoystickEffect(InputMgr *, char *, float, float) { return 0; }

static void xi_ReadInput(InputMgr *, uint8 *pActionsOn, float axisOffsets[3])
{
	axisOffsets[0] = axisOffsets[1] = axisOffsets[2] = 0.0f;
	const DWORD now = timeGetTime();
	float dt = g_LastReadMS ? (now - g_LastReadMS) / 1000.0f : 0.0f;
	g_LastReadMS = now;
	if (dt > 0.1f)
		dt = 0.1f;
	if (g_bMenu || !g_Pad.connected)
		return;

	const uint16_t b = g_Pad.buttons;
	struct { const char *action; bool on; } map[] = {
		{ "Forward", g_Pad.ly > 10000 },
		{ "Backward", g_Pad.ly < -10000 },
		{ "StrafeLeft", g_Pad.lx < -10000 },
		{ "StrafeRight", g_Pad.lx > 10000 },
		{ "Fire", g_Pad.rt > 64 },
		{ "AltFire", g_Pad.lt > 64 },
		{ "Jump", (b & XPAD_A) != 0 },
		{ "Duck", (b & XPAD_B) != 0 },
		{ "Activate", (b & XPAD_X) != 0 },
		{ "NextWeapon", (b & XPAD_Y) != 0 },
		{ "Reload", (b & XPAD_RB) != 0 },
		{ "NextVision", (b & XPAD_LB) != 0 },
		{ "PrevWeapon", (b & XPAD_UP) != 0 },
		{ "TorchSelect", (b & XPAD_DOWN) != 0 },
		{ "MediComp", (b & XPAD_LEFT) != 0 },
		{ "EnergySift", (b & XPAD_RIGHT) != 0 },
		{ "WallWalkToggle", (b & XPAD_LTHUMB) != 0 },
		{ "ZoomIn", (b & XPAD_RTHUMB) != 0 },
		{ "ScoreDisplay", (b & XPAD_BACK) != 0 },
	};
	for (size_t i = 0; i < sizeof map / sizeof map[0]; i++)
	{
		if (!map[i].on)
			continue;
		int code = action_code(map[i].action);
		if (code >= 0 && code < 256)
			pActionsOn[code] |= 1;
	}

	// Mira: como o mouse (Axis1 = x -> axisOffsets[0], Axis2 = y -> [1]; y do mouse cresce para baixo).
	axisOffsets[0] = stick(g_Pad.rx) * kYawSpeed * dt;
	axisOffsets[1] = -stick(g_Pad.ry) * kPitchSpeed * dt;
}

static LTBOOL xi_FlushInputBuffers(InputMgr *) { return LTTRUE; }
static LTRESULT xi_ClearInput() { return LT_OK; }

static void xi_AddAction(InputMgr *, char *pActionName, int actionCode)
{
	if (!pActionName)
		return;
	for (int i = 0; i < g_nActions; i++)
	{
		if (stricmp(g_Actions[i].name, pActionName) == 0)
		{
			g_Actions[i].code = actionCode;
			return;
		}
	}
	if (g_nActions < (int)(sizeof g_Actions / sizeof g_Actions[0]))
	{
		strncpy(g_Actions[g_nActions].name, pActionName, sizeof g_Actions[0].name - 1);
		g_Actions[g_nActions].name[sizeof g_Actions[0].name - 1] = 0;
		g_Actions[g_nActions].code = actionCode;
		g_nActions++;
	}
}

// Os binds de teclado/mouse/joystick do PC sao aceitos e ignorados: o controle usa o mapeamento acima.
static LTBOOL xi_EnableDevice(InputMgr *, char *) { return LTTRUE; }
static LTBOOL xi_ClearBindings(InputMgr *, char *, char *) { return LTTRUE; }
static LTBOOL xi_AddBinding(InputMgr *, char *, char *, char *, float, float) { return LTTRUE; }
static LTBOOL xi_ScaleTrigger(InputMgr *, char *, char *, float, float, float, float) { return LTTRUE; }
static DeviceBinding *xi_GetDeviceBindings(uint32) { return LTNULL; }
static void xi_FreeDeviceBindings(DeviceBinding *) {}
static LTBOOL xi_StartDeviceTrack(InputMgr *, uint32, uint32) { return LTTRUE; }
static LTBOOL xi_TrackDevice(DeviceInput *, uint32 *pnInOut) { if (pnInOut) *pnInOut = 0; return LTTRUE; }
static LTBOOL xi_EndDeviceTrack() { return LTTRUE; }
static DeviceObject *xi_GetDeviceObjects(uint32) { return LTNULL; }
static void xi_FreeDeviceObjects(DeviceObject *) {}
static LTBOOL xi_GetDeviceName(uint32, char *pStrBuffer, uint32 nBufferSize)
{
	if (pStrBuffer && nBufferSize)
	{
		strncpy(pStrBuffer, "Xbox 360 Controller", nBufferSize - 1);
		pStrBuffer[nBufferSize - 1] = 0;
	}
	return LTTRUE;
}
static LTBOOL xi_IsDeviceEnabled(char *) { return LTTRUE; }
static LTBOOL xi_ShowDeviceObjects(char *) { return LTTRUE; }
static LTBOOL xi_ShowInputDevices() { return LTTRUE; }

static InputMgr g_XboxInputMgr = {
	xi_Init, xi_Term, xi_IsInitted, xi_ListDevices, xi_PlayJoystickEffect, xi_ReadInput, xi_FlushInputBuffers,
	xi_ClearInput, xi_AddAction, xi_EnableDevice, xi_ClearBindings, xi_AddBinding, xi_ScaleTrigger,
	xi_GetDeviceBindings, xi_FreeDeviceBindings, xi_StartDeviceTrack, xi_TrackDevice, xi_EndDeviceTrack,
	xi_GetDeviceObjects, xi_FreeDeviceObjects, xi_GetDeviceName, xi_IsDeviceEnabled, xi_ShowDeviceObjects,
	xi_ShowInputDevices,
};

LTRESULT input_GetManager(InputMgr **pMgr)
{
	*pMgr = &g_XboxInputMgr;
	return LT_OK;
}

void input_SaveBindings(FILE *) {}
