/*
 * avp360 v2.0: Motor de Audio 48 kHz de 6 Canais para Xbox 360 (xsdk/audio.h)
 * - Decodificador RIFF/WAVE PCM Little-Endian (8-bit e 16-bit, Mono/Stereo, interpolacao linear -> 48 kHz)
 * - Leitor nativo de HSound\fastfile\common.ffl (RebSnd) com os efeitos .WAV originais da Rebellion (1999)
 * - Streaming em tempo real das faixas de CD Audio (Track 1.wav .. Track 15.wav) do Disco 2
 * - Sintetizador procedural multi-harmonico de fallback para todos os efeitos sonoros
 */
#include "avp_audio.h"
#include "avp_options.h"
#include "avp_endian.h"
#include <xsdk/audio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

extern FILE *xbox360_fopen(const char *path, const char *mode);
#undef fopen
#define fopen xbox360_fopen

#define MAX_VOICES       12
#define MAX_REBSND_SLOTS 256
#define MUSIC_CHUNK_FRAMES 2048

typedef struct {
    const uint8_t *pcm_data;
    uint32_t       pcm_bytes;
    uint32_t       num_frames;
    uint32_t       sample_rate;
    uint16_t       channels;
    uint16_t       bits_per_sample;
} wav_slice_t;

typedef struct {
    bool         active;
    bool         is_pcm;
    avp_sfx_id_t type;
    /* Estado PCM (.WAV real) */
    wav_slice_t  wav;
    double       pcm_cursor;
    double       pcm_step;
    /* Estado Procedural (fallback) */
    uint32_t     pos;
    uint32_t     length;
    float        phase;
    float        phase2;
    float        freq;
    float        freq_end;
    /* Ganho e Panoramica */
    float        amp;
    float        pan; /* -1.0 = left, 0.0 = center, +1.0 = right */
} sfx_voice_t;

static bool        g_audio_open = false;
static sfx_voice_t g_voices[MAX_VOICES];
static uint32_t    g_rng = 0xA53C9E17u;

/* Banco de .WAVs carregados de HSound\fastfile\common.ffl (RebSnd) */
static uint8_t    *g_rebsnd_buf = NULL;
static size_t      g_rebsnd_size = 0;
static wav_slice_t g_sid_wavs[MAX_REBSND_SLOTS];
static uint32_t    g_loaded_wav_count = 0;

/* Streaming de Musica CD (Track 1.wav .. Track 15.wav) */
static FILE       *g_music_file = NULL;
static int         g_music_track = 0;
static uint32_t    g_music_data_offset = 0;
static uint32_t    g_music_data_bytes = 0;
static uint32_t    g_music_bytes_left = 0;
static uint32_t    g_music_rate = 44100;
static uint16_t    g_music_channels = 2;
static uint16_t    g_music_bits = 16;
static uint16_t    g_music_block_align = 4;
static uint8_t     g_music_buf[MUSIC_CHUNK_FRAMES * 4];
static uint32_t    g_music_buf_frames = 0;
static double      g_music_cursor = 0.0;
static double      g_music_step = 44100.0 / 48000.0;

static inline float next_noise(void) {
    g_rng = g_rng * 1664525u + 1013904223u;
    return ((float)((int32_t)(g_rng >> 8)) / 8388608.0f);
}

/* Faz o parse de um buffer RIFF/WAVE Little-Endian em memoria */
static bool parse_wav_le(const uint8_t *data, size_t size, wav_slice_t *out) {
    if (!data || size < 44 || !out) return false;
    if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0) return false;

    uint16_t fmt_tag = 0, channels = 0, bits = 0, block_align = 0;
    uint32_t sample_rate = 0;
    const uint8_t *pcm_ptr = NULL;
    uint32_t pcm_len = 0;

    size_t pos = 12;
    while (pos + 8 <= size) {
        const uint8_t *chunk = data + pos;
        uint32_t chunk_len = load_le32(chunk + 4);
        pos += 8;
        if (pos + chunk_len > size) {
            if (memcmp(chunk, "data", 4) == 0 && pos < size) {
                chunk_len = (uint32_t)(size - pos);
            } else {
                break;
            }
        }
        if (memcmp(chunk, "fmt ", 4) == 0 && chunk_len >= 16) {
            fmt_tag     = load_le16(data + pos + 0);
            channels    = load_le16(data + pos + 2);
            sample_rate = load_le32(data + pos + 4);
            block_align = load_le16(data + pos + 12);
            bits        = load_le16(data + pos + 14);
        } else if (memcmp(chunk, "data", 4) == 0) {
            pcm_ptr = data + pos;
            pcm_len = chunk_len;
            break;
        }
        pos += (chunk_len + 1u) & ~1u;
    }

    if (fmt_tag != 1 || !pcm_ptr || pcm_len == 0) return false;
    if ((channels != 1 && channels != 2) || (bits != 8 && bits != 16)) return false;
    if (sample_rate < 4000 || sample_rate > 96000) return false;

    uint32_t frame_bytes = (uint32_t)channels * ((uint32_t)bits / 8u);
    if (frame_bytes == 0) return false;
    (void)block_align;

    out->pcm_data        = pcm_ptr;
    out->pcm_bytes       = pcm_len;
    out->num_frames      = pcm_len / frame_bytes;
    out->sample_rate     = sample_rate;
    out->channels        = channels;
    out->bits_per_sample = bits;
    return out->num_frames > 1;
}

static inline void sample_wav_frame(const wav_slice_t *w, uint32_t frame_idx, float *out_l, float *out_r) {
    if (frame_idx >= w->num_frames) frame_idx = w->num_frames - 1;
    if (w->bits_per_sample == 16) {
        if (w->channels == 2) {
            const uint8_t *p = w->pcm_data + frame_idx * 4u;
            int16_t sl = (int16_t)load_le16(p);
            int16_t sr = (int16_t)load_le16(p + 2);
            *out_l = (float)sl * (1.0f / 32768.0f);
            *out_r = (float)sr * (1.0f / 32768.0f);
        } else {
            const uint8_t *p = w->pcm_data + frame_idx * 2u;
            int16_t s = (int16_t)load_le16(p);
            *out_l = *out_r = (float)s * (1.0f / 32768.0f);
        }
    } else {
        if (w->channels == 2) {
            const uint8_t *p = w->pcm_data + frame_idx * 2u;
            *out_l = ((int)p[0] - 128) * (1.0f / 128.0f);
            *out_r = ((int)p[1] - 128) * (1.0f / 128.0f);
        } else {
            uint8_t u = w->pcm_data[frame_idx];
            *out_l = *out_r = ((int)u - 128) * (1.0f / 128.0f);
        }
    }
}

bool avp_audio_load_rebsnd_archive(const char *path) {
    if (g_loaded_wav_count > 0 && g_rebsnd_buf != NULL) return true;

    static const char *const candidates[] = {
        "HSound\\fastfile\\common.ffl",
        "LSound\\fastfile\\common.ffl",
        "fastfile\\common.ffl",
        "game:\\avp1\\HSound\\fastfile\\common.ffl",
        "game:\\avp1\\LSound\\fastfile\\common.ffl"
    };

    FILE *f = NULL;
    if (path && path[0]) {
        f = fopen(path, "rb");
    }
    for (size_t i = 0; !f && i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        f = fopen(candidates[i], "rb");
    }
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long fsz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsz < 64 || fsz > 16 * 1024 * 1024) {
        fclose(f);
        return false;
    }

    size_t sz = (size_t)fsz;
    uint8_t *buf = (uint8_t *)malloc(sz);
    if (!buf) {
        fclose(f);
        return false;
    }

    if (fread(buf, 1, sz, f) != sz) {
        free(buf);
        fclose(f);
        return false;
    }
    fclose(f);

    if (g_rebsnd_buf) free(g_rebsnd_buf);
    g_rebsnd_buf = buf;
    g_rebsnd_size = sz;
    memset(g_sid_wavs, 0, sizeof(g_sid_wavs));
    g_loaded_wav_count = 0;

    /* Formato .RebSnd (common.ffl): [uint8 soundIndex][int8 pitch][string wavName\0][RIFF WAVE...] */
    size_t pos = 0;
    while (pos + 16 < sz) {
        uint8_t sid = buf[pos++];
        int8_t pitch = (int8_t)buf[pos++];
        if (sid == 0xFF && pitch == -1) break;

        size_t name_end = pos;
        while (name_end < sz && buf[name_end] != 0) name_end++;
        if (name_end >= sz) break;
        pos = name_end + 1;

        if (pos + 12 > sz || memcmp(buf + pos, "RIFF", 4) != 0) break;
        uint32_t riff_len = load_le32(buf + pos + 4);
        size_t wav_total = 8u + (size_t)riff_len;
        if (pos + wav_total > sz) break;

        wav_slice_t slice;
        if (parse_wav_le(buf + pos, wav_total, &slice)) {
            g_sid_wavs[sid] = slice;
            g_loaded_wav_count++;
        }
        pos += wav_total;
    }

    return g_loaded_wav_count > 0;
}

uint32_t avp_audio_loaded_wav_count(void) {
    return g_loaded_wav_count;
}

bool avp_audio_play_music_track(int track_num) {
    if (track_num <= 0 || track_num > 15) return false;
    if (g_music_file != NULL && g_music_track == track_num) return true;

    avp_audio_stop_music();

    char p1[96], p2[96], p3[96], p4[96];
    snprintf(p1, sizeof(p1), "Track %d.wav", track_num);
    snprintf(p2, sizeof(p2), "music\\Track %d.wav", track_num);
    snprintf(p3, sizeof(p3), "game:\\avp1\\Track %d.wav", track_num);
    snprintf(p4, sizeof(p4), "game:\\avp1\\music\\Track %d.wav", track_num);

    FILE *f = fopen(p1, "rb");
    if (!f) f = fopen(p2, "rb");
    if (!f) f = fopen(p3, "rb");
    if (!f) f = fopen(p4, "rb");
    if (!f) return false;

    uint8_t hdr[256];
    size_t got = fread(hdr, 1, sizeof(hdr), f);
    if (got < 44 || memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) {
        fclose(f);
        return false;
    }

    uint16_t fmt_tag = 0, channels = 0, bits = 0, block_align = 0;
    uint32_t sample_rate = 0, data_off = 0, data_sz = 0;
    uint32_t pos = 12;
    while (pos + 8 <= (uint32_t)got) {
        uint32_t clen = load_le32(hdr + pos + 4);
        if (memcmp(hdr + pos, "fmt ", 4) == 0 && clen >= 16 && pos + 24 <= (uint32_t)got) {
            fmt_tag     = load_le16(hdr + pos + 8);
            channels    = load_le16(hdr + pos + 10);
            sample_rate = load_le32(hdr + pos + 12);
            block_align = load_le16(hdr + pos + 20);
            bits        = load_le16(hdr + pos + 22);
        } else if (memcmp(hdr + pos, "data", 4) == 0) {
            data_off = pos + 8;
            data_sz  = clen;
            break;
        }
        pos += 8 + ((clen + 1u) & ~1u);
    }
    (void)block_align;

    if (fmt_tag != 1 || data_off == 0 || data_sz < 1024 ||
        (channels != 1 && channels != 2) || (bits != 8 && bits != 16) ||
        sample_rate < 8000 || sample_rate > 96000) {
        fclose(f);
        return false;
    }

    fseek(f, (long)data_off, SEEK_SET);
    g_music_file        = f;
    g_music_track       = track_num;
    g_music_data_offset = data_off;
    g_music_data_bytes  = data_sz;
    g_music_bytes_left  = data_sz;
    g_music_rate        = sample_rate;
    g_music_channels    = channels;
    g_music_bits        = bits;
    g_music_block_align = (uint16_t)(channels * (bits / 8u));
    g_music_buf_frames  = 0;
    g_music_cursor      = 0.0;
    g_music_step        = (double)sample_rate / (double)XSDK_AUDIO_RATE_HZ;
    return true;
}

void avp_audio_stop_music(void) {
    if (g_music_file != NULL) {
        fclose(g_music_file);
        g_music_file = NULL;
    }
    g_music_track = 0;
    g_music_buf_frames = 0;
}

void avp_audio_init(void) {
    if (!g_audio_open) {
        memset(g_voices, 0, sizeof(g_voices));
        if (NT_SUCCESS(xsdk_audio_open(0))) {
            g_audio_open = true;
        }
    }
    avp_audio_load_rebsnd_archive(NULL);
}

void avp_audio_shutdown(void) {
    avp_audio_stop_music();
    if (g_rebsnd_buf) {
        free(g_rebsnd_buf);
        g_rebsnd_buf = NULL;
    }
    g_loaded_wav_count = 0;
    if (!g_audio_open) return;
    xsdk_audio_close();
    g_audio_open = false;
}

static sfx_voice_t *alloc_voice(void) {
    int slot = 0;
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!g_voices[i].active) {
            return &g_voices[i];
        }
        if (g_voices[i].pos > g_voices[slot].pos) slot = i;
    }
    return &g_voices[slot];
}

/* Mapeia cada avp_sfx_id_t para o SID_* correspondente em HSound\fastfile\common.ffl */
static int map_sfx_to_rebsnd_sid(avp_sfx_id_t sfx) {
    switch (sfx) {
        case AVP_SFX_MENU_MOVE:     return 200; /* SID_MENUS_CHANGE_ITEM */
        case AVP_SFX_MENU_SELECT:   return 199; /* SID_MENUS_SELECT_ITEM */
        case AVP_SFX_PULSE_RIFLE:   return 6;   /* SID_PULSE_START */
        case AVP_SFX_SMARTGUN:      return 80;  /* SID_SMART1 */
        case AVP_SFX_FLAMETHROWER:  return 75;  /* SID_INCIN_START */
        case AVP_SFX_SADAR:         return 144; /* SID_SADAR_FIRE */
        case AVP_SFX_PLASMA_CASTER: return 0;   /* SID_PRED_LAUNCHER */
        case AVP_SFX_ALIEN_CLAW:    return 94;  /* SID_ALIEN_JAW_ATTACK */
        case AVP_SFX_TRACKER_PING:  return 58;  /* SID_TRACKER_WHEEP */
        case AVP_SFX_VISION_MODE:   return 18;  /* SID_VISION_ON */
        case AVP_SFX_CLOAK:         return 204; /* SID_PRED_CLOAKON */
        case AVP_SFX_DOOR:          return 60;  /* SID_DOORSTART */
        case AVP_SFX_PICKUP:        return 38;  /* SID_PICKUP */
        case AVP_SFX_DAMAGE:        return 59;  /* SID_ACID_SPRAY / hit */
        case AVP_SFX_EXPLOSION:     return 85;  /* SID_NICE_EXPLOSION */
        default:                    return -1;
    }
}

void avp_audio_trigger_sfx(avp_sfx_id_t sfx, float pan, float pitch_scale) {
    if (!g_audio_open) return;
    if (pitch_scale <= 0.1f) pitch_scale = 1.0f;
    if (pitch_scale >  3.0f) pitch_scale = 3.0f;
    if (pan < -1.0f) pan = -1.0f;
    if (pan >  1.0f) pan =  1.0f;

    sfx_voice_t *v = alloc_voice();
    memset(v, 0, sizeof(*v));
    v->active = true;
    v->type   = sfx;
    v->pan    = pan;

    /* Se o .WAV original do AvP 1 estiver carregado de common.ffl, usa o som autentico! */
    int sid = map_sfx_to_rebsnd_sid(sfx);
    if (sid >= 0 && sid < MAX_REBSND_SLOTS && g_sid_wavs[sid].pcm_data != NULL) {
        v->is_pcm     = true;
        v->wav        = g_sid_wavs[sid];
        v->pcm_cursor = 0.0;
        v->pcm_step   = ((double)v->wav.sample_rate * (double)pitch_scale) / (double)XSDK_AUDIO_RATE_HZ;
        v->amp        = 0.72f;
        v->length     = v->wav.num_frames;
        return;
    }

    /* Fallback procedural multi-harmonico aprimorado */
    v->is_pcm = false;
    switch (sfx) {
        case AVP_SFX_MENU_MOVE:
            v->length   = 1800;
            v->freq     = 680.0f * pitch_scale;
            v->freq_end = 880.0f * pitch_scale;
            v->amp      = 0.24f;
            break;
        case AVP_SFX_MENU_SELECT:
            v->length   = 4200;
            v->freq     = 520.0f * pitch_scale;
            v->freq_end = 1040.0f * pitch_scale;
            v->amp      = 0.32f;
            break;
        case AVP_SFX_PULSE_RIFLE:
            v->length   = 3400;
            v->freq     = 360.0f * pitch_scale;
            v->freq_end = 90.0f;
            v->amp      = 0.48f;
            break;
        case AVP_SFX_SMARTGUN:
            v->length   = 4000;
            v->freq     = 480.0f * pitch_scale;
            v->freq_end = 130.0f;
            v->amp      = 0.50f;
            break;
        case AVP_SFX_FLAMETHROWER:
            v->length   = 7200;
            v->freq     = 140.0f * pitch_scale;
            v->freq_end = 75.0f;
            v->amp      = 0.42f;
            break;
        case AVP_SFX_SADAR:
            v->length   = 11000;
            v->freq     = 220.0f * pitch_scale;
            v->freq_end = 42.0f;
            v->amp      = 0.58f;
            break;
        case AVP_SFX_PLASMA_CASTER:
            v->length   = 7200;
            v->freq     = 1380.0f * pitch_scale;
            v->freq_end = 175.0f;
            v->amp      = 0.46f;
            break;
        case AVP_SFX_ALIEN_CLAW:
            v->length   = 4800;
            v->freq     = 940.0f * pitch_scale;
            v->freq_end = 250.0f;
            v->amp      = 0.42f;
            break;
        case AVP_SFX_TRACKER_PING:
            v->length   = 3200;
            v->freq     = 2180.0f * pitch_scale;
            v->freq_end = 2060.0f * pitch_scale;
            v->amp      = 0.28f;
            break;
        case AVP_SFX_VISION_MODE:
        case AVP_SFX_CLOAK:
            v->length   = 6000;
            v->freq     = 280.0f * pitch_scale;
            v->freq_end = 1480.0f * pitch_scale;
            v->amp      = 0.30f;
            break;
        case AVP_SFX_DOOR:
            v->length   = 8400;
            v->freq     = 150.0f * pitch_scale;
            v->freq_end = 95.0f * pitch_scale;
            v->amp      = 0.36f;
            break;
        case AVP_SFX_PICKUP:
            v->length   = 4800;
            v->freq     = 440.0f * pitch_scale;
            v->freq_end = 880.0f * pitch_scale;
            v->amp      = 0.32f;
            break;
        case AVP_SFX_DAMAGE:
            v->length   = 5400;
            v->freq     = 165.0f;
            v->freq_end = 52.0f;
            v->amp      = 0.52f;
            break;
        case AVP_SFX_EXPLOSION:
        default:
            v->length   = 14000;
            v->freq     = 115.0f;
            v->freq_end = 32.0f;
            v->amp      = 0.60f;
            break;
    }
}

void avp_audio_play_wav_le(const uint8_t *wav_data, size_t wav_size, float volume_scale) {
    if (!g_audio_open) return;
    if (volume_scale <= 0.0f) volume_scale = 1.0f;
    if (volume_scale >  2.0f) volume_scale = 2.0f;

    wav_slice_t slice;
    if (parse_wav_le(wav_data, wav_size, &slice)) {
        sfx_voice_t *v = alloc_voice();
        memset(v, 0, sizeof(*v));
        v->active     = true;
        v->is_pcm     = true;
        v->wav        = slice;
        v->pcm_cursor = 0.0;
        v->pcm_step   = (double)slice.sample_rate / (double)XSDK_AUDIO_RATE_HZ;
        v->amp        = 0.75f * volume_scale;
        v->pan        = 0.0f;
        v->length     = slice.num_frames;
        return;
    }
    avp_audio_trigger_sfx(AVP_SFX_MENU_SELECT, 0.0f, 1.0f);
}

static void refill_music_buffer(void) {
    if (g_music_file == NULL || g_music_block_align == 0) {
        g_music_buf_frames = 0;
        return;
    }
    if (g_music_bytes_left < g_music_block_align) {
        fseek(g_music_file, (long)g_music_data_offset, SEEK_SET);
        g_music_bytes_left = g_music_data_bytes;
    }
    uint32_t want_bytes = MUSIC_CHUNK_FRAMES * (uint32_t)g_music_block_align;
    if (want_bytes > sizeof(g_music_buf)) want_bytes = sizeof(g_music_buf);
    if (want_bytes > g_music_bytes_left) want_bytes = g_music_bytes_left;

    size_t got = fread(g_music_buf, 1, want_bytes, g_music_file);
    if (got < g_music_block_align) {
        fseek(g_music_file, (long)g_music_data_offset, SEEK_SET);
        g_music_bytes_left = g_music_data_bytes;
        g_music_buf_frames = 0;
        return;
    }
    g_music_bytes_left -= (uint32_t)got;
    g_music_buf_frames = (uint32_t)got / g_music_block_align;
    g_music_cursor = 0.0;
}

void avp_audio_pump(void) {
    if (!g_audio_open) return;

    /* Volume global das opcoes: O_VOLUME = 0..10 (0% a 100%, padrao 10 = 100%) */
    int vol_idx = avp_opt[O_VOLUME];
    if (vol_idx < 0)  vol_idx = 0;
    if (vol_idx > 10) vol_idx = 10;
    const float master_vol = (float)vol_idx * 0.10f;

    /* Preenche ate 4 blocos de 256 amostras por frame de video (60 Hz -> 800 amostras/frame @ 48 kHz) */
    for (int block = 0; block < 4; block++) {
        float *frame = NULL;
        if (!NT_SUCCESS(xsdk_audio_next_frame(0, &frame)) || !frame)
            break;

        float *fl = frame + XSDK_AUDIO_FRONT_LEFT  * XSDK_AUDIO_FRAME_SAMPLES;
        float *fr = frame + XSDK_AUDIO_FRONT_RIGHT * XSDK_AUDIO_FRAME_SAMPLES;
        memset(frame, 0, XSDK_AUDIO_FRAME_FLOATS * sizeof(float));

        if (master_vol > 0.0f) {
            for (uint32_t s = 0; s < XSDK_AUDIO_FRAME_SAMPLES; s++) {
                float mix_l = 0.0f;
                float mix_r = 0.0f;

                /* 1. Streaming de Musica CD (se ativa) */
                if (g_music_file != NULL) {
                    if ((uint32_t)g_music_cursor >= g_music_buf_frames) {
                        refill_music_buffer();
                    }
                    if (g_music_buf_frames > 0) {
                        wav_slice_t mw = {
                            .pcm_data        = g_music_buf,
                            .pcm_bytes       = g_music_buf_frames * g_music_block_align,
                            .num_frames      = g_music_buf_frames,
                            .sample_rate     = g_music_rate,
                            .channels        = g_music_channels,
                            .bits_per_sample = g_music_bits
                        };
                        float ml = 0.0f, mr = 0.0f;
                        sample_wav_frame(&mw, (uint32_t)g_music_cursor, &ml, &mr);
                        mix_l += ml * 0.25f * master_vol;
                        mix_r += mr * 0.25f * master_vol;
                        g_music_cursor += g_music_step;
                    }
                }

                /* 2. Mistura das vozes de SFX (.WAV reais + Sintetizador) */
                for (int i = 0; i < MAX_VOICES; i++) {
                    sfx_voice_t *v = &g_voices[i];
                    if (!v->active) continue;

                    float l_gain = (v->pan <= 0.0f) ? 1.0f : (1.0f - v->pan * 0.7f);
                    float r_gain = (v->pan >= 0.0f) ? 1.0f : (1.0f + v->pan * 0.7f);

                    if (v->is_pcm) {
                        uint32_t idx0 = (uint32_t)v->pcm_cursor;
                        if (idx0 >= v->wav.num_frames) {
                            v->active = false;
                            continue;
                        }
                        float frac = (float)(v->pcm_cursor - (double)idx0);
                        float l0 = 0.0f, r0 = 0.0f, l1 = 0.0f, r1 = 0.0f;
                        sample_wav_frame(&v->wav, idx0, &l0, &r0);
                        if (idx0 + 1 < v->wav.num_frames) {
                            sample_wav_frame(&v->wav, idx0 + 1, &l1, &r1);
                        } else {
                            l1 = l0;
                            r1 = r0;
                        }
                        float sl = (l0 + (l1 - l0) * frac) * v->amp * master_vol;
                        float sr = (r0 + (r1 - r0) * frac) * v->amp * master_vol;
                        mix_l += sl * l_gain;
                        mix_r += sr * r_gain;
                        v->pcm_cursor += v->pcm_step;
                        v->pos = idx0;
                        continue;
                    }

                    if (v->pos >= v->length) {
                        v->active = false;
                        continue;
                    }

                    float t = (float)v->pos / (float)v->length;
                    float env = 1.0f - t;
                    float cur_freq = v->freq + (v->freq_end - v->freq) * t;
                    v->phase += (6.2831853f * cur_freq) / (float)XSDK_AUDIO_RATE_HZ;
                    if (v->phase > 6.2831853f) v->phase -= 6.2831853f;
                    v->phase2 += (12.5663706f * cur_freq) / (float)XSDK_AUDIO_RATE_HZ;
                    if (v->phase2 > 6.2831853f) v->phase2 -= 6.2831853f;

                    float sig = 0.75f * sinf(v->phase) + 0.25f * sinf(v->phase2);
                    if (v->type == AVP_SFX_PULSE_RIFLE || v->type == AVP_SFX_SMARTGUN ||
                        v->type == AVP_SFX_DAMAGE || v->type == AVP_SFX_EXPLOSION || v->type == AVP_SFX_SADAR) {
                        sig = 0.45f * sig + 0.55f * next_noise();
                    } else if (v->type == AVP_SFX_FLAMETHROWER) {
                        sig = 0.20f * sig + 0.80f * next_noise() * env;
                    } else if (v->type == AVP_SFX_ALIEN_CLAW) {
                        sig = 0.35f * sig + 0.65f * next_noise() * env;
                    }

                    float sample = sig * v->amp * env * master_vol;
                    mix_l += sample * l_gain;
                    mix_r += sample * r_gain;
                    v->pos++;
                }

                if (mix_l >  0.95f) mix_l =  0.95f;
                if (mix_l < -0.95f) mix_l = -0.95f;
                if (mix_r >  0.95f) mix_r =  0.95f;
                if (mix_r < -0.95f) mix_r = -0.95f;
                fl[s] = mix_l;
                fr[s] = mix_r;
            }
        } else {
            for (int i = 0; i < MAX_VOICES; i++) {
                sfx_voice_t *v = &g_voices[i];
                if (!v->active) continue;
                if (v->is_pcm) {
                    v->pcm_cursor += v->pcm_step * (double)XSDK_AUDIO_FRAME_SAMPLES;
                    v->pos = (uint32_t)v->pcm_cursor;
                    if (v->pos >= v->wav.num_frames) v->active = false;
                } else {
                    v->pos += XSDK_AUDIO_FRAME_SAMPLES;
                    if (v->pos >= v->length) v->active = false;
                }
            }
        }

        xsdk_audio_queue_frame();
    }
}
