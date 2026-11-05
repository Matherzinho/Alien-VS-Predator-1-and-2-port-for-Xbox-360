// avp360: o lugar de kernel/sys/win/client.cpp no Xbox 360. Mesmo laco do RunClientApp/StartClient do LithTech, sem
// janela nem fila de mensagens: o renderizador Xenos apresenta direto na tela e o controle e lido pelo input_xbox.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <setjmp.h>
#include <pthread.h>
#include "bdefs.h"
#include "de_memory.h"
#include "clientmgr.h"
#include "dsys_interface.h"
#include "render.h"
#include "../../build/proj/LT2/lithshared/stdlith/helpers.h"

#define MAX_RESTREES	20

// Os globais que o client.cpp original define.
int g_iStopAllocCount = -1;
uint32 g_CurRunIteration = 0;
uint32 g_EngineStartMS;
ClientGlob g_ClientGlob;

extern int g_bDebugStructBanks;
extern int32 g_CV_PlayDemoReps;

void DebugOut(const char *pMsg, ...);
void avp360_input_frame(bool bMenu);	// input_xbox.cpp

// A linha de comando que o lithtech.exe recebia do launcher do AvP2. Os .rez ficam em game:\avp2\ (o diretorio de
// trabalho e ajustado pelo path.c do xsdk).
static char *g_XboxArgs[] = {
	(char *)"avp2.xex",
	(char *)"-rez", (char *)"avp2.rez",
	(char *)"-rez", (char *)"avp2l.rez",
	(char *)"-rez", (char *)"sound.rez",
	(char *)"-rez", (char *)"avp2dll.rez",
	(char *)"-rez", (char *)"avp2p.rez",
	(char *)"-rez", (char *)"avp2p1.rez",
	(char *)"-rez", (char *)"avp2p5.rez",
	(char *)"-rez", (char *)"custom",
	(char *)"-windowtitle", (char *)"Aliens vs Predator 2",
};
int __argc = sizeof(g_XboxArgs) / sizeof(g_XboxArgs[0]);
char **__argv = g_XboxArgs;

char *cl_FindArg(const char *pArgName, int argc, char **argv)
{
	char dashArg[256];
	sprintf(dashArg, "-%s", pArgName);
	for (int i = 0; i < argc; i++)
	{
		if (CHelpers::UpperStrcmp(argv[i], pArgName) || CHelpers::UpperStrcmp(argv[i], dashArg))
			return (i < argc - 1) ? argv[i + 1] : g_EmptyString;
	}
	return LTNULL;
}

char *cl_FindCommandLineArg(const char *pArgName)
{
	return cl_FindArg(pArgName, __argc, __argv);
}

LTBOOL StartClient(ClientGlob *pGlob, CmdLineArgs *pArgs)
{
	const char *resTrees[MAX_RESTREES];
	uint32 nResTrees = 0;
	char strVersion[32];

	pGlob->m_bHost = cl_FindArg("host", pArgs->m_Argc, pArgs->m_Argv) != NULL;
	pGlob->m_pWorldName = cl_FindArg("world", pArgs->m_Argc, pArgs->m_Argv);

	for (int i = 0; i < pArgs->m_Argc - 1 && nResTrees < MAX_RESTREES; i++)
	{
		if (stricmp(pArgs->m_Argv[i], "-rez") == 0)
			resTrees[nResTrees++] = pArgs->m_Argv[i + 1];
	}

	if (cl_FindArg("noinput", pArgs->m_Argc, pArgs->m_Argv))
		pGlob->m_bInputEnabled = LTFALSE;

	const char *pConfigFile = cl_FindArg("config", pArgs->m_Argc, pArgs->m_Argv);
	if (!pConfigFile)
		pConfigFile = "autoexec.cfg";

	uint32 initStartTime = timeGetTime();
	if (pGlob->m_pClientMgr->Init(resTrees, nResTrees, pConfigFile, pArgs) != LT_OK)
		return LTFALSE;

	pGlob->m_pClientMgr->m_VersionInfo.GetString(strVersion, sizeof(strVersion));
	DebugOut("LithTech build %s initialized in %.2f seconds.\n", strVersion,
		(float)(timeGetTime() - initStartTime) / 1000.0f);
	return LTTRUE;
}

static int RunClientApp(CmdLineArgs *pArgs)
{
	ClientGlob *pGlob = &g_ClientGlob;
	memset(pGlob, 0, sizeof(*pGlob));

	if (cl_FindArg("DebugStructBanks", pArgs->m_Argc, pArgs->m_Argv))
		g_bDebugStructBanks = LTTRUE;

	LTBOOL bOutOfMemory = LTFALSE;
	if (setjmp(g_ClientGlob.m_MemoryJmp) != 0)
	{
		bOutOfMemory = LTTRUE;
		goto END_MAINLOOP;
	}

	pGlob->m_bInputEnabled = LTTRUE;
	if (dsi_Init() != 0)
	{
		dsi_Term();
		fprintf(stderr, "avp2: dsi_Init falhou (ltmsg)\n");
		return -1;
	}

	pGlob->m_hInstance = (HINSTANCE)1;
	pGlob->m_WndClassName = (char *)"LithTech";
	pGlob->m_pClientMgr = cm_Init();
	pGlob->m_bClientActive = LTTRUE;
	pGlob->m_WndCaption = cl_FindArg("windowtitle", pArgs->m_Argc, pArgs->m_Argv);
	if (!pGlob->m_WndCaption)
		pGlob->m_WndCaption = "LithTech";
	pGlob->m_hMainWnd = (HWND)1;	// o renderizador Xenos ignora a janela

	if (StartClient(pGlob, pArgs))
	{
		pGlob->m_bProcessWindowMessages = LTTRUE;
		for (;;)
		{
			// O controle (input_xbox.cpp): menus quando o jogo desliga a entrada (pausa, antes de um mapa).
			avp360_input_frame(!pGlob->m_pClientMgr->m_bInputState);
			if (pGlob->m_pClientMgr->Update() != LT_OK)
				break;
			MSG msg;
			// O shim so produz WM_QUIT (PostQuitMessage do jogo ao sair).
			if (PeekMessage(&msg, LTNULL, 0, 0, PM_REMOVE) && msg.message == WM_QUIT)
				break;
		}
	}

END_MAINLOOP:
	if (pGlob->m_ExitMessage[0])
	{
		r_TermRender(pGlob->m_pClientMgr, 2);
		dsi_MessageBox(pGlob->m_ExitMessage, pGlob->m_WndCaption);
	}

	pGlob->m_bProcessWindowMessages = LTFALSE;
	delete pGlob->m_pClientMgr;
	pGlob->m_pClientMgr = LTNULL;
	dsi_Term();

	if (bOutOfMemory)
		fprintf(stderr, "avp2: sem memoria\n");
	return 0;
}

extern "C" void avp360_return_to_menu(void);

static int g_GameResult;

static void *GameThread(void *)
{
	CmdLineArgs args;
	args.m_Argc = __argc;
	args.m_Argv = __argv;
	args.m_pArgBuffer = LTNULL;

	g_EngineStartMS = timeGetTime();
	g_GameResult = RunClientApp(&args);
	while (g_CV_PlayDemoReps > 1 && ++g_CurRunIteration < (uint32)g_CV_PlayDemoReps)
		g_GameResult = RunClientApp(&args);
	return NULL;
}

// A thread principal de um .xex do xsdk tem 1 MB de pilha; o LithTech e o d3d.ren foram feitos para a do PC e tem
// arrays grandes na pilha. O jogo roda numa thread de 16 MB.
int main(int, char **)
{
	pthread_attr_t attr;
	pthread_t thread;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 16u * 1024u * 1024u);
	if (pthread_create(&thread, &attr, GameThread, NULL) == 0)
		pthread_join(thread, NULL);
	else
		GameThread(NULL);
	pthread_attr_destroy(&attr);

	avp360_return_to_menu();
	return g_GameResult;
}
