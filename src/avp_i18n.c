// avp360: Tabelas de traducao in-game (Portugues PT-BR, Espanol, English) para AvP 1 e AvP 2
#include "avp_i18n.h"
#include "avp_options.h"
#include "3dc/avp/langenum.h"
#include "proj/AVP2/ClientRes/ClientRes.h"

#include <stddef.h>

typedef struct {
	int id;
	const char *pt_br;
	const char *es;
} i18n_entry_t;

// Traducoes dos menus, HUD, armas, fases, briefings e mensagens de sistema do Aliens versus Predator 1 (1999 Gold)
static const i18n_entry_t avp1_strings[] = {
	// Menu Principal
	{ TEXTSTRING_MAINMENU_TITLE,                "ALIENS VERSUS PREDATOR",                 "ALIENS VERSUS PREDATOR" },
	{ TEXTSTRING_MAINMENU_SUBTITLE,             "Edi\xc3\xa7\xc3\xa3o Xbox 360",          "Edici\xc3\xb3n Xbox 360" },
	{ TEXTSTRING_MAINMENU_STARTSINGLEPLAYER,    "Iniciar Campanha",                       "Iniciar Campa\xc3\xb1""a" },
	{ TEXTSTRING_MAINMENU_STARTSINGLEPLAYER_HELP, "Jogue como Marine, Alien ou Predator", "Juega como Marine, Alien o Predator" },
	{ TEXTSTRING_MAINMENU_SINGLEPLAYER_ALIEN,   "Alien",                                  "Alien" },
	{ TEXTSTRING_MAINMENU_SINGLEPLAYER_MARINE,  "Colonial Marine",                        "Marine Colonial" },
	{ TEXTSTRING_MAINMENU_SINGLEPLAYER_PREDATOR,"Predator",                               "Predator" },
	{ TEXTSTRING_MAINMENU_MULTIPLAYER,          "Multijogador",                           "Multijugador" },
	{ TEXTSTRING_MAINMENU_SKIRMISH,             "Modo Escaramu\xc3\xa7""a (Skirmish)",    "Modo Escaramuza (Skirmish)" },
	{ TEXTSTRING_MAINMENU_GAMEPLAYOPTIONS,      "Op\xc3\xa7\xc3\xb5""es de Jogo",         "Opciones de Juego" },
	{ TEXTSTRING_MAINMENU_CHANGEUSERPROFILE,    "Trocar Perfil de Jogador",               "Cambiar Perfil de Jugador" },
	{ TEXTSTRING_MAINMENU_CHEATOPTIONS,         "Trapa\xc3\xa7""as e Extras",             "Trucos y Extras" },
	{ TEXTSTRING_MAINMENU_EXITGAME,             "Sair para o Seletor AvP 360",            "Salir al Selector AvP 360" },

	// Menu In-Game (Pausa)
	{ TEXTSTRING_INGAMEMENU_RESUMEGAME,         "Continuar Jogo",                         "Continuar Juego" },
	{ TEXTSTRING_INGAMEMENU_RESTARTMISSION,     "Reiniciar Miss\xc3\xa3o",                "Reiniciar Misi\xc3\xb3n" },
	{ TEXTSTRING_INGAMEMENU_QUITTOMAINMENU,     "Sair para o Menu Principal",             "Salir al Men\xc3\xba Principal" },

	// Dificuldade e Selecao de Fase
	{ TEXTSTRING_LEVELSELECT_TITLE,             "Selecionar Miss\xc3\xa3o",               "Seleccionar Misi\xc3\xb3n" },
	{ TEXTSTRING_LEVELSELECT_START,             "Iniciar Miss\xc3\xa3o",                  "Iniciar Misi\xc3\xb3n" },
	{ TEXTSTRING_DIFFICULTY_LEVEL,              "N\xc3\xadvel de Dificuldade",            "Nivel de Dificultad" },
	{ TEXTSTRING_DIFFICULTY_EASY,               "Treinamento (F\xc3\xa1""cil)",           "Entrenamiento (F\xc3\xa1""cil)" },
	{ TEXTSTRING_DIFFICULTY_MEDIUM,             "Realista (M\xc3\xa9""dio)",              "Realista (Medio)" },
	{ TEXTSTRING_DIFFICULTY_HARD,               "Vers\xc3\xa3o do Diretor (Dif\xc3\xad""cil)", "Versi\xc3\xb3n del Director (Dif\xc3\xad""cil)" },
	{ TEXTSTRING_NOTYETCOMPLETED,               "Ainda n\xc3\xa3o conclu\xc3\xaddo",      "A\xc3\xban no completado" },
	{ TEXTSTRING_COMPLETED_ON_EASY,             "Conclu\xc3\xaddo no F\xc3\xa1""cil",     "Completado en F\xc3\xa1""cil" },
	{ TEXTSTRING_COMPLETED_ON_MEDIUM,           "Conclu\xc3\xaddo no M\xc3\xa9""dio",     "Completado en Medio" },
	{ TEXTSTRING_COMPLETED_ON_HARD,             "Conclu\xc3\xaddo no Dif\xc3\xad""cil",   "Completado en Dif\xc3\xad""cil" },
	{ TEXTSTRING_COMPLETED,                     "Conclu\xc3\xaddo",                       "Completado" },
	{ TEXTSTRING_CONTINUE,                      "Continuar",                              "Continuar" },
	{ TEXTSTRING_NO,                            "N\xc3\xa3o",                             "No" },
	{ TEXTSTRING_YES,                           "Sim",                                    "S\xc3\xad" },
	{ TEXTSTRING_MISSION_ACCOMPLISHED,          "MISS\xc3\x83O CUMPRIDA",                 "MISI\xc3\x93N CUMPLIDA" },
	{ TEXTSTRING_INGAME_PRESSANYKEYTOCONTINUE,  "Pressione A ou Start para continuar",    "Pulsa A o Start para continuar" },

	// HUD, Armas e Status In-Game
	{ TEXTSTRING_INGAME_HEALTH,                 "VIDA",                                   "SALUD" },
	{ TEXTSTRING_INGAME_ARMOUR,                 "BLINDAGEM",                              "BLINDAJE" },
	{ TEXTSTRING_INGAME_AMMO,                   "MUNI\xc3\x87\xc3\x83O",                  "MUNICI\xc3\x93N" },
	{ TEXTSTRING_INGAME_MEDIKIT,                "Kit M\xc3\xa9""dico",                    "Botiqu\xc3\xadn" },
	{ TEXTSTRING_INGAME_PULSERIFLE,             "Rifle de Pulso M41A",                    "Rifle de Pulsos M41A" },
	{ TEXTSTRING_INGAME_SMARTGUN,               "Smartgun M56",                           "Smartgun M56" },
	{ TEXTSTRING_INGAME_FLAMETHROWER,           "Lan\xc3\xa7""a-Chamas M240",             "Lanzallamas M240" },
	{ TEXTSTRING_INGAME_SADAR,                  "Lan\xc3\xa7""a-Foguetes SADAR",          "Lanzacohetes SADAR" },
	{ TEXTSTRING_INGAME_GRENADELAUNCHER,        "Lan\xc3\xa7""a-Granadas",                "Lanzagranadas" },
	{ TEXTSTRING_INGAME_MINIGUN,                "Minigun",                                "Minigun" },
	{ TEXTSTRING_INGAME_MARINE_PISTOL,          "Pistola de Combate",                     "Pistola de Combate" },
	{ TEXTSTRING_INGAME_SKEETER,                "Lan\xc3\xa7""ador Skeeter",              "Lanzador Skeeter" },
	{ TEXTSTRING_INGAME_TWOPISTOLS,             "Pistolas Duplas",                        "Pistolas Dobles" },
	{ TEXTSTRING_INGAME_CLAW,                   "Garras",                                 "Garras" },
	{ TEXTSTRING_INGAME_TAIL,                   "Cauda",                                  "Cola" },
	{ TEXTSTRING_INGAME_WRISTBLADE,             "L\xc3\xa2minas de Pulso",                "Cuchillas de Mu\xc3\xb1""eca" },
	{ TEXTSTRING_INGAME_PISTOL,                 "Pistola de Plasma",                      "Pistola de Plasma" },
	{ TEXTSTRING_INGAME_RIFLE,                  "Lança-Arp\xc3\xb5""es (Speargun)",       "Fusil de Arpones (Speargun)" },
	{ TEXTSTRING_INGAME_SHOULDERCANNON,         "Canh\xc3\xa3o de Plasma de Ombro",       "Ca\xc3\xb1\xc3\xb3n de Plasma de Hombro" },
	{ TEXTSTRING_INGAME_DISC,                   "Disco de Batalha",                       "Disco de Combate" },
	{ TEXTSTRING_INGAME_MEDICOMP,               "Medicomp",                               "Medicomp" },
	{ TEXTSTRING_INGAME_INTENSIFIERON,          "Vis\xc3\xa3o Noturna ATIVADA",           "Visi\xc3\xb3n Nocturna ACTIVADA" },
	{ TEXTSTRING_INGAME_INTENSIFIEROFF,         "Vis\xc3\xa3o Noturna DESATIVADA",        "Visi\xc3\xb3n Nocturna DESACTIVADA" },
	{ TEXTSTRING_ROUNDS,                        "Proj\xc3\xa9teis",                       "Proyectiles" },
	{ TEXTSTRING_MAGAZINES,                     "Pentes",                                 "Cargadores" },
	{ TEXTSTRING_NOAMMOFORWEAPON,               "Sem muni\xc3\xa7\xc3\xa3o para esta arma", "Sin munici\xc3\xb3n para esta arma" },

	// Save / Load Game
	{ TEXTSTRING_LOADGAME,                      "Carregar Jogo",                          "Cargar Partida" },
	{ TEXTSTRING_SAVEGAME,                      "Salvar Jogo",                            "Guardar Partida" },
	{ TEXTSTRING_SAVEGAME_EMPTYSLOT,            "Espa\xc3\xa7o Vazio",                    "Ranura Vac\xc3\xada" },
	{ TEXTSTRING_SAVEGAME_ERRORSAVING,          "Erro ao salvar o jogo",                  "Error al guardar la partida" },
	{ TEXTSTRING_SAVEGAME_ERRORLOADING,         "Erro ao carregar o jogo",                "Error al cargar la partida" },
	{ TEXTSTRING_SAVEGAME_NOSAVESLEFT,          "Nenhum salvamento restante nesta miss\xc3\xa3o", "No quedan guardados en esta misi\xc3\xb3n" },
	{ TEXTSTRING_SAVEGAME_GAMESAVED,            "Jogo Salvo com Sucesso",                 "Partida Guardada" },
	{ TEXTSTRING_SAVEGAME_GAMELOADED,           "Jogo Carregado",                         "Partida Cargada" },
	{ TEXTSTRING_SAVEGAME_NOTALLOWED,           "Salvamento n\xc3\xa3o permitido agora",  "Guardado no permitido ahora" },
	{ TEXTSTRING_SAVEGAME_SAVESLEFT,            "Salvamentos Restantes",                  "Guardados Restantes" },

	// Episodios / Fases - Colonial Marine
	{ TEXTSTRING_MARINELEVELS_1,                "Ep. 1: Derelict (Nave Abandonada)",      "Ep. 1: Derelict (Nave Abandonada)" },
	{ TEXTSTRING_MARINELEVELS_2,                "Ep. 2: Colony (Col\xc3\xb4nia LV-426)",  "Ep. 2: Colony (Colonia LV-426)" },
	{ TEXTSTRING_MARINELEVELS_3,                "Ep. 3: Invasion (Invas\xc3\xa3o)",       "Ep. 3: Invasion (Invasi\xc3\xb3n)" },
	{ TEXTSTRING_MARINELEVELS_4,                "Ep. 4: Orbital (Esta\xc3\xa7\xc3\xa3o Orbital)", "Ep. 4: Orbital (Estaci\xc3\xb3n Orbital)" },
	{ TEXTSTRING_MARINELEVELS_5,                "Ep. 5: Tyrargo (USS Tyrargo)",           "Ep. 5: Tyrargo (USS Tyrargo)" },
	{ TEXTSTRING_MARINELEVELS_6,                "Ep. 6: Hangar (Confronto Final)",        "Ep. 6: Hangar (Enfrentamiento Final)" },
	{ TEXTSTRING_MARINELEVELS_7,                "B\xc3\xb4nus 1: Vaults (Cofres)",        "Bonus 1: Vaults (B\xc3\xb3vedas)" },
	{ TEXTSTRING_MARINELEVELS_8,                "B\xc3\xb4nus 2: Fury 161",               "Bonus 2: Fury 161" },
	{ TEXTSTRING_MARINELEVELS_9,                "B\xc3\xb4nus 3: Caverns (Cavernas)",     "Bonus 3: Caverns (Cavernas)" },
	{ TEXTSTRING_MARINELEVELS_10,               "B\xc3\xb4nus 4: Battlezone",             "Bonus 4: Battlezone" },
	{ TEXTSTRING_MARINELEVELS_11,               "B\xc3\xb4nus 5: Subway (Metr\xc3\xb4)",  "Bonus 5: Subway (Metro)" },

	// Episodios / Fases - Predator
	{ TEXTSTRING_PREDATORLEVELS_1,              "Ep. 1: Waterfall (Cachoeira)",           "Ep. 1: Waterfall (Cascada)" },
	{ TEXTSTRING_PREDATORLEVELS_2,              "Ep. 2: Area 52 (\xc3\x81rea 52)",        "Ep. 2: Area 52 (\xc3\x81rea 52)" },
	{ TEXTSTRING_PREDATORLEVELS_3,              "Ep. 3: Vaults (Cofres Subterr\xc3\xa2neos)", "Ep. 3: Vaults (B\xc3\xb3vedas Subterr\xc3\xa1neas)" },
	{ TEXTSTRING_PREDATORLEVELS_4,              "Ep. 4: Fury 161 (Refinaria)",            "Ep. 4: Fury 161 (Refiner\xc3\xada)" },
	{ TEXTSTRING_PREDATORLEVELS_5,              "Ep. 5: Caverns (Ninho Xenomorfo)",       "Ep. 5: Caverns (Nido Xenomorfo)" },
	{ TEXTSTRING_PREDATORLEVELS_6,              "Ep. 6: Battlezone (Zona de Guerra)",     "Ep. 6: Battlezone (Zona de Guerra)" },
	{ TEXTSTRING_PREDATORLEVELS_7,              "B\xc3\xb4nus 1: Infiltration",           "Bonus 1: Infiltration" },
	{ TEXTSTRING_PREDATORLEVELS_8,              "B\xc3\xb4nus 2: Colony (Col\xc3\xb4nia)","Bonus 2: Colony (Colonia)" },
	{ TEXTSTRING_PREDATORLEVELS_9,              "B\xc3\xb4nus 3: Shaft (Po\xc3\xa7o)",    "Bonus 3: Shaft (Pozo)" },
	{ TEXTSTRING_PREDATORLEVELS_10,             "B\xc3\xb4nus 4: Tyrargo",                "Bonus 4: Tyrargo" },
	{ TEXTSTRING_PREDATORLEVELS_11,             "B\xc3\xb4nus 5: Temple (Templo)",        "Bonus 5: Temple (Templo)" },

	// Episodios / Fases - Alien
	{ TEXTSTRING_ALIENLEVELS_1,                 "Ep. 1: Temple (Templo Natal)",           "Ep. 1: Temple (Templo Natal)" },
	{ TEXTSTRING_ALIENLEVELS_2,                 "Ep. 2: Escape (Fuga do Laborat\xc3\xb3rio)", "Ep. 2: Escape (Fuga del Laboratorio)" },
	{ TEXTSTRING_ALIENLEVELS_3,                 "Ep. 3: Ferarco (Nave Ferarco)",          "Ep. 3: Ferarco (Nave Ferarco)" },
	{ TEXTSTRING_ALIENLEVELS_4,                 "Ep. 4: Gateway (Esta\xc3\xa7\xc3\xa3o Gateway)", "Ep. 4: Gateway (Estaci\xc3\xb3n Gateway)" },
	{ TEXTSTRING_ALIENLEVELS_5,                 "Ep. 5: Earthbound (Rumo \xc3\xa0 Terra)","Ep. 5: Earthbound (Rumbo a la Tierra)" },
	{ TEXTSTRING_ALIENLEVELS_6,                 "B\xc3\xb4nus 1: Invasion",               "Bonus 1: Invasion" },
	{ TEXTSTRING_ALIENLEVELS_7,                 "B\xc3\xb4nus 2: Derelict",               "Bonus 2: Derelict" },
	{ TEXTSTRING_ALIENLEVELS_8,                 "B\xc3\xb4nus 3: Tyrargo",                "Bonus 3: Tyrargo" },
	{ TEXTSTRING_ALIENLEVELS_9,                 "B\xc3\xb4nus 4: Vaults",                 "Bonus 4: Vaults" },
	{ TEXTSTRING_ALIENLEVELS_10,                "B\xc3\xb4nus 5: Fury 161",               "Bonus 5: Fury 161" },

	// Modos de Visao e Estatisticas
	{ TEXTSTRING_GAMESTATS_VM_NORMAL,           "Vis\xc3\xa3o Normal",                    "Visi\xc3\xb3n Normal" },
	{ TEXTSTRING_GAMESTATS_VM_NAVSENSE,         "Sentido de Navega\xc3\xa7\xc3\xa3o Alien", "Sentido de Navegaci\xc3\xb3n Alien" },
	{ TEXTSTRING_GAMESTATS_VM_INTENSIFIER,      "Intensificador de Imagem",               "Intensificador de Imagen" },
	{ TEXTSTRING_GAMESTATS_VM_THERMAL,          "Vis\xc3\xa3o T\xc3\xa9rmica (Humanos)",  "Visi\xc3\xb3n T\xc3\xa9rmica (Humanos)" },
	{ TEXTSTRING_GAMESTATS_VM_ELECTRICAL,       "Vis\xc3\xa3n El\xc3\xa9trica (Xenomorfos)", "Visi\xc3\xb3n El\xc3\xa9ctrica (Xenomorfos)" },
	{ TEXTSTRING_GAMESTATS_VM_PREDTECH,         "Vis\xc3\xa3o PredTech",                  "Visi\xc3\xb3n PredTech" },
};

// Traducoes dos menus, HUD e legendas do Aliens versus Predator 2 (2001 Monolith)
static const i18n_entry_t avp2_strings[] = {
	{ IDS_TITLE_MAINMENU,        "MENU PRINCIPAL",                    "MEN\xc3\x9a PRINCIPAL" },
	{ IDS_TITLE_SINGLE,          "UM JOGADOR",                        "UN JUGADOR" },
	{ IDS_TITLE_OPTIONS,         "OP\xc3\x87\xc3\x95""ES",            "OPCIONES" },
	{ IDS_TITLE_LOADGAME,        "CARREGAR JOGO",                     "CARGAR PARTIDA" },
	{ IDS_TITLE_SAVEGAME,        "SALVAR JOGO",                       "GUARDAR PARTIDA" },
	{ IDS_QUICKSAVING,           "Salvamento R\xc3\xa1pido...",       "Guardado R\xc3\xa1pido..." },
	{ IDS_SINGLEPLAYER,          "Campanha (Um Jogador)",             "Campa\xc3\xb1""a (Un Jugador)" },
	{ IDS_MULTIPLAYER,           "Multijogador",                      "Multijugador" },
	{ IDS_OPTIONS,               "Op\xc3\xa7\xc3\xb5""es",            "Opciones" },
	{ IDS_EXIT,                  "Sair para o Seletor AvP 360",       "Salir al Selector AvP 360" },
	{ IDS_LOADGAME,              "Carregar Jogo",                     "Cargar Partida" },
	{ IDS_SAVEGAME,              "Salvar Jogo",                       "Guardar Partida" },
	{ IDS_DISPLAY,               "V\xc3\xad""deo",                    "V\xc3\xad""deo" },
	{ IDS_SOUND,                 "\xc3\x81udio",                      "Audio" },
	{ IDS_BACK,                  "Voltar",                            "Volver" },
	{ IDS_ON,                    "Ligado",                            "Activado" },
	{ IDS_OFF,                   "Desligado",                         "Desactivado" },
	{ IDS_YES,                   "Sim",                               "S\xc3\xad" },
	{ IDS_NO,                    "N\xc3\xa3o",                        "No" },
	{ IDS_NONE,                  "Nenhum",                            "Ninguno" },
	{ IDS_ALL,                   "Todos",                             "Todos" },
	{ IDS_RESTOREDEFAULTS,       "Restaurar Padr\xc3\xb5""es",        "Restaurar Valores" },
	{ IDS_CONTINUE,              "Continuar",                         "Continuar" },
	{ IDS_START_MISSION,         "Iniciar Miss\xc3\xa3o",             "Iniciar Misi\xc3\xb3n" },
	{ IDS_SELECT,                "Selecionar",                        "Seleccionar" },
	{ IDS_EMPTY_SLOT,            "Espa\xc3\xa7o Vazio",               "Ranura Vac\xc3\xada" },
	{ IDS_TITLE_ALIEN,           "CAMPANHA ALIEN",                    "CAMPA\xc3\x91""A ALIEN" },
	{ IDS_TITLE_MARINE,          "CAMPANHA MARINE",                   "CAMPA\xc3\x91""A MARINE" },
	{ IDS_TITLE_PREDATOR,        "CAMPANHA PREDATOR",                 "CAMPA\xc3\x91""A PREDATOR" },
	{ IDS_SELECT_MISSION,        "Selecionar Miss\xc3\xa3o",          "Seleccionar Misi\xc3\xb3n" },
	{ IDS_SPECIES,               "Esp\xc3\xa9""cie",                  "Especie" },
	{ IDS_MISSION,               "Miss\xc3\xa3o",                     "Misi\xc3\xb3n" },
	{ IDS_CONFIRMSAVE,           "Sobrescrever este jogo salvo?",     "\xc2\xbfSobrescribir esta partida guardada?" },
	{ IDS_TITLE_PAUSE,           "JOGO PAUSADO",                      "JUEGO EN PAUSA" },
	{ IDS_SUBTITLES,             "Legendas",                          "Subt\xc3\xadtulos" },
};

const char *avp1_i18n_get(int textstring_id)
{
	const int lang = avp_opt[O_LANGUAGE];
	if (lang == LANG_EN) return NULL;
	for (size_t i = 0; i < sizeof avp1_strings / sizeof avp1_strings[0]; i++) {
		if (avp1_strings[i].id == textstring_id)
			return (lang == LANG_ES) ? avp1_strings[i].es : avp1_strings[i].pt_br;
	}
	return NULL;
}

const char *avp2_i18n_get(int ids_id)
{
	const int lang = avp_opt[O_LANGUAGE];
	if (lang == LANG_EN) return NULL;
	for (size_t i = 0; i < sizeof avp2_strings / sizeof avp2_strings[0]; i++) {
		if (avp2_strings[i].id == ids_id)
			return (lang == LANG_ES) ? avp2_strings[i].es : avp2_strings[i].pt_br;
	}
	return NULL;
}
