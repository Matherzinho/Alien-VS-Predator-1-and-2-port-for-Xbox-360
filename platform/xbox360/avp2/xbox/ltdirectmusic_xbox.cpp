// avp360: ver ltdirectmusic_xbox.h.
#include "bdefs.h"
#include "ltdirectmusic_xbox.h"

static const struct { LTDMEnactTypes type; const char *name; } g_EnactNames[] = {
	{ LTDMEnactInvalid, "Invalid" }, { LTDMEnactDefault, "Default" }, { LTDMEnactImmediately, "Immediately" },
	{ LTDMEnactImmediately, "Immediate" }, { LTDMEnactNextBeat, "Beat" }, { LTDMEnactNextMeasure, "Measure" },
	{ LTDMEnactNextGrid, "Grid" }, { LTDMEnactNextSegment, "Segment" },
};

LTDMEnactTypes CLTDirectMusicMgr::StringToEnactType(const char *sName)
{
	if (!sName) return LTDMEnactInvalid;
	for (size_t i = 0; i < sizeof g_EnactNames / sizeof g_EnactNames[0]; i++)
		if (stricmp(sName, g_EnactNames[i].name) == 0) return g_EnactNames[i].type;
	return LTDMEnactInvalid;
}

void CLTDirectMusicMgr::EnactTypeToString(LTDMEnactTypes nType, char *sName)
{
	if (!sName) return;
	for (size_t i = 0; i < sizeof g_EnactNames / sizeof g_EnactNames[0]; i++)
		if (g_EnactNames[i].type == nType) { strcpy(sName, g_EnactNames[i].name); return; }
	strcpy(sName, "Invalid");
}
