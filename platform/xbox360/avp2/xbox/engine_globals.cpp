// avp360: globais e funcoes que o AVP2 Reconstructed declara (extern / prototipo) mas cuja definicao fica num objeto
// ainda nao reconstruido. Os tipos seguem as declaracoes; as funcoes vazias sao as que o linker do VC6 juntou com
// outras funcoes vazias (os comentarios do decomp dizem qual).
#include "bdefs.h"
#include "clientmgr.h"
#include "servermgr.h"
#include "model.h"
#include "nexus.h"
#include "serverde_impl.h"
#include "input.h"
#include "de_objects.h"
#include "../../build/proj/LT2/lithshared/stdlith/linklist.h"

int32 g_DebugLevel = 0;
char *g_ReturnErrString = (char *)"%s returned %s (%s)";
char g_EmptyString[1] = "";

CClientMgr *g_pClientMgr = LTNULL;
CClientMgr *g_pCommandClientMgr = LTNULL;
CServerMgr *g_pServerMgr = LTNULL;
CClassMgr *g_pClassMgr = LTNULL;
MainWorld *g_pServerWorld = LTNULL;
char *g_pNoModelFilename = (char *)"<none>";
uint32 g_ModelMemory = 0;
float g_CV_DefaultDrawIndexedDist = 0.0f;
Node *g_NodeStack[1000];	// o mesmo tamanho do NOLF (engine/server/worldsim.cpp)

// Contadores de perfil (so o console de depuracao le).
uint32 g_nPolyTouchesBoxCalls, g_Ticks_PolyTouchesBox;
uint32 g_Ticks_FrameClientShell, g_Ticks_FrameNet, g_Ticks_FrameServer, g_Ticks_SoundUpdate;
uint32 g_Ticks_RenderScene, g_Ticks_Render_Objects, g_Ticks_Render_Models, g_Ticks_Render_Sprites;
uint32 g_Ticks_Render_WorldModels, g_Ticks_Render_ParticleSystems, g_Ticks_Render_PolyGrids;

// As sanguessugas dos modelos (cliente e servidor) nao reagem a mensagens do nexus nesta versao.
static LTRESULT ModelLeechFn(Nexus *, Leech *, int, void *) { return LT_OK; }
LeechDef g_ClientModelLeechDef = { ModelLeechFn, LTNULL };
LeechDef g_ServerModelLeechDef = { ModelLeechFn, LTNULL };

// Funcoes vazias juntadas pelo linker.
void w_NullProgressFn(uint32) {}
void sm_TermDebug() {}
void cb_ModuleLoaded(ClassBindModule *) {}
void cm_OnMemoryFailure(void *) {}
void cm_OnTextureBound(CClientMgr *) {}
void cm_ShowLoadProgress(CClientMgr *) {}
void ic_StartCounter(LTCounter *) {}
void vq_DefaultFn1() {}
void vq_DefaultFn2() {}
LTBOOL vq_DefaultBoolFn() { return LTFALSE; }
int sm_FTCantOpenFileFn(FTServ *, char *) { return 1; }

// stdlith/linklist.cpp
LPOS LinkList_FindElementMemcmp(const void *pHead, const void *pToFind, DWORD elementSize)
{
	CGenLLNode *pCur = (CGenLLNode *)pHead;
	if (pCur)
	{
		do
		{
			if (memcmp(pCur->m_Data, pToFind, elementSize) == 0)
				return pCur;
			pCur = pCur->m_pNext;
		} while (pCur != (CGenLLNode *)pHead);
	}
	return NULL;
}

// O tokenizador de comandos do console (0x0040c3d0); o mesmo do NOLF (engine/server/console.cpp parse_Command).
#define PARSE_DONE 0
#define PARSE_ARG 1
#define PARSE_END 2
static int parse_Token(char **ppPos, char **ppOut)
{
	int nDepth = 0;
	while (**ppPos == ' ')
		(*ppPos)++;
	if (**ppPos == 0)
		return PARSE_DONE;

	char *pStart = *ppOut;
	for (;;)
	{
		if (**ppPos == 0)
			break;
		char c = **ppPos;
		if (c == '%')
		{
			(*ppPos)++;
			c = **ppPos;
			if (c == 0)
				break;
		}
		else if (c == ';' || iscntrl((unsigned char)c))
		{
			**ppOut = 0;
			(*ppOut)++;
			if (*pStart == 0)
			{
				(*ppPos)++;
				return PARSE_END;
			}
			return PARSE_ARG;
		}
		else if (c == '(')
		{
			if (++nDepth == 1)
				goto next;
		}
		else if (c == ')')
		{
			if (--nDepth == 0)
			{
				(*ppPos)++;
				break;
			}
		}
		else if (c == '"')
		{
			if (nDepth != 0)
			{
				(*ppPos)++;
				break;
			}
			nDepth = 1;
			goto next;
		}
		else if (c == ' ' && nDepth == 0)
		{
			break;
		}

		**ppOut = c;
		if (*ppOut - pStart >= 0x100)
		{
			**ppOut = 0;
			(*ppOut)--;
		}
		(*ppOut)++;
	next:
		(*ppPos)++;
	}
	**ppOut = 0;
	(*ppOut)++;
	return (*pStart != 0);
}

int ic_Parse(char *pCommand, char **pNewCommandPos, char *argBuffer, char **argPointers, int *nArgs)
{
	char *pCur = pCommand;
	char *pOut = argBuffer;
	*nArgs = 0;
	for (;;)
	{
		int nType = parse_Token(&pCur, &pOut);
		if (nType == PARSE_DONE)
			return 0;
		if (nType == PARSE_ARG)
		{
			argPointers[(*nArgs)++] = argBuffer;
			if (*nArgs >= 30)
				return 0;
		}
		else if (nType == PARSE_END)
		{
			break;
		}
		argBuffer = pOut;
	}
	*pNewCommandPos = pCur;
	return 1;
}
