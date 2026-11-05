// avp360: GameSpy (lista de servidores na internet) e LithTrack (rastreamento de servidores) nao existem no console.
// As classes respondem como uma rede sem servidores: nenhuma lista, nenhuma chave de CD exigida. So os membros que o
// jogo chama estao aqui (os inline ficam nos cabecalhos).
// O LithTrackMgr que o jogo usa e o do cliente (INTERFACE = ILTClient, com Draw).
#ifndef _CLIENTBUILD
#define _CLIENTBUILD
#endif
#include "mfcstub.h"
#include "iltclient.h"
#include "GameSpyMgr.h"
#include "GameSpyClientMgr.h"
#include "lithtrackmgr.h"
#include "ltguimgr.h"

// ---- Servidor: responde a consultas GameSpy (nunca chegam) ----
CGameSpyMgr::CGameSpyMgr() { m_bInit = FALSE; m_pSender = NULL; m_pAuthContext = NULL; m_dwFlags = 0; }
BOOL CGameSpyMgr::Init(const char *, const char *, const char *, int, const char *, int nGamePort, int nSpyPort, BOOL, DWORD dwFlags)
{
	m_dwGamePort = nGamePort;
	m_dwSpyPort = nSpyPort;
	m_dwFlags = dwFlags;
	return FALSE;
}
void CGameSpyMgr::Term() { m_bInit = FALSE; }
void CGameSpyMgr::Update() {}
BOOL CGameSpyMgr::OnQuery(char *, DWORD, char *, int) { return FALSE; }
void CGameSpyMgr::SendResponseInfo(const char *, const char *) {}
void CGameSpyMgr::SendResponseInfo(const char *, int) {}

// ---- Cliente: lista de servidores sempre vazia ----
CGameSpyClientMgr::CGameSpyClientMgr()
{
	m_bInitialized = FALSE;
	m_bLANOnly = TRUE;
	m_bForceCDKeyValid = TRUE;
	m_nVersionCheckState = 0;
	m_sGameName[0] = m_sSecretKey[0] = m_sVersion[0] = 0;
	m_nGetServerIndex = 0;
	m_nProgress = 0;
	m_bNewServer = FALSE;
	m_pAuthContext = NULL;
}
BOOL CGameSpyClientMgr::Init(const char *, const char *, const char *sVersion)
{
	strncpy(m_sVersion, sVersion ? sVersion : "", sizeof m_sVersion - 1);
	m_sVersion[sizeof m_sVersion - 1] = 0;
	m_bInitialized = TRUE;
	return TRUE;
}
void CGameSpyClientMgr::Term() { m_bInitialized = FALSE; }
BOOL CGameSpyClientMgr::SetupCDKey(char *) { return TRUE; }
BOOL CGameSpyClientMgr::IsCDKeyValid() { return TRUE; }
BOOL CGameSpyClientMgr::SetupUserIdentity() { return TRUE; }
BOOL CGameSpyClientMgr::SetupDirServers() { return TRUE; }
BOOL CGameSpyClientMgr::SetupMOTD(char *szSysMsg, char *szGameMsg, int nMaxLength)
{
	if (szSysMsg && nMaxLength > 0) szSysMsg[0] = 0;
	if (szGameMsg && nMaxLength > 0) szGameMsg[0] = 0;
	return TRUE;
}
BOOL CGameSpyClientMgr::SetupVersionCheck() { return TRUE; }
BOOL CGameSpyClientMgr::SetupAuthentication(char *szFailedMsg, int nMaxLength)
{
	if (szFailedMsg && nMaxLength > 0) szFailedMsg[0] = 0;
	return TRUE;
}
int CGameSpyClientMgr::GetNumServers() { return 0; }
int CGameSpyClientMgr::GetState() { return 0; }
BOOL CGameSpyClientMgr::RefreshServers(BOOL) { return FALSE; }
BOOL CGameSpyClientMgr::RefreshLANServers() { return FALSE; }
CGameSpyServer *CGameSpyClientMgr::GetServerFromHandle(void *) { return NULL; }
CGameSpyServer *CGameSpyClientMgr::GetFirstServer() { return NULL; }
CGameSpyServer *CGameSpyClientMgr::GetNextServer() { return NULL; }
BOOL CGameSpyClientMgr::ExistNewServer() { return FALSE; }
void CGameSpyClientMgr::SortServersByName(BOOL) {}
void CGameSpyClientMgr::SortServersByPing(BOOL) {}
void CGameSpyClientMgr::SortServersByPlayers(BOOL) {}
void CGameSpyClientMgr::SortServersByMap(BOOL) {}
void CGameSpyClientMgr::SortServersByGameType(BOOL) {}
void CGameSpyClientMgr::Update() {}
void CGameSpyClientMgr::KillGetServersOp() {}

// ---- Servidor e jogador de uma lista (nunca ha nenhum) ----
void CGameSpyServer::Term() { m_bInit = FALSE; }
char *CGameSpyServer::GetName() { return (char *)""; }
char *CGameSpyServer::GetGameType() { return (char *)""; }
BOOL CGameSpyServer::GetDedicated() { return FALSE; }
int CGameSpyServer::GetPing() { return 0; }
int CGameSpyServer::GetNumPlayers() { return 0; }
int CGameSpyServer::GetMaxPlayers() { return 0; }
char *CGameSpyServer::GetMap() { return (char *)""; }
char *CGameSpyServer::GetAddress() { return (char *)""; }
char *CGameSpyServer::GetStringValue(char *, char *sDefault) { return sDefault; }
int CGameSpyServer::GetIntValue(char *, int nDefault) { return nDefault; }
int CGameSpyServer::UpdatePlayers() { return 0; }
CGameSpyPlayer *CGameSpyServer::GetFirstPlayer() { return NULL; }
CGameSpyPlayer *CGameSpyServer::GetNextPlayer() { return NULL; }
void CGameSpyServer::SortPlayers(BOOL, int) {}
void CGameSpyPlayer::Term() {}
char *CGameSpyPlayer::GetName() { return (char *)""; }
int CGameSpyPlayer::GetRace() { return 0; }

// ---- LithTrack ----
LithTrackMgr *g_pTrackMgr = LTNULL;	// nunca criado no console (GameClientShell.cpp)
DBOOL LithTrackMgr::Init(INTERFACE *) { return DFALSE; }
void LithTrackMgr::Term() {}
DBOOL LithTrackMgr::Update() { return DTRUE; }
DBOOL LithTrackMgr::Draw() { return DTRUE; }

// ---- ltguimgr: novo no AvP2 ----
// Coluna de texto com quebra de linha (tela de controles). Por enquanto uma coluna simples alinhada a esquerda.
LTBOOL CLTGUIColumnTextCtrl::AddWordWrapColumn(int messageCode, int nWidth)
{
	return AddColumn(messageCode, nWidth, LTF_JUSTIFY_LEFT);
}

// Chamados pelos construtores inline dos cabecalhos.
void CGameSpyServer::Clear() { m_pGServer = NULL; m_bInit = FALSE; m_nNumPlayers = 0; m_nPlayerIndex = 0; }
void CGameSpyPlayer::Clear() {}
