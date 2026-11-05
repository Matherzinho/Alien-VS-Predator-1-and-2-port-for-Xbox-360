// avp360: Opcoes compartilhadas entre o Launcher, AvP 1 (3dc) e AvP 2 (LithTech 2.2)
#ifndef AVP_OPTIONS_H
#define AVP_OPTIONS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	O_LANGUAGE,          // 0: Portugues, 1: English, 2: Espanol, 3: Francais, 4: Deutsch
	O_SPECIES,           // 0: Menu do jogo, 1: Marine, 2: Alien, 3: Predator
	O_DIFFICULTY,        // 0: Nao mudar, 1: Facil, 2: Medio, 3: Dificil, 4: Diretor
	O_UNLIMITED_SAVES,   // AvP 1 QoL: Saves ilimitados em todas as dificuldades
	O_UNLOCK_LEVELS,     // QoL: Desbloquear todas as fases das 3 campanhas
	O_TRAINING,          // QoL: Vida / Energia / Municao infinita
	O_FILTER,            // 0: Bilinear nitido, 1: Pixels nitidos, 2: Bilinear comum, 3: Catmull-Rom, 4: Scale3x
	O_WIDE,              // 0: 16:9 Widescreen, 1: 4:3 Original
	O_FOV,               // 0: 75, 1: 85, 2: 90, 3: 100, 4: 110
	O_GAMMA,             // 0: Padrao, 1: +15%, 2: +30%, 3: +45%
	O_LINES,             // 0: Nao, 1: Leves, 2: Medias, 3: Fortes, 4: CRT
	O_FPS,               // 0: Nao, 1: Sim
	O_SENSITIVITY,       // 0: Baixa, 1: Media, 2: Alta, 3: Muito alta
	O_INVERT_Y,          // 0: Nao, 1: Sim
	O_AIM_ASSIST,        // 0: Nao, 1: Leve, 2: Forte
	O_LAYOUT,            // 0: Padrao Moderno (RT Atira / LT Alt), 1: Classico Xbox, 2: Tatico
	O_RUMBLE,            // 0: Nao, 1: Sim
	O_VOLUME,            // 0..10 (0% a 100%)
	OPTIONS
};

enum { LANG_PT, LANG_EN, LANG_ES, LANGS };
enum { FILTER_SHARP, FILTER_NEAREST, FILTER_SOFT, FILTER_CUBIC, FILTER_SCALE3X, FILTERS };
enum { PAUSE_RESUME, PAUSE_OTHER_GAME, PAUSE_QUIT, PAUSE_ACTIONS };

extern int avp_opt[OPTIONS];

// Traduz string base em Portugues para o idioma atual
const char *T(const char *pt);

// Retorna o FOV horizontal/vertical escolhido em graus
int avp_option_fov_degrees(void);

// Retorna multiplicador de sensibilidade do analogico
float avp_option_sensitivity_scale(void);

// Retorna multiplicador de brilho/gamma (1.00, 1.15, 1.30, 1.45)
static inline float avp_option_gamma_scale(void)
{
	int g = avp_opt[O_GAMMA];
	if (g < 0) g = 0;
	if (g > 3) g = 3;
	return 1.0f + (float)g * 0.15f;
}

// Dispara vibracao no controle do Xbox 360 (se O_RUMBLE estiver ativo)
void avp_trigger_rumble(uint16_t left_motor, uint16_t right_motor, int frames);
void avp_stop_rumble(void);

#ifdef __cplusplus
}
#endif

#endif // AVP_OPTIONS_H
