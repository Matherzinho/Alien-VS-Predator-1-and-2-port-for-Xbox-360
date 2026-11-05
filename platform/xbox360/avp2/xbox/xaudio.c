/* avp360: o thread alimentador do audio (ver xaudio.h). O kernel pede um quadro de 256 amostras a cada 5,33 ms; o
 * mixer do Miles (mss_xbox.cpp) preenche esquerda e direita, e os outros 4 canais ficam em silencio. */
#include <pthread.h>
#include <string.h>
#include <xsdk/audio.h>
#include <xsdk/kernel.h>
#include <xsdk/log.h>
#include "xaudio.h"

static XAudioMixFn g_mix;
static void *g_user;

static void *feeder(void *unused)
{
	(void)unused;
	float left[XAUDIO_FRAME], right[XAUDIO_FRAME];
	for (;;) {
		float *frame = NULL;
		NTSTATUS st = xsdk_audio_next_frame(1000, &frame);
		if (st == STATUS_INVALID_HANDLE)
			break;
		if (!NT_SUCCESS(st) || !frame)
			continue;
		g_mix(left, right, g_user);
		memcpy(frame + XSDK_AUDIO_FRONT_LEFT * XSDK_AUDIO_FRAME_SAMPLES, left, sizeof left);
		memcpy(frame + XSDK_AUDIO_FRONT_RIGHT * XSDK_AUDIO_FRAME_SAMPLES, right, sizeof right);
		for (int c = 0; c < XSDK_AUDIO_CHANNELS; c++) {
			if (c != XSDK_AUDIO_FRONT_LEFT && c != XSDK_AUDIO_FRONT_RIGHT)
				memset(frame + c * XSDK_AUDIO_FRAME_SAMPLES, 0, XSDK_AUDIO_FRAME_SAMPLES * sizeof(float));
		}
		xsdk_audio_queue_frame();
	}
	return NULL;
}

int xaudio_start(XAudioMixFn mix, void *user)
{
	static int started;
	if (started)
		return 1;
	g_mix = mix;
	g_user = user;
	NTSTATUS st = xsdk_audio_open(0);
	if (!NT_SUCCESS(st)) {
		xsdk_log_write("avp2 audio: xsdk_audio_open falhou: 0x%08X", (unsigned)st);
		return 0;
	}
	pthread_t thread;
	if (pthread_create(&thread, NULL, feeder, NULL) != 0) {
		xsdk_audio_close();
		return 0;
	}
	pthread_detach(thread);
	started = 1;
	xsdk_log_write("avp2 audio: pronto (48 kHz)");
	return 1;
}
