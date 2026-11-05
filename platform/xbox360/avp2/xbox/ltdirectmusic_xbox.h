// avp360: ILTDirectMusicMgr sem DirectMusic (o console nao tem). Por enquanto silencioso: aceita os comandos do
// jogo (intensidades, segmentos secundarios, motifs) e guarda o estado que ele consulta. A trilha do AvP2 e de
// segmentos DirectMusic (.sgt/.sty/.dls); toca-los de verdade e uma etapa a parte.
#ifndef AVP360_LTDIRECTMUSIC_XBOX_H
#define AVP360_LTDIRECTMUSIC_XBOX_H

#include "iltdirectmusic.h"

class CLTDirectMusicMgr : public ILTDirectMusicMgr
{
public:
	CLTDirectMusicMgr() : m_nIntensity(0), m_nVolume(0), m_bPlaying(false) {}
	virtual ~CLTDirectMusicMgr() {}

	virtual LTRESULT Init() { return LT_OK; }
	virtual LTRESULT Term() { return LT_OK; }
	virtual LTRESULT InitLevel(const char *, const char *, const char * = NULL, const char * = NULL, const char * = NULL) { m_nIntensity = 0; return LT_OK; }
	virtual LTRESULT TermLevel() { m_bPlaying = false; return LT_OK; }
	virtual LTRESULT Play() { m_bPlaying = true; return LT_OK; }
	virtual LTRESULT Stop(const LTDMEnactTypes = LTDMEnactDefault) { m_bPlaying = false; return LT_OK; }
	virtual LTRESULT Pause(const LTDMEnactTypes = LTDMEnactDefault) { return LT_OK; }
	virtual LTRESULT UnPause() { return LT_OK; }
	virtual LTRESULT SetVolume(const long nVolume) { m_nVolume = nVolume; return LT_OK; }
	virtual LTRESULT ChangeIntensity(const int n, const LTDMEnactTypes = LTDMEnactInvalid) { m_nIntensity = n; return LT_OK; }
	virtual LTRESULT PlaySecondary(const char *, const LTDMEnactTypes = LTDMEnactDefault) { return LT_OK; }
	virtual LTRESULT StopSecondary(const char * = NULL, const LTDMEnactTypes = LTDMEnactDefault) { return LT_OK; }
	virtual LTRESULT PlayMotif(const char *, const char *, const LTDMEnactTypes = LTDMEnactDefault) { return LT_OK; }
	virtual LTRESULT StopMotif(const char *, const char * = NULL, const LTDMEnactTypes = LTDMEnactDefault) { return LT_OK; }
	virtual int GetCurIntensity() { return m_nIntensity; }
	virtual LTDMEnactTypes StringToEnactType(const char *sName);
	virtual void EnactTypeToString(LTDMEnactTypes nType, char *sName);
	virtual int GetNumIntensities() { return 0; }
	virtual int GetInitialIntensity() { return 0; }
	virtual int GetInitialVolume() { return 0; }
	virtual int GetVolumeOffset() { return 0; }

private:
	int m_nIntensity;
	long m_nVolume;
	bool m_bPlaying;
};

#endif
