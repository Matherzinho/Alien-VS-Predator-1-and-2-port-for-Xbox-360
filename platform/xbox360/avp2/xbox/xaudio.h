/* avp360: a saida de audio do AvP2 (xaudio.c, sobre o xsdk_audio). So tipos do C: o kernel.h do xsdk nao convive com
 * o windows.h do shim. */
#ifndef AVP360_XAUDIO_H
#define AVP360_XAUDIO_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { XAUDIO_RATE = 48000, XAUDIO_FRAME = 256 };

/* Chamado no thread de audio a cada quadro: preenche left/right com XAUDIO_FRAME amostras (-1..1). */
typedef void (*XAudioMixFn)(float *left, float *right, void *user);

/* Abre o audio e comeca o thread alimentador; 0 se nao der. */
int xaudio_start(XAudioMixFn mix, void *user);

#ifdef __cplusplus
}
#endif
#endif
