// avp360: funcoes do lithtech.exe que o AVP2 Reconstructed ainda nao reconstruiu (ou que estavam em arquivos da
// camada Windows que o Xbox nao usa). Cada uma segue a funcao irma que o decomp ja tem.
#include "bdefs.h"
#include "serverde_impl.h"
#include "cloaderthread.h"
#include "model.h"
#include "iltcsbase.h"
#include "binkvideomgrimpl.h"
#include "smackvideomgrimpl.h"

// Como CPhysicsLT (client/clientde_impl.cpp): o servidor guarda a altura de degrau do mesmo jeito.
LTRESULT SPhysicsLT::GetStairHeight(float &fHeight)
{
	fHeight = m_fStairHeight;
	return LT_OK;
}

LTRESULT SPhysicsLT::SetStairHeight(float fHeight)
{
	m_fStairHeight = fHeight;
	return LT_OK;
}

// 0x0048eb80: o mesmo codigo de CServerLoaderThread::Init (o linker juntou as duas).
LTBOOL CLoaderThread::Init(CClientMgr *pClientMgr)
{
	m_pClientMgr = pClientMgr;
	return LTTRUE;
}

// 0x0046c290
char *Model::GetFilename()
{
	return m_pFilename;
}

// Par de CLTServer::GetObjectFlags (server/serverde_impl.cpp).
void CLTServer::SetObjectFlags(HOBJECT hObj, uint32 flags)
{
	m_pCommonLT->SetObjectFlags(hObj, OFT_Flags, flags);
}

// Par de ILTCSBase::ReadFromLoadSaveMessageObject (shared/csbase.cpp).
LTRESULT ILTCSBase::WriteToLoadSaveMessageObject(HMESSAGEWRITE hMessage, HOBJECT hObject)
{
	if (hMessage)
		return hMessage->WriteObject(hObject);
	RETURN_ERROR(2, ILTCSBase::WriteToLoadSaveMessageObject, LT_INVALIDPARAMS);
}

// Bink e Smacker (videos): sem decodificador no console por enquanto. Init falha, e o jogo segue sem os filmes,
// como faz no PC quando o binkw32.dll nao carrega.
BinkVideoMgr::BinkVideoMgr(CClientMgr *pClientMgr) : VideoMgr(pClientMgr) {}
BinkVideoMgr::~BinkVideoMgr() {}
LTRESULT BinkVideoMgr::Init() { return LT_ERROR; }
SmackVideoMgr::SmackVideoMgr(CClientMgr *pClientMgr) : VideoMgr(pClientMgr) {}
SmackVideoMgr::~SmackVideoMgr() {}
LTRESULT SmackVideoMgr::Init() { return LT_ERROR; }
LTRESULT BinkVideoMgr::CreateScreenVideo(const char *, uint32, VideoInst *&pVideo) { pVideo = LTNULL; return LT_ERROR; }
LTRESULT BinkVideoMgr::CreateTextureVideo(const char *, uint32, VideoInst *&pVideo) { pVideo = LTNULL; return LT_ERROR; }
LTRESULT SmackVideoMgr::CreateScreenVideo(const char *, uint32, VideoInst *&pVideo) { pVideo = LTNULL; return LT_ERROR; }
LTRESULT SmackVideoMgr::CreateTextureVideo(const char *, uint32, VideoInst *&pVideo) { pVideo = LTNULL; return LT_ERROR; }
