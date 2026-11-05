// avp360: o Miles Sound System que o soundmgr do LithTech 2.2 usa, sobre o audio do Xbox 360 (xaudio.c).
//
// Um mixer de software estereo a 48 kHz, no thread de audio:
//  - amostras 2D: PCM de 8/16 bits mono/estereo (set_sample_address + set_sample_type) ou um WAV inteiro em memoria
//    (set_sample_file; IMA ADPCM e decodificado ao carregar), volume 0..127, pan 0..127, loops por bloco;
//  - amostras 3D: o mesmo, com o ganho pela distancia ao ouvinte como no Miles (cheio ate a distancia minima, depois
//    minima/distancia, mudo alem da maxima), pan pela orientacao do ouvinte e abafado pela obstrucao/oclusao;
//  - streams: o WAV lido inteiro do handle que o soundmgr passa no nome ("\\\\<handle>").
// O "provedor 3D" anuncia os nomes que o soundmgr conhece, todos o mesmo, para qualquer configuracao do PC servir.
// Ainda nao: MP3 (AIL_decompress_ASI), reverb/EAX, filtros.
//
// Os dados de som sao little-endian (WAV do PC): o mixer le as amostras de 16 bits byte a byte.
#include <windows.h>
#include <pthread.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mss.h"
#include "xaudio.h"

// ---------------------------------------------------------------- objetos

enum { OBJ_SAMPLE = 1, OBJ_SAMPLE3D, OBJ_STREAM, OBJ_LISTENER };
enum { MAX_OBJECTS = 160, USER_SLOTS = 8 };

struct XObj
{
	int kind;			// 0 = livre
	S32 user[USER_SLOTS];

	// Dados (PCM little-endian).
	const U8 *data;
	U32 len;
	U8 *owned;			// decodificado/lido por nos: liberado com o objeto
	int channels, bits;
	U32 rate;

	// Reproducao.
	U32 status;
	double pos;			// em quadros (amostras por canal)
	S32 loops;			// 1 = uma vez, 0 = sem fim, n = n vezes
	S32 loopStart, loopEnd;	// bytes; -1 = fim
	S32 volume, pan;
	bool paused;

	// 3D (amostras e ouvinte).
	float x, y, z;
	float fx, fy, fz, ux, uy, uz;
	float maxDist, minDist;
	float obstruction, occlusion;
};

static XObj g_Obj[MAX_OBJECTS];
static XObj *g_pListener;
static pthread_mutex_t g_Lock;
static pthread_once_t g_Once = PTHREAD_ONCE_INIT;
static S32 g_Master = 127;
static S32 g_Prefs[64];
static bool g_bOpen;

static void init_lock()
{
	pthread_mutexattr_t a;
	pthread_mutexattr_init(&a);
	pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&g_Lock, &a);
	pthread_mutexattr_destroy(&a);
	g_Prefs[DIG_MIXER_CHANNELS] = 64;
}

struct Guard
{
	Guard() { pthread_once(&g_Once, init_lock); pthread_mutex_lock(&g_Lock); }
	~Guard() { pthread_mutex_unlock(&g_Lock); }
};

static void obj_clear_data(XObj *o)
{
	free(o->owned);
	o->owned = NULL;
	o->data = NULL;
	o->len = 0;
}

static void obj_reset(XObj *o, int kind)
{
	obj_clear_data(o);
	memset(o, 0, sizeof *o);
	o->kind = kind;
	o->status = SMP_DONE;
	o->channels = 1;
	o->bits = 16;
	o->rate = 22050;
	o->loops = 1;
	o->loopEnd = -1;
	o->volume = 127;
	o->pan = 64;
	o->fz = 1.0f;
	o->uy = 1.0f;
	o->maxDist = 1000.0f;
	o->minDist = 1.0f;
}

static XObj *obj_alloc(int kind)
{
	Guard g;
	for (int i = 0; i < MAX_OBJECTS; i++)
	{
		if (!g_Obj[i].kind)
		{
			obj_reset(&g_Obj[i], kind);
			return &g_Obj[i];
		}
	}
	return NULL;
}

static void obj_free(XObj *o)
{
	if (!o)
		return;
	Guard g;
	obj_clear_data(o);
	o->kind = 0;
}

static inline XObj *O(void *h) { return (XObj *)h; }

// ---------------------------------------------------------------- WAV e ADPCM

static inline U16 le16(const U8 *p) { return (U16)(p[0] | (p[1] << 8)); }
static inline U32 le32(const U8 *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((U32)p[3] << 24); }

struct WavInfo
{
	U16 format, channels, bits, blockAlign;
	U32 rate;
	const U8 *data;
	U32 dataLen;
};

// Le os pedacos fmt e data de um WAV em memoria (RIFF little-endian, com LIST/fact pelo caminho).
static bool parse_wav(const U8 *p, U32 size, WavInfo *w)
{
	memset(w, 0, sizeof *w);
	if (size < 12 || memcmp(p, "RIFF", 4) != 0 || memcmp(p + 8, "WAVE", 4) != 0)
		return false;
	U32 pos = 12;
	bool fmt = false;
	while (pos + 8 <= size)
	{
		U32 len = le32(p + pos + 4);
		const U8 *c = p + pos + 8;
		if (memcmp(p + pos, "fmt ", 4) == 0 && len >= 16)
		{
			w->format = le16(c);
			w->channels = le16(c + 2);
			w->rate = le32(c + 4);
			w->blockAlign = le16(c + 12);
			w->bits = le16(c + 14);
			fmt = true;
		}
		else if (memcmp(p + pos, "data", 4) == 0)
		{
			w->data = c;
			w->dataLen = len <= size - pos - 8 ? len : size - pos - 8;
		}
		pos += 8 + len + (len & 1);
	}
	return fmt && w->data;
}

static const int kImaIndex[16] = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8 };
static const int kImaStep[89] = {
	7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107,
	118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
	1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894,
	6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
	32767,
};

static inline S16 ima_nibble(int nib, int *pred, int *index)
{
	int step = kImaStep[*index];
	int diff = step >> 3;
	if (nib & 1) diff += step >> 2;
	if (nib & 2) diff += step >> 1;
	if (nib & 4) diff += step;
	if (nib & 8) diff = -diff;
	*pred += diff;
	if (*pred > 32767) *pred = 32767;
	if (*pred < -32768) *pred = -32768;
	*index += kImaIndex[nib];
	if (*index < 0) *index = 0;
	if (*index > 88) *index = 88;
	return (S16)*pred;
}

// IMA ADPCM (blocos do WAV) -> PCM de 16 bits little-endian. Devolve o numero de amostras por canal.
static U32 ima_decode(const U8 *in, U32 inLen, int channels, U32 blockAlign, U8 *out, U32 maxSamples)
{
	if (channels < 1 || channels > 2 || blockAlign < 4u * channels)
		return 0;
	const U32 perBlock = (blockAlign - 4 * channels) * 8 / (4 * channels) + 1;
	U32 total = 0;
	for (U32 b = 0; b + 4u * channels <= inLen && total < maxSamples; b += blockAlign)
	{
		const U8 *blk = in + b;
		U32 blkLen = inLen - b < blockAlign ? inLen - b : blockAlign;
		int pred[2], index[2];
		S16 *dst = (S16 *)(out) + total * channels;
		U32 n = 0;
		for (int c = 0; c < channels; c++)
		{
			pred[c] = (S16)le16(blk + 4 * c);
			index[c] = blk[4 * c + 2] > 88 ? 88 : blk[4 * c + 2];
		}
		// A primeira amostra de cada bloco e o preditor.
		for (int c = 0; c < channels; c++)
			dst[c] = (S16)pred[c];
		n = 1;
		// Depois, grupos de 4 bytes (8 amostras) por canal, intercalados.
		U32 off = 4 * channels;
		while (off + 4u * channels <= blkLen && n + 8 <= perBlock && total + n + 8 <= maxSamples)
		{
			for (int c = 0; c < channels; c++)
			{
				const U8 *g = blk + off + 4 * c;
				for (int k = 0; k < 4; k++)
				{
					dst[(n + 2 * k) * channels + c] = ima_nibble(g[k] & 15, &pred[c], &index[c]);
					dst[(n + 2 * k + 1) * channels + c] = ima_nibble(g[k] >> 4, &pred[c], &index[c]);
				}
			}
			n += 8;
			off += 4 * channels;
		}
		total += n;
	}
	// As amostras foram escritas como S16 nativos (big-endian): passa para little-endian, como o resto dos dados.
	U8 *p = out;
	for (U32 i = 0; i < total * channels; i++, p += 2)
	{
		U8 t = p[0];
		p[0] = p[1];
		p[1] = t;
	}
	return total;
}

// Carrega um WAV de memoria no objeto (PCM aponta para os dados; ADPCM decodifica).
static bool obj_load_wav(XObj *o, const U8 *image, U32 size)
{
	WavInfo w;
	if (!parse_wav(image, size, &w))
		return false;
	obj_clear_data(o);
	o->channels = w.channels ? w.channels : 1;
	o->rate = w.rate ? w.rate : 22050;
	if (w.format == WAVE_FORMAT_PCM)
	{
		o->bits = w.bits == 8 ? 8 : 16;
		o->data = w.data;
		o->len = w.dataLen;
		return true;
	}
	if (w.format == 0x0011 /* WAVE_FORMAT_IMA_ADPCM */ && w.blockAlign)
	{
		U32 blocks = (w.dataLen + w.blockAlign - 1) / w.blockAlign;
		U32 perBlock = (w.blockAlign - 4 * o->channels) * 8 / (4 * o->channels) + 1;
		U32 maxSamples = blocks * perBlock;
		o->owned = (U8 *)malloc((size_t)maxSamples * o->channels * 2 + 4);
		if (!o->owned)
			return false;
		U32 n = ima_decode(w.data, w.dataLen, o->channels, w.blockAlign, o->owned, maxSamples);
		o->bits = 16;
		o->data = o->owned;
		o->len = n * o->channels * 2;
		return true;
	}
	return false;
}

// ---------------------------------------------------------------- mixer

static inline float sample_at(const XObj *o, U32 frame, int ch)
{
	if (o->bits == 16)
	{
		const U8 *p = o->data + ((size_t)frame * o->channels + ch) * 2;
		return (S16)le16(p) * (1.0f / 32768.0f);
	}
	return ((int)o->data[(size_t)frame * o->channels + ch] - 128) * (1.0f / 128.0f);
}

static void gains(const XObj *o, float *gl, float *gr)
{
	float vol = (o->volume < 0 ? 0 : o->volume > 127 ? 127 : o->volume) / 127.0f;
	vol *= g_Master / 127.0f;
	if (o->kind != OBJ_SAMPLE3D || !g_pListener)
	{
		float p = (o->pan < 0 ? 0 : o->pan > 127 ? 127 : o->pan) / 127.0f;
		*gl = vol * (p < 0.5f ? 1.0f : 2.0f * (1.0f - p));
		*gr = vol * (p > 0.5f ? 1.0f : 2.0f * p);
		return;
	}
	const XObj *l = g_pListener;
	float dx = o->x - l->x, dy = o->y - l->y, dz = o->z - l->z;
	float d = sqrtf(dx * dx + dy * dy + dz * dz);
	float att = 1.0f;
	if (d > o->maxDist)
		att = 0.0f;
	else if (d > o->minDist && d > 1e-4f)
		att = o->minDist / d;
	float muffle = o->obstruction > o->occlusion ? o->obstruction : o->occlusion;
	att *= 1.0f - 0.6f * (muffle < 0 ? 0 : muffle > 1 ? 1 : muffle);
	// Direita do ouvinte = cima x frente (sistema de mao esquerda do LithTech).
	float rx = l->uy * l->fz - l->uz * l->fy, ry = l->uz * l->fx - l->ux * l->fz, rz = l->ux * l->fy - l->uy * l->fx;
	float rl = sqrtf(rx * rx + ry * ry + rz * rz);
	float s = 0.0f;
	if (d > 1e-4f && rl > 1e-6f)
		s = (dx * rx + dy * ry + dz * rz) / (d * rl);
	s *= 0.8f;	// nunca so de um lado
	float g = vol * att;
	*gl = g * (s > 0 ? 1.0f - s : 1.0f);
	*gr = g * (s < 0 ? 1.0f + s : 1.0f);
}

static void mix_object(XObj *o, float *left, float *right)
{
	const U32 frameBytes = o->channels * (o->bits / 8);
	if (!o->data || !frameBytes)
	{
		o->status = SMP_DONE;
		return;
	}
	const U32 frames = o->len / frameBytes;
	U32 endFrame = o->loopEnd < 0 ? frames : (U32)o->loopEnd / frameBytes;
	if (endFrame > frames || endFrame == 0)
		endFrame = frames;
	U32 startFrame = o->loopStart > 0 ? (U32)o->loopStart / frameBytes : 0;
	if (startFrame >= endFrame)
		startFrame = 0;
	float gl, gr;
	gains(o, &gl, &gr);
	const double step = (double)o->rate / XAUDIO_RATE;
	for (int i = 0; i < XAUDIO_FRAME; i++)
	{
		// Fim do trecho: volta ao inicio do loop ou termina.
		U32 limit = (o->loops == 1) ? frames : endFrame;
		if (o->pos >= limit)
		{
			if (o->loops == 1)
			{
				o->status = SMP_DONE;
				return;
			}
			if (o->loops > 1)
				o->loops--;
			o->pos = startFrame + (o->pos - limit);
			if (o->pos >= limit)
				o->pos = startFrame;
		}
		U32 f0 = (U32)o->pos;
		U32 f1 = f0 + 1 < frames ? f0 + 1 : f0;
		float t = (float)(o->pos - f0);
		float a = sample_at(o, f0, 0), b = sample_at(o, f1, 0);
		float l = a + (b - a) * t, r = l;
		if (o->channels > 1)
		{
			float c = sample_at(o, f0, 1), e = sample_at(o, f1, 1);
			r = c + (e - c) * t;
		}
		left[i] += l * gl;
		right[i] += r * gr;
		o->pos += step;
	}
}

static void mix(float *left, float *right, void *)
{
	memset(left, 0, XAUDIO_FRAME * sizeof(float));
	memset(right, 0, XAUDIO_FRAME * sizeof(float));
	{
		Guard g;
		for (int i = 0; i < MAX_OBJECTS; i++)
		{
			XObj *o = &g_Obj[i];
			if ((o->kind == OBJ_SAMPLE || o->kind == OBJ_SAMPLE3D || o->kind == OBJ_STREAM) &&
				o->status == SMP_PLAYING && !o->paused)
				mix_object(o, left, right);
		}
	}
	for (int i = 0; i < XAUDIO_FRAME; i++)
	{
		left[i] = left[i] > 1.0f ? 1.0f : left[i] < -1.0f ? -1.0f : left[i];
		right[i] = right[i] > 1.0f ? 1.0f : right[i] < -1.0f ? -1.0f : right[i];
	}
}

static void set_ms(XObj *o, S32 ms)
{
	o->pos = ms > 0 ? (double)ms * o->rate / 1000.0 : 0.0;
}

static void set_offset(XObj *o, U32 bytes)
{
	U32 fb = o->channels * (o->bits / 8);
	o->pos = fb ? (double)(bytes / fb) : 0.0;
}

// ---------------------------------------------------------------- sistema

extern "C" S32 MSS_auto_cleanup(void) { return 0; }
extern "C" S32 AILCALL AIL_startup(void) { pthread_once(&g_Once, init_lock); return 1; }
extern "C" void AILCALL AIL_shutdown(void) {}
extern "C" void AILCALL AIL_lock(void) { pthread_once(&g_Once, init_lock); pthread_mutex_lock(&g_Lock); }
extern "C" void AILCALL AIL_unlock(void) { pthread_mutex_unlock(&g_Lock); }
extern "C" S32 AILCALL AIL_set_preference(U32 n, S32 v)
{
	if (n >= 64)
		return 0;
	S32 old = g_Prefs[n];
	g_Prefs[n] = v;
	return old;
}
extern "C" S32 AILCALL AIL_get_preference(U32 n) { return n < 64 ? g_Prefs[n] : 0; }
extern "C" U32 AILCALL AIL_ms_count(void) { return timeGetTime(); }
extern "C" void *AILCALL AIL_mem_alloc_lock(U32 size) { return malloc(size ? size : 1); }
extern "C" void AILCALL AIL_mem_free_lock(void *ptr) { free(ptr); }

// ---------------------------------------------------------------- driver

static int g_DriverToken;

extern "C" S32 AILCALL AIL_waveOutOpen(HDIGDRIVER *drvr, LPHWAVEOUT *lphWaveOut, S32, LPWAVEFORMAT)
{
	pthread_once(&g_Once, init_lock);
	if (lphWaveOut)
		*lphWaveOut = NULL;
	if (!g_bOpen)
		g_bOpen = xaudio_start(mix, NULL) != 0;
	if (drvr)
		*drvr = g_bOpen ? (HDIGDRIVER)&g_DriverToken : NULL;
	return g_bOpen ? 0 : -1;
}
extern "C" void AILCALL AIL_waveOutClose(HDIGDRIVER)
{
	Guard g;
	for (int i = 0; i < MAX_OBJECTS; i++)
		if (g_Obj[i].kind == OBJ_SAMPLE || g_Obj[i].kind == OBJ_SAMPLE3D || g_Obj[i].kind == OBJ_STREAM)
			g_Obj[i].status = SMP_STOPPED;
}
extern "C" S32 AILCALL AIL_digital_handle_release(HDIGDRIVER) { return 1; }
extern "C" S32 AILCALL AIL_digital_handle_reacquire(HDIGDRIVER) { return 1; }
extern "C" void AILCALL AIL_set_digital_master_volume(HDIGDRIVER, S32 v) { g_Master = v; }
extern "C" S32 AILCALL AIL_digital_master_volume(HDIGDRIVER) { return g_Master; }

// ---------------------------------------------------------------- amostras 2D

extern "C" HSAMPLE AILCALL AIL_allocate_sample_handle(HDIGDRIVER) { return obj_alloc(OBJ_SAMPLE); }
extern "C" void AILCALL AIL_release_sample_handle(HSAMPLE S) { obj_free(O(S)); }
extern "C" void AILCALL AIL_init_sample(HSAMPLE S)
{
	Guard g;
	if (!S)
		return;
	S32 user[USER_SLOTS];
	memcpy(user, O(S)->user, sizeof user);	// o soundmgr guarda seus dados antes de iniciar a amostra
	obj_reset(O(S), OBJ_SAMPLE);
	memcpy(O(S)->user, user, sizeof user);
}
extern "C" S32 AILCALL AIL_set_sample_file(HSAMPLE S, void const *file_image, S32)
{
	Guard g;
	if (!S || !file_image)
		return 0;
	// O tamanho vem do proprio RIFF.
	const U8 *p = (const U8 *)file_image;
	U32 size = memcmp(p, "RIFF", 4) == 0 ? le32(p + 4) + 8 : 0;
	return obj_load_wav(O(S), p, size) ? 1 : 0;
}
extern "C" void AILCALL AIL_set_sample_address(HSAMPLE S, void const *start, U32 len)
{
	Guard g;
	if (!S)
		return;
	obj_clear_data(O(S));
	O(S)->data = (const U8 *)start;
	O(S)->len = len;
}
extern "C" void AILCALL AIL_set_sample_type(HSAMPLE S, S32 format, U32)
{
	Guard g;
	if (!S)
		return;
	O(S)->channels = (format == DIG_F_STEREO_8 || format == DIG_F_STEREO_16) ? 2 : 1;
	O(S)->bits = (format == DIG_F_MONO_16 || format == DIG_F_STEREO_16) ? 16 : 8;
}
extern "C" void AILCALL AIL_set_sample_playback_rate(HSAMPLE S, S32 r) { if (S && r > 0) O(S)->rate = (U32)r; }
extern "C" void AILCALL AIL_set_sample_volume(HSAMPLE S, S32 v) { if (S) O(S)->volume = v; }
extern "C" void AILCALL AIL_set_sample_pan(HSAMPLE S, S32 p) { if (S) O(S)->pan = p; }
extern "C" S32 AILCALL AIL_sample_volume(HSAMPLE S) { return S ? O(S)->volume : 0; }
extern "C" S32 AILCALL AIL_sample_pan(HSAMPLE S) { return S ? O(S)->pan : 64; }
extern "C" void AILCALL AIL_set_sample_user_data(HSAMPLE S, U32 i, S32 v) { if (S && i < USER_SLOTS) O(S)->user[i] = v; }
extern "C" S32 AILCALL AIL_sample_user_data(HSAMPLE S, U32 i) { return S && i < USER_SLOTS ? O(S)->user[i] : 0; }
extern "C" void AILCALL AIL_stop_sample(HSAMPLE S)
{
	Guard g;
	if (S && O(S)->status == SMP_PLAYING)
		O(S)->status = SMP_STOPPED;
}
extern "C" void AILCALL AIL_resume_sample(HSAMPLE S)
{
	Guard g;
	if (S && O(S)->data)
		O(S)->status = SMP_PLAYING;
}
extern "C" void AILCALL AIL_set_sample_loop_count(HSAMPLE S, S32 n) { if (S) O(S)->loops = n; }
extern "C" void AILCALL AIL_set_sample_loop_block(HSAMPLE S, S32 a, S32 b)
{
	if (!S)
		return;
	O(S)->loopStart = a;
	O(S)->loopEnd = b;
}
extern "C" void AILCALL AIL_set_sample_ms_position(HSAMPLE S, S32 ms) { Guard g; if (S) set_ms(O(S), ms); }
extern "C" void AILCALL AIL_set_sample_reverb(HSAMPLE, F32, F32, F32) {}
extern "C" HPROVIDER AILCALL AIL_set_sample_processor(HSAMPLE, U32, HPROVIDER) { return 0; }

// ---------------------------------------------------------------- filtros (nenhum)

extern "C" S32 AILCALL AIL_enumerate_filters(HPROENUM *, HPROVIDER *, char **) { return 0; }
extern "C" S32 AILCALL AIL_enumerate_filter_sample_attributes(HPROVIDER, HINTENUM *, RIB_INTERFACE_ENTRY *) { return 0; }
extern "C" void AILCALL AIL_filter_sample_attribute(HSAMPLE, char const *, void *val) { if (val) *(S32 *)val = 0; }
extern "C" void AILCALL AIL_set_filter_sample_preference(HSAMPLE, char const *, void const *) {}

// ---------------------------------------------------------------- 3D

// Os nomes que o soundmgr reconhece: qualquer um que a configuracao do PC peca casa com o mesmo provedor.
static const char *const kProviders[] = {
	"Miles 2D Stereo Positional Audio",
	"Microsoft DirectSound3D software emulation",
	"Microsoft DirectSound3D hardware support",
	"Microsoft DirectSound3D with Creative Labs EAX(TM)",
	"Aureal A3D Interactive(TM)",
	"Intel Realistic Sound Experience(TM)",
};
static const HPROVIDER kProvider = 0x360;

extern "C" S32 AILCALL AIL_enumerate_3D_providers(HPROENUM *next, HPROVIDER *dest, char **name)
{
	if (!next || *next >= sizeof kProviders / sizeof kProviders[0])
		return 0;
	if (dest)
		*dest = kProvider;
	if (name)
		*name = (char *)kProviders[*next];
	(*next)++;
	return 1;
}
extern "C" S32 AILCALL AIL_open_3D_provider(HPROVIDER) { return M3D_NOERR; }
extern "C" void AILCALL AIL_close_3D_provider(HPROVIDER) {}
extern "C" void AILCALL AIL_3D_provider_attribute(HPROVIDER, char const *name, void *val)
{
	if (!val)
		return;
	if (name && strcmp(name, "Maximum supported samples") == 0)
		*(S32 *)val = 48;
	else
		*(S32 *)val = -1;	// sem EAX
}
extern "C" void AILCALL AIL_set_3D_room_type(HPROVIDER, S32) {}
extern "C" S32 AILCALL AIL_3D_room_type(HPROVIDER) { return -1; }
extern "C" H3DPOBJECT AILCALL AIL_open_3D_listener(HPROVIDER)
{
	XObj *l = obj_alloc(OBJ_LISTENER);
	if (l)
		g_pListener = l;
	return l;
}
extern "C" void AILCALL AIL_close_3D_listener(H3DPOBJECT L)
{
	Guard g;
	if (O(L) == g_pListener)
		g_pListener = NULL;
	obj_free(O(L));
}

extern "C" void AILCALL AIL_set_3D_position(H3DPOBJECT h, F32 X, F32 Y, F32 Z)
{
	if (!h)
		return;
	O(h)->x = X;
	O(h)->y = Y;
	O(h)->z = Z;
}
extern "C" void AILCALL AIL_set_3D_velocity_vector(H3DPOBJECT, F32, F32, F32) {}
extern "C" void AILCALL AIL_set_3D_orientation(H3DPOBJECT h, F32 xf, F32 yf, F32 zf, F32 xu, F32 yu, F32 zu)
{
	if (!h)
		return;
	XObj *o = O(h);
	o->fx = xf; o->fy = yf; o->fz = zf;
	o->ux = xu; o->uy = yu; o->uz = zu;
}
extern "C" void AILCALL AIL_3D_position(H3DPOBJECT h, F32 *X, F32 *Y, F32 *Z)
{
	XObj *o = O(h);
	if (X) *X = o ? o->x : 0;
	if (Y) *Y = o ? o->y : 0;
	if (Z) *Z = o ? o->z : 0;
}
extern "C" void AILCALL AIL_3D_velocity(H3DPOBJECT, F32 *x, F32 *y, F32 *z)
{
	if (x) *x = 0;
	if (y) *y = 0;
	if (z) *z = 0;
}
extern "C" void AILCALL AIL_3D_orientation(H3DPOBJECT h, F32 *xf, F32 *yf, F32 *zf, F32 *xu, F32 *yu, F32 *zu)
{
	XObj *o = O(h);
	if (!o)
		return;
	if (xf) *xf = o->fx;
	if (yf) *yf = o->fy;
	if (zf) *zf = o->fz;
	if (xu) *xu = o->ux;
	if (yu) *yu = o->uy;
	if (zu) *zu = o->uz;
}
extern "C" void AILCALL AIL_set_3D_user_data(H3DPOBJECT h, U32 i, S32 v) { if (h && i < USER_SLOTS) O(h)->user[i] = v; }
extern "C" S32 AILCALL AIL_3D_user_data(H3DPOBJECT h, U32 i) { return h && i < USER_SLOTS ? O(h)->user[i] : 0; }

extern "C" H3DSAMPLE AILCALL AIL_allocate_3D_sample_handle(HPROVIDER) { return obj_alloc(OBJ_SAMPLE3D); }
extern "C" void AILCALL AIL_release_3D_sample_handle(H3DSAMPLE S) { obj_free(O(S)); }
extern "C" void AILCALL AIL_start_3D_sample(H3DSAMPLE S)
{
	Guard g;
	if (!S || !O(S)->data)
		return;
	O(S)->pos = 0;
	O(S)->status = SMP_PLAYING;
}
extern "C" void AILCALL AIL_stop_3D_sample(H3DSAMPLE S)
{
	Guard g;
	if (S && O(S)->status == SMP_PLAYING)
		O(S)->status = SMP_STOPPED;
}
extern "C" void AILCALL AIL_resume_3D_sample(H3DSAMPLE S)
{
	Guard g;
	if (S && O(S)->data)
		O(S)->status = SMP_PLAYING;
}
extern "C" void AILCALL AIL_end_3D_sample(H3DSAMPLE S)
{
	Guard g;
	if (S)
		O(S)->status = SMP_DONE;
}
extern "C" S32 AILCALL AIL_set_3D_sample_info(H3DSAMPLE S, AILSOUNDINFO const *info)
{
	Guard g;
	if (!S || !info || !info->data_ptr)
		return 0;
	XObj *o = O(S);
	obj_clear_data(o);
	if (info->format != WAVE_FORMAT_PCM)
		return 0;	// o soundmgr descomprime antes (AIL_decompress_ADPCM)
	o->data = (const U8 *)info->data_ptr;
	o->len = info->data_len;
	o->rate = info->rate ? info->rate : 22050;
	o->bits = info->bits == 8 ? 8 : 16;
	o->channels = info->channels == 2 ? 2 : 1;
	o->pos = 0;
	o->loops = 1;
	o->loopStart = 0;
	o->loopEnd = -1;
	o->status = SMP_DONE;
	return 1;
}
extern "C" void AILCALL AIL_set_3D_sample_volume(H3DSAMPLE S, S32 v) { if (S) O(S)->volume = v; }
extern "C" S32 AILCALL AIL_3D_sample_volume(H3DSAMPLE S) { return S ? O(S)->volume : 0; }
extern "C" void AILCALL AIL_set_3D_sample_playback_rate(H3DSAMPLE S, S32 r) { if (S && r > 0) O(S)->rate = (U32)r; }
extern "C" void AILCALL AIL_set_3D_sample_offset(H3DSAMPLE S, U32 off) { Guard g; if (S) set_offset(O(S), off); }
extern "C" void AILCALL AIL_set_3D_sample_loop_count(H3DSAMPLE S, U32 n) { if (S) O(S)->loops = (S32)n; }
extern "C" void AILCALL AIL_set_3D_sample_loop_block(H3DSAMPLE S, S32 a, S32 b)
{
	if (!S)
		return;
	O(S)->loopStart = a;
	O(S)->loopEnd = b;
}
extern "C" U32 AILCALL AIL_3D_sample_status(H3DSAMPLE S) { return S ? O(S)->status : SMP_FREE; }
extern "C" void AILCALL AIL_set_3D_sample_distances(H3DSAMPLE S, F32 maxd, F32 mind)
{
	if (!S)
		return;
	O(S)->maxDist = maxd > 0 ? maxd : 1.0f;
	O(S)->minDist = mind > 0 ? mind : 0.01f;
}
extern "C" void AILCALL AIL_set_3D_sample_preference(H3DSAMPLE, char const *, void const *) {}
extern "C" void AILCALL AIL_set_3D_sample_effects_level(H3DSAMPLE, F32) {}
extern "C" void AILCALL AIL_set_3D_sample_obstruction(H3DSAMPLE S, F32 v) { if (S) O(S)->obstruction = v; }
extern "C" F32 AILCALL AIL_3D_sample_obstruction(H3DSAMPLE S) { return S ? O(S)->obstruction : 0.0f; }
extern "C" void AILCALL AIL_set_3D_sample_occlusion(H3DSAMPLE S, F32 v) { if (S) O(S)->occlusion = v; }

// ---------------------------------------------------------------- streams

// O soundmgr passa o handle do arquivo (ja posicionado no WAV dentro do .rez) no nome: "\\\\<numero>".
extern "C" HSTREAM AILCALL AIL_open_stream(HDIGDRIVER, char const *filename, S32)
{
	if (!filename)
		return NULL;
	const char *p = filename;
	while (*p == '\\')
		p++;
	// O soundmgr escreve o ponteiro com %d: pode vir negativo; strtoul devolve o mesmo padrao de bits.
	HANDLE h = (HANDLE)(uintptr_t)strtoul(p, NULL, 10);
	if (!h || h == INVALID_HANDLE_VALUE)
		return NULL;

	U8 hdr[12];
	DWORD got = 0;
	if (!ReadFile(h, hdr, sizeof hdr, &got, NULL) || got != sizeof hdr || memcmp(hdr, "RIFF", 4) != 0)
		return NULL;
	U32 size = le32(hdr + 4) + 8;
	U8 *image = (U8 *)malloc(size);
	if (!image)
		return NULL;
	memcpy(image, hdr, sizeof hdr);
	if (!ReadFile(h, image + sizeof hdr, size - sizeof hdr, &got, NULL))
		got = 0;
	size = sizeof hdr + got;

	XObj *o = obj_alloc(OBJ_STREAM);
	if (!o)
	{
		free(image);
		return NULL;
	}
	Guard g;
	if (!obj_load_wav(o, image, size))
	{
		free(image);
		obj_free(o);
		return NULL;
	}
	// Os dados PCM apontam para dentro da imagem: o objeto passa a ser dono dela.
	if (!o->owned)
		o->owned = image;
	else
		free(image);	// ADPCM: obj_load_wav ja decodificou para um buffer proprio
	return o;
}
extern "C" void AILCALL AIL_close_stream(HSTREAM s) { obj_free(O(s)); }
extern "C" void AILCALL AIL_start_stream(HSTREAM s)
{
	Guard g;
	if (!s)
		return;
	O(s)->pos = 0;
	O(s)->paused = false;
	O(s)->status = SMP_PLAYING;
}
extern "C" void AILCALL AIL_pause_stream(HSTREAM s, S32 onoff)
{
	Guard g;
	if (!s)
		return;
	O(s)->paused = onoff != 0;
	if (!onoff)
		O(s)->status = SMP_PLAYING;
}
extern "C" void AILCALL AIL_set_stream_loop_count(HSTREAM s, S32 n) { if (s) O(s)->loops = n; }
extern "C" void AILCALL AIL_set_stream_ms_position(HSTREAM s, S32 ms) { Guard g; if (s) set_ms(O(s), ms); }
extern "C" void AILCALL AIL_set_stream_playback_rate(HSTREAM s, S32 r) { if (s && r > 0) O(s)->rate = (U32)r; }
extern "C" void AILCALL AIL_set_stream_volume(HSTREAM s, S32 v) { if (s) O(s)->volume = v; }
extern "C" void AILCALL AIL_set_stream_pan(HSTREAM s, S32 p) { if (s) O(s)->pan = p; }
extern "C" S32 AILCALL AIL_stream_volume(HSTREAM s) { return s ? O(s)->volume : 0; }
extern "C" S32 AILCALL AIL_stream_pan(HSTREAM s) { return s ? O(s)->pan : 64; }
extern "C" void AILCALL AIL_set_stream_user_data(HSTREAM s, U32 i, S32 v) { if (s && i < USER_SLOTS) O(s)->user[i] = v; }

// ---------------------------------------------------------------- descompressao

// IMA ADPCM -> um WAV PCM de 16 bits completo (o soundmgr o le de volta com GetWaveInfo).
extern "C" S32 AILCALL AIL_decompress_ADPCM(AILSOUNDINFO const *info, void **outdata, U32 *outsize)
{
	if (!info || !info->data_ptr || !outdata || !outsize || !info->block_size)
		return 0;
	int ch = info->channels == 2 ? 2 : 1;
	U32 blockAlign = info->block_size;
	U32 blocks = (info->data_len + blockAlign - 1) / blockAlign;
	U32 perBlock = (blockAlign - 4 * ch) * 8 / (4 * ch) + 1;
	U32 maxSamples = blocks * perBlock;
	U8 *wav = (U8 *)malloc(44 + (size_t)maxSamples * ch * 2);
	if (!wav)
		return 0;
	U32 n = ima_decode((const U8 *)info->data_ptr, info->data_len, ch, blockAlign, wav + 44, maxSamples);
	if (info->samples && n > info->samples)
		n = info->samples;
	U32 dataLen = n * ch * 2;
	U32 rate = info->rate ? info->rate : 22050;
	U8 *h = wav;
	auto put32 = [](U8 *p, U32 v) { p[0] = (U8)v; p[1] = (U8)(v >> 8); p[2] = (U8)(v >> 16); p[3] = (U8)(v >> 24); };
	auto put16 = [](U8 *p, U16 v) { p[0] = (U8)v; p[1] = (U8)(v >> 8); };
	memcpy(h, "RIFF", 4); put32(h + 4, 36 + dataLen); memcpy(h + 8, "WAVE", 4);
	memcpy(h + 12, "fmt ", 4); put32(h + 16, 16); put16(h + 20, WAVE_FORMAT_PCM); put16(h + 22, (U16)ch);
	put32(h + 24, rate); put32(h + 28, rate * ch * 2); put16(h + 32, (U16)(ch * 2)); put16(h + 34, 16);
	memcpy(h + 36, "data", 4); put32(h + 40, dataLen);
	*outdata = wav;
	*outsize = 44 + dataLen;
	return 1;
}

// MP3 (Miles ASI): ainda sem decodificador; o soundmgr segue sem esse som.
extern "C" S32 AILCALL AIL_decompress_ASI(void const *, U32, char const *, void **outdata, U32 *outsize, AILLENGTHYCB)
{
	if (outdata)
		*outdata = NULL;
	if (outsize)
		*outsize = 0;
	return 0;
}
