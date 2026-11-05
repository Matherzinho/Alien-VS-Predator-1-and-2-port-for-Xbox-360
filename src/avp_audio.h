/*
 * avp360: Motor de Audio 48 kHz de 6 Canais para Xbox 360 (xsdk/audio.h)
 * Suporta efeitos sonoros sintetizados iconicos (Rifle de Pulso M41A, Motion Tracker,
 * Plasma Caster do Predator, Ataque Xenomorfo, UI) e reproducao de .WAV PCM Little-Endian.
 */
#ifndef AVP_AUDIO_H
#define AVP_AUDIO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AVP_SFX_MENU_MOVE = 0,
    AVP_SFX_MENU_SELECT,
    AVP_SFX_PULSE_RIFLE,
    AVP_SFX_SMARTGUN,
    AVP_SFX_FLAMETHROWER,
    AVP_SFX_SADAR,
    AVP_SFX_PLASMA_CASTER,
    AVP_SFX_ALIEN_CLAW,
    AVP_SFX_TRACKER_PING,
    AVP_SFX_VISION_MODE,
    AVP_SFX_CLOAK,
    AVP_SFX_DOOR,
    AVP_SFX_PICKUP,
    AVP_SFX_DAMAGE,
    AVP_SFX_EXPLOSION,
    AVP_SFX_COUNT
} avp_sfx_id_t;

void avp_audio_init(void);
void avp_audio_shutdown(void);
void avp_audio_pump(void);

void avp_audio_trigger_sfx(avp_sfx_id_t sfx, float pan, float pitch_scale);
void avp_audio_play_wav_le(const uint8_t *wav_data, size_t wav_size, float volume_scale);

/* Carrega o arquivo HSound\fastfile\common.ffl (RebSnd) com os .WAV originais do AvP 1 */
bool avp_audio_load_rebsnd_archive(const char *path);
uint32_t avp_audio_loaded_wav_count(void);

/* Streaming de faixas de musica CD (Track 1.wav .. Track 15.wav) em 48 kHz */
bool avp_audio_play_music_track(int track_num);
void avp_audio_stop_music(void);

/* Aliases compativeis com psnd.h / psndplat.h do AvP 1 (Rebellion 3dc) */
#define SID_MENUS_CHANGE_ITEM AVP_SFX_MENU_MOVE
#define SID_MENUS_SELECT_ITEM AVP_SFX_MENU_SELECT
#define SID_PULSE_START       AVP_SFX_PULSE_RIFLE
#define SID_SMART1            AVP_SFX_SMARTGUN
#define SID_INCIN_START       AVP_SFX_FLAMETHROWER
#define SID_SADAR_FIRE        AVP_SFX_SADAR
#define SID_PRED_LAUNCHER     AVP_SFX_PLASMA_CASTER
#define SID_ALIEN_JAW_ATTACK  AVP_SFX_ALIEN_CLAW
#define SID_TRACKER_WHEEP     AVP_SFX_TRACKER_PING
#define SID_VISION_ON         AVP_SFX_VISION_MODE
#define SID_PRED_CLOAKON      AVP_SFX_CLOAK
#define SID_DOORSTART         AVP_SFX_DOOR
#define SID_PICKUP            AVP_SFX_PICKUP
#define SID_NICE_EXPLOSION    AVP_SFX_EXPLOSION

static inline void avp_audio_TriggerSound(int sid)
{
    avp_audio_trigger_sfx((avp_sfx_id_t)sid, 0.0f, 1.0f);
}

#ifdef __cplusplus
}
#endif

#endif /* AVP_AUDIO_H */
