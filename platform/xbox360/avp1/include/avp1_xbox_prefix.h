/* avp360: incluido antes de todo arquivo do AvP1 (-include). Ajustes de plataforma para xsdk/PowerPC. */
#ifndef AVP1_XBOX_PREFIX_H
#define AVP1_XBOX_PREFIX_H

#include <limits.h>
#include "avp1_endian.h"
#ifndef PATH_MAX
#define PATH_MAX 260
#endif

/* Os dados do jogo moram em game:\avp1. O path.c do xsdk converte caminhos POSIX relativos a game:\. */
#define AVP1_GAME_ROOT "avp1"

/* A pilha da thread principal de um .xex do xsdk e de 1 MB; o motor foi escrito para a do PC. O main do NakedAVP
 * vira avp1_game_main e roda numa thread com pilha grande (avp1_entry.c). */
#define main avp1_game_main

#ifdef __cplusplus
extern "C" {
#endif
void avp1_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
/* Marca de etapa da inicializacao (frontend/main.c): a ultima no log mostra onde um travamento aconteceu. */
#define AVP360_STEP(s) avp1_log("etapa: %s", s)
/* xenos_gl.c: o subconjunto de OpenGL que o renderizador do AvP usa, sobre xsdk_gpu. */
int xsdk_launch(const char *path, const void *data, size_t size_bytes);
int xgl_open(void);
void xgl_swap(void);
void xgl_screen_size(int *width, int *height);
#ifdef __cplusplus
}
#endif

#endif
