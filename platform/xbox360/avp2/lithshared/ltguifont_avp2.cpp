// avp360: CLTGUIFont para o cabecalho do AvP2 (proj/LT2/lithshared/ltguimgr/ltguifont.h).
// A logica de desenho e a do NOLF (nolfsrc-master/src/lithshared/ltguimgr/ltguifont.cpp). O AvP2 acrescentou a
// fonte de menu: tres LithFonts (normal, selecionada, desabilitada) escolhidas pelo estado em DrawMenu*.
#include "mfcstub.h"
#include "ltguimgr.h"
#include "ltguifont.h"

LTBOOL CLTGUIFont::s_bWrapUseSpaces = LTTRUE;

CLTGUIFont::CLTGUIFont()
{
	m_pClientDE = LTNULL;
	m_nFontHeight = 0;
	m_nType = LGFT_NONE;
	for (int i = 0; i < LGF_MAX_FONTS; i++)
		m_pLithFonts[i] = LTNULL;
}

CLTGUIFont::~CLTGUIFont()
{
	Term();
}

LTBOOL CLTGUIFont::Init(ILTClient *pClientDE, LITHFONTCREATESTRUCT *lfCS)
{
	Term();
	m_pClientDE = pClientDE;
	if (!pClientDE)
		return LTFALSE;
	m_pLithFonts[LGFI_NORMAL] = CreateLithFont(pClientDE, lfCS, LTTRUE);
	m_nType = LGFT_BITMAP;
	return m_pLithFonts[LGFI_NORMAL] != LTNULL;
}

LTBOOL CLTGUIFont::Init(ILTClient *pClientDE, LITHFONTCREATESTRUCT *sNormalFont, LITHFONTCREATESTRUCT *sSelectedFont,
	LITHFONTCREATESTRUCT *sDisabledFont)
{
	Term();
	m_pClientDE = pClientDE;
	if (!pClientDE)
		return LTFALSE;
	LITHFONTCREATESTRUCT *cs[LGF_MAX_FONTS] = { sNormalFont, sSelectedFont, sDisabledFont };
	for (int i = 0; i < LGF_MAX_FONTS; i++) {
		if (!cs[i])
			continue;
		m_pLithFonts[i] = CreateLithFont(pClientDE, cs[i], LTTRUE);
		if (!m_pLithFonts[i]) {
			Term();
			return LTFALSE;
		}
	}
	m_nType = LGFT_MENU;
	return m_pLithFonts[LGFI_NORMAL] != LTNULL;
}

void CLTGUIFont::Term()
{
	for (int i = 0; i < LGF_MAX_FONTS; i++) {
		if (m_pLithFonts[i]) {
			FreeLithFont(m_pLithFonts[i]);
			m_pLithFonts[i] = LTNULL;
		}
	}
	m_nType = LGFT_NONE;
}

// Como o NOLF: fontes de bitmap sem chroma key sao desenhadas com mascara e depois somadas na cor pedida.
static void DrawWithColor(ILTClient *pClient, LithFont *pFont, HSURFACE hDest, char *szString, LITHFONTDRAWDATA *dd,
	int x, int y, HLTCOLOR color)
{
	if (pFont->IsChromaKeyed() || pFont->IsSystemFont()) {
		pFont->Draw(hDest, szString, dd, x, y);
		return;
	}
	if (color == SETRGB(255, 255, 255)) {
		pClient->SetOptimized2DBlend(LTSURFACEBLEND_ADD);
		pFont->Draw(hDest, szString, dd, x, y);
	} else if (color == LTNULL) {
		pClient->SetOptimized2DBlend(LTSURFACEBLEND_MASK);
		pFont->Draw(hDest, szString, dd, x, y);
	} else {
		pClient->SetOptimized2DBlend(LTSURFACEBLEND_MASK);
		pFont->Draw(hDest, szString, dd, x, y);
		pClient->SetOptimized2DBlend(LTSURFACEBLEND_ADD);
		pClient->SetOptimized2DColor(color);
		pFont->Draw(hDest, szString, dd, x, y, LTNULL);
	}
	pClient->SetOptimized2DBlend(LTSURFACEBLEND_ALPHA);
	pClient->SetOptimized2DColor(SETRGB(255, 255, 255));
}

static uint8 Justify(int justify)
{
	switch (justify) {
	case 1: return LTF_JUSTIFY_RIGHT;
	case 2: return LTF_JUSTIFY_CENTER;
	default: return LTF_JUSTIFY_LEFT;
	}
}

void CLTGUIFont::Draw(char *szString, HSURFACE hDest, int x, int y, int justify, HLTCOLOR color)
{
	if (!m_pClientDE || !szString || !m_pLithFonts[LGFI_NORMAL])
		return;
	LITHFONTDRAWDATA dd;
	dd.dwFlags = LTF_DRAW_NORMAL;
	dd.hColor = color;
	dd.bForceFormat = LTTRUE;
	dd.byJustify = Justify(justify);
	DrawWithColor(m_pClientDE, m_pLithFonts[LGFI_NORMAL], hDest, szString, &dd, x, y, color);
}

void CLTGUIFont::Draw(HSTRING hString, HSURFACE hDest, int x, int y, int justify, HLTCOLOR color)
{
	if (!m_pClientDE || !hString)
		return;
	Draw(m_pClientDE->GetStringData(hString), hDest, x, y, justify, color);
}

void CLTGUIFont::DrawFormat(char *szString, HSURFACE hDest, int x, int y, int nWidth, HLTCOLOR color)
{
	if (!m_pClientDE || !szString || !m_pLithFonts[LGFI_NORMAL])
		return;
	LITHFONTDRAWDATA dd;
	dd.dwFlags = LTF_DRAW_FORMATTED;
	dd.dwFormatWidth = nWidth;
	dd.byJustify = LTF_JUSTIFY_LEFT;
	dd.bForceFormat = LTTRUE;
	dd.hColor = color;
	DrawWithColor(m_pClientDE, m_pLithFonts[LGFI_NORMAL], hDest, szString, &dd, x, y, color);
}

void CLTGUIFont::DrawFormat(HSTRING hString, HSURFACE hDest, int x, int y, int nWidth, HLTCOLOR color)
{
	if (!m_pClientDE || !hString)
		return;
	DrawFormat(m_pClientDE->GetStringData(hString), hDest, x, y, nWidth, color);
}

void CLTGUIFont::Draw(char *szString, HSURFACE hDest, LITHFONTDRAWDATA *lfDD, int x, int y, LITHFONTSAVEDATA *lfSD)
{
	if (!m_pClientDE || !szString || !m_pLithFonts[LGFI_NORMAL])
		return;
	m_pLithFonts[LGFI_NORMAL]->Draw(hDest, szString, lfDD, x, y, lfSD);
}

void CLTGUIFont::Draw(HSTRING hString, HSURFACE hDest, LITHFONTDRAWDATA *lfDD, int x, int y, LITHFONTSAVEDATA *lfSD)
{
	if (!m_pClientDE || !hString)
		return;
	Draw(m_pClientDE->GetStringData(hString), hDest, lfDD, x, y, lfSD);
}

// Fonte de menu: o estado (LGFI_NORMAL, LGFI_SELECTED, LGFI_DISABLED) escolhe a fonte, que ja tem a sua cor.
void CLTGUIFont::DrawMenu(char *szString, HSURFACE hDest, int x, int y, int justify, int state)
{
	if (!m_pClientDE || !szString)
		return;
	if (state < 0 || state >= LGF_MAX_FONTS || !m_pLithFonts[state])
		state = LGFI_NORMAL;
	LithFont *pFont = m_pLithFonts[state];
	if (!pFont)
		return;
	LITHFONTDRAWDATA dd;
	dd.dwFlags = LTF_DRAW_NORMAL;
	dd.hColor = LTNULL;
	dd.bForceFormat = LTTRUE;
	dd.byJustify = Justify(justify);
	DrawWithColor(m_pClientDE, pFont, hDest, szString, &dd, x, y, LTNULL);
}

void CLTGUIFont::DrawMenu(HSTRING hString, HSURFACE hDest, int x, int y, int justify, int state)
{
	if (!m_pClientDE || !hString)
		return;
	DrawMenu(m_pClientDE->GetStringData(hString), hDest, x, y, justify, state);
}

void CLTGUIFont::DrawMenuFormat(char *szString, HSURFACE hDest, int x, int y, int width, int state)
{
	if (!m_pClientDE || !szString)
		return;
	if (state < 0 || state >= LGF_MAX_FONTS || !m_pLithFonts[state])
		state = LGFI_NORMAL;
	LithFont *pFont = m_pLithFonts[state];
	if (!pFont)
		return;
	LITHFONTDRAWDATA dd;
	dd.dwFlags = LTF_DRAW_FORMATTED;
	dd.dwFormatWidth = width;
	dd.byJustify = LTF_JUSTIFY_LEFT;
	dd.bForceFormat = LTTRUE;
	dd.hColor = LTNULL;
	DrawWithColor(m_pClientDE, pFont, hDest, szString, &dd, x, y, LTNULL);
}

void CLTGUIFont::DrawMenuFormat(HSTRING hString, HSURFACE hDest, int x, int y, int width, int state)
{
	if (!m_pClientDE || !hString)
		return;
	DrawMenuFormat(m_pClientDE->GetStringData(hString), hDest, x, y, width, state);
}

LTIntPt CLTGUIFont::GetTextExtents(char *szString)
{
	LTIntPt pt(0, 0);
	if (!m_pClientDE || !szString || !m_pLithFonts[LGFI_NORMAL])
		return pt;
	LITHFONTDRAWDATA dd;
	dd.dwFlags = LTF_DRAW_NORMAL;
	dd.byJustify = LTF_JUSTIFY_LEFT;
	uint32 x = 0, y = 0;
	m_pLithFonts[LGFI_NORMAL]->GetTextExtents(szString, &dd, x, y);
	pt.x = x;
	pt.y = y;
	return pt;
}

LTIntPt CLTGUIFont::GetTextExtents(HSTRING hString)
{
	if (!m_pClientDE || !hString)
		return LTIntPt(0, 0);
	return GetTextExtents(m_pClientDE->GetStringData(hString));
}

LTIntPt CLTGUIFont::GetTextExtentsFormat(char *szString, int nWidth)
{
	LTIntPt pt(0, 0);
	if (!m_pClientDE || !szString || !m_pLithFonts[LGFI_NORMAL])
		return pt;
	LITHFONTDRAWDATA dd;
	dd.dwFlags = LTF_DRAW_FORMATTED;
	dd.dwFormatWidth = nWidth;
	dd.byJustify = LTF_JUSTIFY_LEFT;
	dd.bForceFormat = LTTRUE;
	uint32 x = 0, y = 0;
	m_pLithFonts[LGFI_NORMAL]->GetTextExtents(szString, &dd, x, y);
	pt.x = x;
	pt.y = y;
	return pt;
}

LTIntPt CLTGUIFont::GetTextExtentsFormat(HSTRING hString, int nWidth)
{
	if (!m_pClientDE || !hString)
		return LTIntPt(0, 0);
	return GetTextExtentsFormat(m_pClientDE->GetStringData(hString), nWidth);
}

LTIntPt CLTGUIFont::GetPositionFromIndex(HSTRING hString, int nIndex, int nWidth)
{
	LTIntPt pt(0, 0);
	if (!m_pClientDE || !hString || !m_pLithFonts[LGFI_NORMAL])
		return pt;
	LITHFONTDRAWDATA dd;
	if (nWidth > 0) {
		dd.dwFlags = LTF_DRAW_FORMATTED;
		dd.dwFormatWidth = nWidth;
	} else {
		dd.dwFlags = LTF_DRAW_NORMAL;
	}
	dd.bForceFormat = LTTRUE;
	dd.byJustify = LTF_JUSTIFY_LEFT;
	uint32 x = 0, y = 0;
	m_pLithFonts[LGFI_NORMAL]->GetPositionFromIndex(m_pClientDE->GetStringData(hString), &dd, nIndex, x, y);
	pt.x = x;
	pt.y = y;
	return pt;
}

// O caractere cuja posicao fica logo antes de (x, y): para clicar dentro de um campo de texto.
int CLTGUIFont::GetIndexFromPosition(HSTRING hString, int x, int y)
{
	if (!m_pClientDE || !hString || !m_pLithFonts[LGFI_NORMAL])
		return 0;
	const char *sz = m_pClientDE->GetStringData(hString);
	const int len = sz ? (int)strlen(sz) : 0;
	const int height = GetHeight();
	for (int i = 0; i < len; i++) {
		LTIntPt next = GetPositionFromIndex(hString, i + 1);
		if (next.y + height > y && next.x > x)
			return i;
	}
	return len;
}

int CLTGUIFont::GetHeight()
{
	return m_pLithFonts[LGFI_NORMAL] ? m_pLithFonts[LGFI_NORMAL]->Height() : 0;
}

LTBOOL CLTGUIFont::IsChromaKeyed()
{
	return m_pLithFonts[LGFI_NORMAL] ? m_pLithFonts[LGFI_NORMAL]->IsChromaKeyed() : LTFALSE;
}

LTBOOL CLTGUIFont::IsSystemFont()
{
	return m_pLithFonts[LGFI_NORMAL] ? m_pLithFonts[LGFI_NORMAL]->IsSystemFont() : LTFALSE;
}
