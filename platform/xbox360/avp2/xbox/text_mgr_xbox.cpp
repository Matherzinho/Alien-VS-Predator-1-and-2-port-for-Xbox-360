// avp360: o lugar de kernel/sys/win/text_mgr.cpp (fontes do Windows via GDI). O AvP2 desenha quase todo o texto com
// as fontes em bitmap do lithshared (CLTGUIFont); estas funcoes do ILTClient sao a via antiga (HLTFONT).
// Por enquanto: medidas aproximadas e sem desenho. TODO: rasterizar com a fonte do xsdk numa superficie do motor.
#include <windows.h>
#include <string.h>
#include "bdefs.h"
#include "stringmgr.h"
#include "iltclient.h"

struct XboxFont
{
	int m_Width, m_Height, m_Extra;
};

static HLTFONT tmgr_CreateFont(char *, int width, int height, LTBOOL, LTBOOL, LTBOOL)
{
	XboxFont *pFont = new XboxFont;
	pFont->m_Height = height > 0 ? height : 12;
	pFont->m_Width = width > 0 ? width : (pFont->m_Height * 6) / 10;
	pFont->m_Extra = 0;
	return (HLTFONT)pFont;
}

static void tmgr_DeleteFont(HLTFONT hFont)
{
	delete (XboxFont *)hFont;
}

static LTRESULT tmgr_SetFontExtraSpace(HLTFONT hFont, int pixels)
{
	if (!hFont)
		return LT_ERROR;
	((XboxFont *)hFont)->m_Extra = pixels;
	return LT_OK;
}

static LTRESULT tmgr_GetFontExtraSpace(HLTFONT hFont, int &pixels)
{
	if (!hFont)
		return LT_ERROR;
	pixels = ((XboxFont *)hFont)->m_Extra;
	return LT_OK;
}

static void tmgr_GetStringDimensions(HLTFONT hFont, HSTRING hString, int *sizeX, int *sizeY)
{
	XboxFont *pFont = (XboxFont *)hFont;
	int nBytes = 0;
	uint8 *pBytes = hString ? str_GetStringBytes(hString, &nBytes) : LTNULL;
	int len = pBytes ? (int)strlen((const char *)pBytes) : 0;
	if (sizeX)
		*sizeX = pFont ? len * (pFont->m_Width + pFont->m_Extra) : 0;
	if (sizeY)
		*sizeY = pFont ? pFont->m_Height : 0;
}

static void tmgr_DrawStringToSurface(HSURFACE, HLTFONT, HSTRING, LTRect *, HLTCOLOR, HLTCOLOR) {}

static HSURFACE tmgr_CreateSurfaceFromString(HLTFONT, HSTRING, HLTCOLOR, HLTCOLOR, int, int)
{
	return LTNULL;
}

void tmgr_Init(ILTClient *pClientDE)
{
	pClientDE->CreateFont = tmgr_CreateFont;
	pClientDE->DeleteFont = tmgr_DeleteFont;
	pClientDE->SetFontExtraSpace = tmgr_SetFontExtraSpace;
	pClientDE->GetFontExtraSpace = tmgr_GetFontExtraSpace;
	pClientDE->GetStringDimensions = tmgr_GetStringDimensions;
	pClientDE->DrawStringToSurface = tmgr_DrawStringToSurface;
	pClientDE->CreateSurfaceFromString = tmgr_CreateSurfaceFromString;
}

void tmgr_Term() {}
