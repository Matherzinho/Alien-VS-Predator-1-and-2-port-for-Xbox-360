// avp360: Aliens versus Predator (1999) e Aliens versus Predator 2 (2001) no Xbox 360, a partir dos arquivos do jogador.
// Port para Xbox 360 feito por Matherzin.
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avp_audio.h"
#include "avp_endian.h"
#include "avp_options.h"
#include "AvP360_shaders.h"
#include "xsdk/audio.h"
#include "xsdk/gpu.h"
#include "xsdk/kernel.h"
#include "xsdk/log.h"
#include "xsdk/present.h"
#include "xsdk/shader.h"
#include "xsdk/system.h"
#include "xsdk/text.h"
#include "xsdk/texture.h"
#include "xsdk/thread.h"
#include "xsdk/xam.h"

static inline uint64_t ticks(void) { return __builtin_ppc_get_timebase(); }
#define TICKS_MS (49875.0)

// Arquivo de flag ao lado do .xex: para testes automatizados no Xenia / console.
static int flag_file(const char *name, int if_absent)
{
	char path[128];
	snprintf(path, sizeof path, "game:\\%s", name);
	FILE *f = fopen(path, "rb");
	if (!f) return if_absent;
	int v = 1;
	if (fscanf(f, "%d", &v) != 1) v = 1;
	fclose(f);
	return v;
}

// Arquivos que mostram que um jogo esta na pasta (os dados sao do proprio jogador).
static const char *const kAvp1Files[] = {
	"game:\\avp1\\avp_huds.rif", "game:\\avp1\\AvP_Huds\\marine.rif", "game:\\avp1\\fastfile\\ffinfo.txt",
	"game:\\avp1\\AvP_Rifs\\Derelict.RIF", NULL,
};
static const char *const kAvp2Files[] = { "game:\\avp2\\AVP2.REZ", "game:\\avp2\\avp2.rez", NULL };

static bool game_present(const char *const *files)
{
	for (int i = 0; files[i]; i++) {
		FILE *f = fopen(files[i], "rb");
		if (f) {
			fclose(f);
			return true;
		}
	}
	return false;
}

// ---- Opcoes: avp_opcoes.cfg ao lado do .xex, chave=valor, escolhidas no menu (Y ou Start no jogo) ----
static const char *const names_language[] = { "Portugu\xc3\xaas", "English", "Espa\xc3\xb1ol" };
static const char *const names_yes_no[] = { "N\xc3\xa3o", "Sim" };
static const char *const names_species[] = { "Menu do jogo", "Colonial Marine", "Alien", "Predator" };
static const char *const names_difficulty[] = { "N\xc3\xa3o mudar", "F\xc3\xa1""cil", "M\xc3\xa9""dio", "Dif\xc3\xad""cil", "Diretor" };
static const char *const names_filter[] = { "Bilinear n\xc3\xadtido", "Pixels n\xc3\xadtidos", "Bilinear comum", "Catmull-Rom", "Scale3x" };
static const char *const names_wide[] = { "16:9", "4:3" };
static const char *const names_fov[] = { "75\xc2\xb0", "85\xc2\xb0", "90\xc2\xb0", "100\xc2\xb0", "110\xc2\xb0" };
static const char *const names_gamma[] = { "Padr\xc3\xa3o", "+15%", "+30%", "+45%" };
static const char *const names_lines[] = { "N\xc3\xa3o", "Leves", "M\xc3\xa9""dias", "Fortes", "CRT" };
static const char *const names_sens[] = { "Baixa", "M\xc3\xa9""dia", "Alta", "Muito alta" };
static const char *const names_aim[] = { "N\xc3\xa3o", "Leve", "Forte" };
static const char *const names_layout[] = { "Padr\xc3\xa3o (RT Atira / LT Alt)", "Cl\xc3\xa1ssico Xbox", "T\xc3\xa1tico" };
static const char *const names_volume[] = { "0%", "10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%" };

static const struct { const char *key, *label; int start, count; const char *const *names; } option_info[OPTIONS] = {
	[O_LANGUAGE]        = { "idioma",            "Idioma",                                 0, LANGS, names_language },
	[O_SPECIES]         = { "especie_inicial",   "Campanha inicial",                       0, 4,     names_species },
	[O_DIFFICULTY]      = { "dificuldade",       "Dificuldade",                            0, 5,     names_difficulty },
	[O_UNLIMITED_SAVES] = { "saves_ilimitados",  "Saves ilimitados (AvP 1)",               1, 2,     names_yes_no },
	[O_UNLOCK_LEVELS]   = { "desbloquear_fases", "Desbloquear todas as fases",             1, 2,     names_yes_no },
	[O_TRAINING]        = { "treino",            "Treino: vida e muni\xc3\xa7\xc3\xa3o infinitas", 0, 2, names_yes_no },
	[O_FILTER]          = { "filtro_imagem",     "Filtro da imagem",                       0, 5,     names_filter },
	[O_WIDE]            = { "proporcao",         "Propor\xc3\xa7\xc3\xa3o",                0, 2,     names_wide },
	[O_FOV]             = { "fov",               "Campo de vis\xc3\xa3o (FOV)",            2, 5,     names_fov },
	[O_GAMMA]           = { "brilho",            "Brilho / Gamma",                         0, 4,     names_gamma },
	[O_LINES]           = { "scanlines",         "Scanlines",                              0, 5,     names_lines },
	[O_FPS]             = { "fps",               "Mostrar FPS",                            0, 2,     names_yes_no },
	[O_SENSITIVITY]     = { "sensibilidade",     "Sensibilidade da mira",                  1, 4,     names_sens },
	[O_INVERT_Y]        = { "inverter_y",        "Inverter eixo Y",                        0, 2,     names_yes_no },
	[O_AIM_ASSIST]      = { "mira_assistida",    "Mira assistida (Aim Assist)",            1, 3,     names_aim },
	[O_LAYOUT]          = { "botoes",            "Esquema de bot\xc3\xb5""es",             0, 3,     names_layout },
	[O_RUMBLE]          = { "vibracao",          "Vibrar ao levar dano / atirar",          1, 2,     names_yes_no },
	[O_VOLUME]          = { "volume",            "Volume",                                10, 11,    names_volume },
};

int avp_opt[OPTIONS];

// Dicionario trilingue (Portugues, English, Espanol)
static const char *const words[][LANGS] = {
	{ "Idioma", "Language", "Idioma" },
	{ "Campanha inicial", "Starting campaign", "Campa\xc3\xb1""a inicial" },
	{ "Menu do jogo", "In-game menu", "Men\xc3\xba del juego" },
	{ "Dificuldade", "Difficulty", "Dificultad" },
	{ "Saves ilimitados (AvP 1)", "Unlimited saves (AvP 1)", "Guardados ilimitados (AvP 1)" },
	{ "Desbloquear todas as fases", "Unlock all levels", "Desbloquear todos los niveles" },
	{ "Treino: vida e muni\xc3\xa7\xc3\xa3o infinitas", "Training: endless health & ammo", "Pr\xc3\xa1""ctica: vida y munici\xc3\xb3n infinitas" },
	{ "Filtro da imagem", "Picture filter", "Filtro de imagen" },
	{ "Propor\xc3\xa7\xc3\xa3o", "Aspect ratio", "Proporci\xc3\xb3n" },
	{ "Campo de vis\xc3\xa3o (FOV)", "Field of view (FOV)", "Campo de visi\xc3\xb3n (FOV)" },
	{ "Brilho / Gamma", "Brightness / Gamma", "Brillo / Gamma" },
	{ "Mostrar FPS", "Show FPS", "Mostrar FPS" },
	{ "Sensibilidade da mira", "Look sensitivity", "Sensibilidad de mira" },
	{ "Inverter eixo Y", "Invert Y axis", "Invertir eje Y" },
	{ "Mira assistida (Aim Assist)", "Aim assist", "Asistencia de apuntado" },
	{ "Esquema de bot\xc3\xb5""es", "Button layout", "Disposici\xc3\xb3n de botones" },
	{ "Vibrar ao levar dano / atirar", "Rumble on damage / fire", "Vibrar al recibir da\xc3\xb1o / disparar" },
	{ "Volume", "Volume", "Volumen" },
	{ "N\xc3\xa3o", "No", "No" },
	{ "Sim", "Yes", "S\xc3\xad" },
	{ "N\xc3\xa3o mudar", "Don't change", "No cambiar" },
	{ "F\xc3\xa1""cil", "Easy", "F\xc3\xa1""cil" },
	{ "M\xc3\xa9""dio", "Medium", "Medio" },
	{ "Dif\xc3\xad""cil", "Hard", "Dif\xc3\xad""cil" },
	{ "Diretor", "Director's Cut", "Director" },
	{ "Padr\xc3\xa3o", "Default", "Est\xc3\xa1ndar" },
	{ "Baixa", "Low", "Baja" },
	{ "M\xc3\xa9""dia", "Medium", "Media" },
	{ "Alta", "High", "Alta" },
	{ "Muito alta", "Very high", "Muy alta" },
	{ "Leve", "Light", "Suave" },
	{ "Forte", "Strong", "Fuerte" },
	{ "Padr\xc3\xa3o (RT Atira / LT Alt)", "Default (RT Fire / LT Alt)", "Est\xc3\xa1ndar (RT Disparo / LT Alt)" },
	{ "Cl\xc3\xa1ssico Xbox", "Classic Xbox", "Cl\xc3\xa1sico Xbox" },
	{ "T\xc3\xa1tico", "Tactical", "T\xc3\xa1""ctico" },
	{ "Bilinear n\xc3\xadtido", "Sharp bilinear", "Bilineal n\xc3\xadtido" },
	{ "Pixels n\xc3\xadtidos", "Sharp pixels", "P\xc3\xadxeles n\xc3\xadtidos" },
	{ "Bilinear comum", "Plain bilinear", "Bilineal com\xc3\xban" },
	{ "Leves", "Light", "Leves" },
	{ "M\xc3\xa9""dias", "Medium", "Medias" },
	{ "Fortes", "Strong", "Fuertes" },
	{ "Continuar", "Resume", "Continuar" },
	{ "Escolher outro jogo", "Choose another game", "Elegir otro juego" },
	{ "Sair do jogo", "Quit the game", "Salir del juego" },
	{ "Op\xc3\xa7\xc3\xb5""es", "Options", "Opciones" },
	{ "Y: Op\xc3\xa7\xc3\xb5""es", "Y: Options", "Y: Opciones" },
	{ "Aliens vs. Predator 1 & 2 no Xbox 360", "Aliens vs. Predator 1 & 2 on Xbox 360", "Aliens vs. Predator 1 & 2 en Xbox 360" },
	{ "Port feito por Matherzin (Rebellion 3dc & Monolith LithTech)", "Port by Matherzin (Rebellion 3dc & Monolith LithTech)", "Port hecho por Matherzin (Rebellion 3dc & Monolith LithTech)" },
	{ "Direcional: escolher e mudar     B: voltar e guardar     X: tudo como vem de f\xc3\xa1""brica",
	  "D-pad: choose and change     B: back and save     X: restore factory defaults",
	  "Cruceta: elegir y cambiar     B: volver y guardar     X: restaurar valores de f\xc3\xa1""brica" },
	{ "Back + Start: sair", "Back + Start: quit", "Back + Start: salir" },
	{ "Nenhum jogo encontrado nesta pasta.\n\n"
	  "Coloque aqui os seus proprios arquivos originais:\n"
	  "  avp1\\  (avp_huds.rif, language.txt, fastfiles do AvP 1 Gold)\n"
	  "  avp2\\  (AVP2.REZ, AVP2DLL.REZ, AVP2L.REZ do AvP 2)\n\n"
	  "Veja o LEIA-ME.  Back + Start: sair",
	  "No game found in this folder.\n\n"
	  "Put here your own original game files:\n"
	  "  avp1\\  (avp_huds.rif, language.txt, AvP 1 Gold fastfiles)\n"
	  "  avp2\\  (AVP2.REZ, AVP2DLL.REZ, AVP2L.REZ from AvP 2)\n\n"
	  "See the README.  Back + Start: quit",
	  "No hay ning\xc3\xban juego en esta carpeta.\n\n"
	  "Pon aqu\xc3\xad tus propios archivos originales:\n"
	  "  avp1\\  (avp_huds.rif, language.txt, fastfiles de AvP 1 Gold)\n"
	  "  avp2\\  (AVP2.REZ, AVP2DLL.REZ, AVP2L.REZ de AvP 2)\n\n"
	  "Mira el L\xc3\x89""AME.  Back + Start: salir" },
};

const char *T(const char *pt)
{
	const int l = avp_opt[O_LANGUAGE];
	if (l <= LANG_PT || l >= LANGS) return pt;
	for (size_t i = 0; i < sizeof words / sizeof words[0]; i++)
		if (!strcmp(words[i][LANG_PT], pt)) return words[i][l];
	return pt;
}

int avp_option_fov_degrees(void)
{
	static const int fovs[5] = { 75, 85, 90, 100, 110 };
	const int idx = avp_opt[O_FOV];
	return (idx >= 0 && idx < 5) ? fovs[idx] : 90;
}

float avp_option_sensitivity_scale(void)
{
	static const float scales[4] = { 0.65f, 1.0f, 1.45f, 2.0f };
	const int idx = avp_opt[O_SENSITIVITY];
	return (idx >= 0 && idx < 4) ? scales[idx] : 1.0f;
}

static int console_language(void)
{
	const uint32_t l = xsdk_system_language();
	return l == XSDK_LANGUAGE_PORTUGUESE ? LANG_PT : l == XSDK_LANGUAGE_SPANISH ? LANG_ES : LANG_EN;
}

static void options_defaults(void)
{
	const int language = avp_opt[O_LANGUAGE];
	for (int i = 0; i < OPTIONS; i++) avp_opt[i] = option_info[i].start;
	avp_opt[O_LANGUAGE] = language;
}

static void options_load(void)
{
	avp_opt[O_LANGUAGE] = console_language();
	options_defaults();
	FILE *f = fopen("game:\\avp_opcoes.cfg", "rb");
	if (!f) return;
	char line[96], key[40];
	int v;
	while (fgets(line, sizeof line, f))
		if (sscanf(line, "%39[^=]=%d", key, &v) == 2)
			for (int i = 0; i < OPTIONS; i++)
				if (!strcmp(key, option_info[i].key) && v >= 0 && v < option_info[i].count) avp_opt[i] = v;
	fclose(f);
}

static void options_save(void)
{
	if (flag_file("avp_nosave.txt", 0)) return;
	FILE *f = fopen("game:\\avp_opcoes.cfg", "wb");
	if (!f) { xsdk_log_write("avp360: avp_opcoes.cfg can't be written"); return; }
	for (int i = 0; i < OPTIONS; i++) fprintf(f, "%s=%d\n", option_info[i].key, avp_opt[i]);
	fclose(f);
}

// ---- Controles ----
enum {
	IN_UP = 0x0040, IN_DOWN = 0x0080, IN_LEFT = 0x0100, IN_RIGHT = 0x0200,
	IN_START = 0x0400, IN_BACK = 0x0800,
};

static uint32_t pad_read_full(uint32_t index, uint32_t *buttons_out, int16_t *lx, int16_t *ly, int16_t *rx, int16_t *ry, uint8_t *lt, uint8_t *rt)
{
	INPUT_STATE s;
	memset(&s, 0, sizeof s);
	if (XamInputGetState(index, 0, &s) != ERROR_SUCCESS) {
		if (buttons_out) *buttons_out = 0;
		if (lx) *lx = 0;
		if (ly) *ly = 0;
		if (rx) *rx = 0;
		if (ry) *ry = 0;
		if (lt) *lt = 0;
		if (rt) *rt = 0;
		return 0;
	}
	uint32_t b = s.gamepad.buttons, v = 0;
	if (buttons_out) *buttons_out = b;
	if (lx) *lx = s.gamepad.thumb_lx;
	if (ly) *ly = s.gamepad.thumb_ly;
	if (rx) *rx = s.gamepad.thumb_rx;
	if (ry) *ry = s.gamepad.thumb_ry;
	if (lt) *lt = s.gamepad.left_trigger;
	if (rt) *rt = s.gamepad.right_trigger;

	const int dead = 12000;
	if ((b & INPUT_GAMEPAD_DPAD_UP) || s.gamepad.thumb_ly > dead) v |= IN_UP;
	if ((b & INPUT_GAMEPAD_DPAD_DOWN) || s.gamepad.thumb_ly < -dead) v |= IN_DOWN;
	if ((b & INPUT_GAMEPAD_DPAD_LEFT) || s.gamepad.thumb_lx < -dead) v |= IN_LEFT;
	if ((b & INPUT_GAMEPAD_DPAD_RIGHT) || s.gamepad.thumb_lx > dead) v |= IN_RIGHT;
	if (b & INPUT_GAMEPAD_START) v |= IN_START;
	if (b & INPUT_GAMEPAD_BACK) v |= IN_BACK;
	return v;
}

static uint32_t pad_bits(uint32_t index, uint32_t *buttons_out)
{
	return pad_read_full(index, buttons_out, NULL, NULL, NULL, NULL, NULL, NULL);
}

// ---- Video (720p nativo 1280x720 @ 60 Hz via xsdk, com fallback offscreen para Xenia headless) ----
enum { SCREEN_TEXTURES = 3, FB_W = 1280, FB_H = 720 };
#define TICKS_PER_FRAME_60HZ 831250ull
static XsdkTexture screen_tex[SCREEN_TEXTURES];
static XsdkSamplerState sampler_near, sampler_soft;
static XsdkShader vs, ps, ps_sharp, ps_cubic, ps_scale3x;
static volatile uint32_t *quad;
static uint32_t present_w, present_h;
static bool have_screen, wide;
static int filter;
static char fps_text[16];
static XsdkTexture offscreen;
static bool have_offscreen;
enum { QUAD_FLOATS = 4, QUAD_VERTS = 4 };

static const XsdkVertexElement quad_layout[] = {
	{ .usage = XSDK_USAGE_POSITION, .format = XSDK_VERTEX_FLOAT2, .offset_bytes = 0 },
	{ .usage = XSDK_USAGE_TEXCOORD, .format = XSDK_VERTEX_FLOAT2, .offset_bytes = 8 },
};

static bool video_open(void)
{
	NTSTATUS st = xsdk_gpu_open();
	if (!NT_SUCCESS(st)) { xsdk_log_write("avp360: xsdk_gpu_open failed: 0x%08X", (unsigned)st); return false; }
	st = xsdk_present_open(XSDK_DISPLAY_720P, 3, 1, &present_w, &present_h);
	const bool display = NT_SUCCESS(st);
	if (!display) {
		xsdk_log_write("avp360: no display (0x%08X): Xenia, drawing into offscreen texture", (unsigned)st);
		present_w = 1280;
		present_h = 720;
		st = xsdk_texture_create(&offscreen, XSDK_FORMAT_A8R8G8B8, present_w, present_h, 1, true);
		if (!NT_SUCCESS(st)) return false;
		xsdk_texture_base_at_corner(&offscreen);
		have_offscreen = true;
	}
	st = xsdk_shader_load(&vs, blit_vert.container, blit_vert.container_bytes);
	if (NT_SUCCESS(st)) st = xsdk_shader_load(&ps, blit_frag.container, blit_frag.container_bytes);
	if (NT_SUCCESS(st)) st = xsdk_shader_load(&ps_sharp, blit_sharp_frag.container, blit_sharp_frag.container_bytes);
	if (NT_SUCCESS(st)) st = xsdk_shader_load(&ps_cubic, blit_cubic_frag.container, blit_cubic_frag.container_bytes);
	if (NT_SUCCESS(st)) st = xsdk_shader_load(&ps_scale3x, blit_scale3x_frag.container, blit_scale3x_frag.container_bytes);
	if (NT_SUCCESS(st))
		st = xsdk_shader_set_vertex_layout(&vs, quad_layout, 2, (const uint32_t[]){ QUAD_FLOATS * 4 });
	if (!NT_SUCCESS(st)) { xsdk_log_write("avp360: shaders failed: 0x%08X", (unsigned)st); return false; }
	for (int i = 0; i < SCREEN_TEXTURES; i++) {
		st = xsdk_texture_create(&screen_tex[i], XSDK_FORMAT_A8R8G8B8, FB_W, FB_H, 1, false);
		if (!NT_SUCCESS(st)) { xsdk_log_write("avp360: texture failed: 0x%08X", (unsigned)st); return false; }
	}
	XsdkSampler s = { .mag_filter = XSDK_FILTER_POINT, .min_filter = XSDK_FILTER_POINT, .mip_filter = XSDK_FILTER_POINT,
		.address_u = XSDK_ADDRESS_CLAMP, .address_v = XSDK_ADDRESS_CLAMP };
	xsdk_texture_sampler_state(&sampler_near, &s);
	s.mag_filter = s.min_filter = XSDK_FILTER_LINEAR;
	xsdk_texture_sampler_state(&sampler_soft, &s);
	quad = xsdk_gpu_alloc(2 * QUAD_FLOATS * QUAD_VERTS * 4);
	if (!quad) return false;
	const float x = 960.0f / (float)present_w;
	// Quad 0: 16:9 tela cheia (1280x720); Quad 1: 4:3 centralizado (960x720)
	const float verts[2 * QUAD_VERTS][QUAD_FLOATS] = {
		{ -1.0f, 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 0.0f }, { -1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, -1.0f, 1.0f, 1.0f },
		{ -x, 1.0f, 0.0f, 0.0f }, { x, 1.0f, 1.0f, 0.0f }, { -x, -1.0f, 0.0f, 1.0f }, { x, -1.0f, 1.0f, 1.0f },
	};
	uint32_t words_v[2 * QUAD_FLOATS * QUAD_VERTS];
	memcpy(words_v, verts, sizeof verts);
	for (int i = 0; i < 2 * QUAD_FLOATS * QUAD_VERTS; i++) quad[i] = words_v[i];
	xsdk_gpu_invalidate(xsdk_gpu_address(quad), 2 * QUAD_FLOATS * QUAD_VERTS * 4);
	st = xsdk_text_open();
	if (!NT_SUCCESS(st)) xsdk_log_write("avp360: text failed: 0x%08X", (unsigned)st);
	have_screen = true;
	return display;
}

static void upload_framebuffer(XsdkTexture *tex, const uint32_t *src, uint32_t w, uint32_t h)
{
	const uint32_t copy_w = w < FB_W ? w : FB_W;
	const uint32_t copy_h = h < FB_H ? h : FB_H;
	if (tex->pitch == FB_W && copy_w == FB_W && w == FB_W) {
		memcpy((void *)tex->texels, src, (size_t)copy_h * FB_W * sizeof(uint32_t));
	} else {
		const size_t row_bytes = (size_t)copy_w * sizeof(uint32_t);
		for (uint32_t y = 0; y < copy_h; y++) {
			memcpy((void *)(tex->texels + y * tex->pitch), src + y * w, row_bytes);
		}
	}
	__asm__ volatile("sync");
}

static void show_frame(uint32_t frame, const uint32_t *fb, uint32_t fb_w, uint32_t fb_h, const char *osd)
{
	uint32_t address = offscreen.address;
	if (!have_offscreen) {
		NTSTATUS st = xsdk_present_begin(&address, 5000);
		if (!NT_SUCCESS(st)) return;
	}
	XsdkTexture *tex = &screen_tex[frame % SCREEN_TEXTURES];
	if (have_offscreen) xsdk_gpu_wait(xsdk_gpu_fence(), 100);
	upload_framebuffer(tex, fb, fb_w, fb_h);
	xsdk_gpu_invalidate(tex->address, tex->size_bytes);
	xsdk_gpu_set_target(present_w, present_h);

	static const float line_dark[5] = { 0.0f, 0.11f, 0.22f, 0.35f, 0.18f };
	static const float gamma_boost[4] = { 0.0f, 0.15f, 0.30f, 0.45f };
	const float look[4] = {
		line_dark[avp_opt[O_LINES]],
		avp_opt[O_LINES] == 4 ? 0.25f : 0.0f,
		wide ? 1280.0f / 2.0f : 640.0f,
		gamma_boost[avp_opt[O_GAMMA]]
	};
	const XsdkShader *pixel = &ps;
	const XsdkBuiltShader *built = &blit_frag;
	int push = BLIT_FRAG_PUSH, sampler = BLIT_FRAG_SAMPLER_0_0;
	if (filter == FILTER_SHARP) { pixel = &ps_sharp; built = &blit_sharp_frag; push = BLIT_SHARP_FRAG_PUSH; sampler = BLIT_SHARP_FRAG_SAMPLER_0_0; }
	if (filter == FILTER_CUBIC) { pixel = &ps_cubic; built = &blit_cubic_frag; push = BLIT_CUBIC_FRAG_PUSH; sampler = BLIT_CUBIC_FRAG_SAMPLER_0_0; }
	if (filter == FILTER_SCALE3X) { pixel = &ps_scale3x; built = &blit_scale3x_frag; push = BLIT_SCALE3X_FRAG_PUSH; sampler = BLIT_SCALE3X_FRAG_SAMPLER_0_0; }
	xsdk_gpu_set_shaders(&vs, pixel);
	xsdk_built_shader_set_literals(&blit_vert);
	xsdk_built_shader_set_literals(built);
	xsdk_gpu_set_pixel_constants((uint32_t)push, look, 1);
	// avp_opt[O_WIDE] == 0 -> 16:9 (quad 0), == 1 -> 4:3 (quad 1)
	const uint32_t quad_offset = (avp_opt[O_WIDE] == 1) ? (QUAD_FLOATS * QUAD_VERTS * 4) : 0;
	xsdk_gpu_set_vertices(0, xsdk_gpu_address(quad) + quad_offset, QUAD_FLOATS * QUAD_VERTS * 4);
	xsdk_gpu_set_texture(sampler, tex, filter == FILTER_NEAREST || filter == FILTER_SCALE3X ? &sampler_near : &sampler_soft);
	xsdk_gpu_draw(XSDK_PRIMITIVE_TRIANGLE_STRIP, QUAD_VERTS);
	if (osd && *osd) xsdk_text_draw(140, 40, 1, 0xff40ff80, osd);
	if (avp_opt[O_FPS]) xsdk_text_draw(20, 10, 2, 0xff40ff40, fps_text);
	xsdk_gpu_resolve(address, present_w, present_h, 0xff000000);
	if (!have_offscreen) xsdk_present_end();
}

static void show_message(const char *text)
{
	if (!have_screen) return;
	uint32_t address = offscreen.address;
	if (!have_offscreen && !NT_SUCCESS(xsdk_present_begin(&address, 5000))) return;
	xsdk_gpu_set_target(present_w, present_h);
	xsdk_text_draw(100, 80, 2, 0xffffffff, text);
	xsdk_gpu_resolve(address, present_w, present_h, 0xff08120c);
	if (!have_offscreen) xsdk_present_end();
}

static void dump_gpu_frame(int frame)
{
	xsdk_gpu_wait(xsdk_gpu_fence(), 2000);
	char path[64];
	snprintf(path, sizeof path, "cache:\\avp_gpu_%d.ppm", frame);
	FILE *f = fopen(path, "wb");
	if (!f) return;
	fprintf(f, "P6\n%u %u\n255\n", (unsigned)present_w, (unsigned)present_h);
	static uint32_t row[1280];
	static uint8_t out[1280 * 3];
	for (uint32_t y = 0; y < present_h; y++) {
		xsdk_texture_download_rows(&offscreen, 0, y, 1, row, sizeof row);
		for (uint32_t x = 0; x < present_w; x++) {
			const uint32_t p = __builtin_bswap32(row[x]);
			out[x * 3] = (uint8_t)(p >> 16);
			out[x * 3 + 1] = (uint8_t)(p >> 8);
			out[x * 3 + 2] = (uint8_t)p;
		}
		fwrite(out, 1, present_w * 3, f);
	}
	fclose(f);
}

// ---- Menu de escolha de jogo (avp_menu.bin 1280x720 RGB ou menu interativo estilizado) ----
// Retangulos dos dois paineis na arte "The Double Feature" (assets/menu_art.jpg redimensionada para 1280x720).
enum { MENU_W = 1280, MENU_H = 720, MENU_EDGE = 20, MENU_BRIGHT = 300, MENU_DIM = 120, MENU_BORDER = 4 };
static const struct { int x0, x1, y0, y1; } menu_cabinet[2] = { { 44, 606, 58, 678 }, { 672, 1238, 58, 678 } };
static const uint32_t menu_glow[2] = { 0xff60ff70, 0xffc8ff70 };

static int menu_ramp(int at, int from, int to)
{
	int d = at - from < to - at ? at - from : to - at;
	if (d <= 0) return 0;
	return d >= MENU_EDGE ? 256 : d * 256 / MENU_EDGE;
}

static void menu_fill(XsdkTexture *tex, const uint8_t *rgb, int chosen)
{
	const int other = 1 - chosen;
	for (int y = 0; y < MENU_H; y++) {
		volatile uint32_t *dst = tex->texels + (uint32_t)y * tex->pitch;
		if (chosen < 0) {
			for (int x = 0; x < MENU_W; x++, rgb += 3)
				dst[x] = 0xff000000u | (uint32_t)(rgb[0] * 56 >> 8) << 16 | (uint32_t)(rgb[1] * 56 >> 8) << 8 | (uint32_t)(rgb[2] * 56 >> 8);
			continue;
		}
		const int ry_c = menu_ramp(y, menu_cabinet[chosen].y0, menu_cabinet[chosen].y1);
		const int ry_o = menu_ramp(y, menu_cabinet[other].y0, menu_cabinet[other].y1);
		const int cx0 = menu_cabinet[chosen].x0, cx1 = menu_cabinet[chosen].x1;
		const int cy0 = menu_cabinet[chosen].y0, cy1 = menu_cabinet[chosen].y1;
		const bool in_rows = y >= cy0 - MENU_BORDER && y < cy1 + MENU_BORDER;
		const bool edge_row = in_rows && (y < cy0 || y >= cy1);
		for (int x = 0; x < MENU_W; x++, rgb += 3) {
			if (in_rows && x >= cx0 - MENU_BORDER && x < cx1 + MENU_BORDER && (edge_row || x < cx0 || x >= cx1)) {
				dst[x] = menu_glow[chosen];
				continue;
			}
			const int c = menu_ramp(x, menu_cabinet[chosen].x0, menu_cabinet[chosen].x1) * ry_c >> 8;
			const int o = menu_ramp(x, menu_cabinet[other].x0, menu_cabinet[other].x1) * ry_o >> 8;
			const int k = 256 + ((MENU_BRIGHT - 256) * c >> 8) + ((MENU_DIM - 256) * o >> 8);
			int r = rgb[0] * k >> 8, g = rgb[1] * k >> 8, b = rgb[2] * k >> 8;
			if (r > 255) r = 255;
			if (g > 255) g = 255;
			if (b > 255) b = 255;
			dst[x] = 0xff000000u | (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b;
		}
	}
	__asm__ volatile("sync");
	xsdk_gpu_invalidate(tex->address, tex->size_bytes);
}

static bool menu_begin(XsdkTexture *tex, uint32_t *address)
{
	*address = offscreen.address;
	if (!have_offscreen && !NT_SUCCESS(xsdk_present_begin(address, 5000))) return false;
	xsdk_gpu_set_target(present_w, present_h);
	if (!tex) return true;
	xsdk_gpu_set_shaders(&vs, &ps);
	xsdk_built_shader_set_literals(&blit_vert);
	xsdk_built_shader_set_literals(&blit_frag);
	const float no_look[4] = { 0.0f, 0.0f, 640.0f, 0.0f };
	xsdk_gpu_set_pixel_constants(BLIT_FRAG_PUSH, no_look, 1);
	xsdk_gpu_set_vertices(0, xsdk_gpu_address(quad), QUAD_FLOATS * QUAD_VERTS * 4);
	xsdk_gpu_set_texture(BLIT_FRAG_SAMPLER_0_0, tex, &sampler_near);
	xsdk_gpu_draw(XSDK_PRIMITIVE_TRIANGLE_STRIP, QUAD_VERTS);
	return true;
}

static void menu_end(uint32_t address)
{
	xsdk_gpu_resolve(address, present_w, present_h, 0xff08120c);
	if (!have_offscreen) xsdk_present_end();
}

static void menu_draw(XsdkTexture *tex, int chosen)
{
	uint32_t address;
	if (!menu_begin(tex, &address)) return;
	// A arte ja traz os logotipos; o texto fica so na barra inferior, fora dos paineis.
	static const char *const names[2] = { "Aliens versus Predator (1999 Gold)", "Aliens versus Predator 2 (2001)" };
	const char *keys = T("A: Jogar   Y: Op\xc3\xa7\xc3\xb5""es   B: Sair");
	uint32_t w = 0, h = 0;
	xsdk_text_draw(48, 690, 1, 0xffa0a0a0, T("Port feito por Matherzin"));
	xsdk_text_measure(names[chosen], 1, &w, &h);
	xsdk_text_draw((MENU_W - (int)w) / 2, 690, 1, menu_glow[chosen], names[chosen]);
	xsdk_text_measure(keys, 1, &w, &h);
	xsdk_text_draw(MENU_W - 48 - (int)w, 690, 1, 0xffffffff, keys);
	menu_end(address);
}

static char menu_keys[128];
static int menu_key_at;
static bool menu_keys_on, menu_key_gap;
static int menu_test;

static void menu_input_open(void)
{
	menu_test = flag_file("avp_menutest.txt", 0);
	FILE *f = fopen("game:\\avp_menukeys.txt", "rb");
	if (!f) return;
	menu_keys_on = fgets(menu_keys, sizeof menu_keys, f) != NULL;
	fclose(f);
}

static bool menu_keys_done(void)
{
	return !menu_keys_on || !menu_keys[menu_key_at] || menu_keys[menu_key_at] == '\r' || menu_keys[menu_key_at] == '\n';
}

static uint32_t menu_read(uint32_t *b)
{
	if (!menu_keys_on) return pad_bits(0, b);
	*b = 0;
	menu_key_gap = !menu_key_gap;
	if (!menu_key_gap || menu_keys_done()) return 0;
	switch (menu_keys[menu_key_at++]) {
	case 'U': return IN_UP;
	case 'D': return IN_DOWN;
	case 'L': return IN_LEFT;
	case 'R': return IN_RIGHT;
	case 'A': *b = INPUT_GAMEPAD_A; break;
	case 'B': *b = INPUT_GAMEPAD_B; break;
	case 'X': *b = INPUT_GAMEPAD_X; break;
	case 'Y': *b = INPUT_GAMEPAD_Y; break;
	case 'S': *b = INPUT_GAMEPAD_START; break;
	}
	return 0;
}

static int options_screen(XsdkTexture *back, bool in_game)
{
	avp_stop_rumble();
	static const char *const actions[PAUSE_ACTIONS] = { "Continuar", "Escolher outro jogo", "Sair do jogo" };
	const int first = in_game ? PAUSE_ACTIONS : 0, rows = first + OPTIONS;
	int at = 0, hold_frames = 0;
	uint32_t last_v = ~0u, last_b = ~0u;
	for (;;) {
		uint32_t address;
		if (menu_begin(back, &address)) {
			if (!in_game) xsdk_text_draw(100, 10, 2, 0xff80ff90, T("Op\xc3\xa7\xc3\xb5""es"));
			const int top = in_game ? 8 : 44, step_y = in_game ? 29 : 33;
			for (int i = 0; i < rows; i++) {
				const int y = top + i * step_y;
				const uint32_t colour = i == at ? 0xffffff40 : i < first ? 0xffffffff : 0xffd0d0d0;
				xsdk_text_draw(70, y, 2, colour, i == at ? ">" : " ");
				xsdk_text_draw(100, y, 2, colour, T(i < first ? actions[i] : option_info[i - first].label));
				if (i >= first) xsdk_text_draw(760, y, 2, colour, T(option_info[i - first].names[avp_opt[i - first]]));
			}
			xsdk_text_draw(100, 675, 1, 0xffc0c0c0,
				T("Direcional: escolher e mudar     B: voltar e guardar     X: tudo como vem de f\xc3\xa1""brica"));
			menu_end(address);
		}
		avp_audio_pump();
		if (menu_test && menu_keys_done()) {
			dump_gpu_frame(10 + menu_test);
			return PAUSE_RESUME;
		}
		uint32_t b = 0;
		const uint32_t v = menu_read(&b);
		if (last_v != ~0u) {
			uint32_t pv = v & ~last_v;
			const uint32_t pb = b & ~last_b;
			if (!menu_keys_on && (v & (IN_UP | IN_DOWN | IN_LEFT | IN_RIGHT))) {
				if ((v & (IN_UP | IN_DOWN | IN_LEFT | IN_RIGHT)) == (last_v & (IN_UP | IN_DOWN | IN_LEFT | IN_RIGHT))) {
					hold_frames++;
					if (hold_frames >= 18 && (hold_frames - 18) % 5 == 0) {
						pv |= v & (IN_UP | IN_DOWN | IN_LEFT | IN_RIGHT);
					}
				} else {
					hold_frames = 0;
				}
			} else {
				hold_frames = 0;
			}

			int step = 0;
			if (pv & IN_UP) { at = (at + rows - 1) % rows; avp_audio_trigger_sfx(AVP_SFX_MENU_MOVE, 0.0f, 1.0f); }
			if (pv & IN_DOWN) { at = (at + 1) % rows; avp_audio_trigger_sfx(AVP_SFX_MENU_MOVE, 0.0f, 1.0f); }
			if (pv & IN_LEFT) step = -1;
			if ((pv & IN_RIGHT) || (pb & INPUT_GAMEPAD_A)) step = 1;
			if (at < first && (pb & (INPUT_GAMEPAD_A | INPUT_GAMEPAD_START))) {
				avp_audio_trigger_sfx(AVP_SFX_MENU_SELECT, 0.0f, 1.1f);
				options_save();
				return at;
			}
			if (step && at >= first) {
				const int o = at - first;
				avp_opt[o] = (avp_opt[o] + step + option_info[o].count) % option_info[o].count;
				avp_audio_trigger_sfx(AVP_SFX_MENU_MOVE, step > 0 ? 0.25f : -0.25f, 1.1f);
			}
			if (pb & INPUT_GAMEPAD_X) {
				options_defaults();
				avp_audio_trigger_sfx(AVP_SFX_MENU_SELECT, 0.0f, 0.9f);
			}
			if (pb & (INPUT_GAMEPAD_B | INPUT_GAMEPAD_BACK | (in_game ? INPUT_GAMEPAD_START : 0))) {
				avp_audio_trigger_sfx(AVP_SFX_MENU_SELECT, 0.0f, 1.0f);
				options_save();
				return PAUSE_RESUME;
			}
		}
		last_v = v;
		last_b = b;
	}
}

static int menu_picture(const bool have[2])
{
	FILE *f = fopen("game:\\avp_menu.bin", "rb");
	if (!f) return -2;
	const size_t bytes = MENU_W * MENU_H * 3;
	uint8_t *rgb = malloc(bytes);
	const bool read_ok = rgb && fread(rgb, 1, bytes, f) == bytes;
	fclose(f);
	static XsdkTexture tex[3];
	if (!read_ok || !NT_SUCCESS(xsdk_texture_create(&tex[0], XSDK_FORMAT_A8R8G8B8, MENU_W, MENU_H, 1, false)) ||
		!NT_SUCCESS(xsdk_texture_create(&tex[1], XSDK_FORMAT_A8R8G8B8, MENU_W, MENU_H, 1, false)) ||
		!NT_SUCCESS(xsdk_texture_create(&tex[2], XSDK_FORMAT_A8R8G8B8, MENU_W, MENU_H, 1, false))) {
		xsdk_log_write("avp360: avp_menu.bin can't be used: it must be %u bytes", (unsigned)bytes);
		free(rgb);
		return -2;
	}
	menu_fill(&tex[0], rgb, 0);
	menu_fill(&tex[1], rgb, 1);
	menu_fill(&tex[2], rgb, -1);
	free(rgb);
	const int test = menu_test;
	int chosen = test == 2 || !have[0] ? 1 : 0, result = -3;
	bool released = menu_keys_on;
	uint32_t last_v = 0, last_b = 0;
	while (result == -3) {
		menu_draw(&tex[chosen], chosen);
		avp_audio_pump();
		if (test && menu_keys_done()) {
			dump_gpu_frame(test);
			result = -1;
			break;
		}
		uint32_t b = 0;
		const uint32_t v = menu_read(&b);
		if (!b && !v) released = true;
		if (released) {
			if ((v & IN_LEFT) && !(last_v & IN_LEFT) && have[0]) { chosen = 0; avp_audio_trigger_sfx(AVP_SFX_MENU_MOVE, -0.4f, 1.0f); }
			if ((v & IN_RIGHT) && !(last_v & IN_RIGHT) && have[1]) { chosen = 1; avp_audio_trigger_sfx(AVP_SFX_MENU_MOVE, 0.4f, 1.0f); }
			if ((b & INPUT_GAMEPAD_Y) && !(last_b & INPUT_GAMEPAD_Y)) {
				avp_audio_trigger_sfx(AVP_SFX_MENU_SELECT, 0.0f, 1.0f);
				options_screen(&tex[2], false);
				released = false;
				if (test && menu_keys_done()) { result = -1; break; }
			}
			else if ((b & INPUT_GAMEPAD_BACK) && (b & INPUT_GAMEPAD_START)) result = -1;
			else if ((b & INPUT_GAMEPAD_A) && !(last_b & INPUT_GAMEPAD_A)) { avp_audio_trigger_sfx(AVP_SFX_MENU_SELECT, 0.0f, 1.2f); result = chosen; }
			else if ((b & INPUT_GAMEPAD_START) && !(last_b & INPUT_GAMEPAD_START)) { avp_audio_trigger_sfx(AVP_SFX_MENU_SELECT, 0.0f, 1.2f); result = chosen; }
			else if ((b & INPUT_GAMEPAD_B) && !(last_b & INPUT_GAMEPAD_B)) result = -1;
		}
		last_v = v;
		last_b = b;
	}
	xsdk_gpu_wait(xsdk_gpu_fence(), 1000);
	xsdk_texture_free(&tex[0]);
	xsdk_texture_free(&tex[1]);
	xsdk_texture_free(&tex[2]);
	return result;
}

// Retorna 1 para AvP 1 (1999), 2 para AvP 2 (2001), ou 0 para sair.
static int choose_game(const bool have[2])
{
	int forced = flag_file("avp_game.txt", 0);
	char again = 0;
	if (xsdk_launch_data(&again, 1) == 1 && (again == '1' || again == '2')) forced = again - '0';
	if (forced == 1 && have[0]) return 1;
	if (forced == 2 && have[1]) return 2;
	if (!have_screen) return have[0] ? 1 : 2;
	const int picked = menu_picture(have);
	if (picked == 0) return 1;
	if (picked == 1) return 2;
	if (picked == -1) return 0;

	bool released = false;
	for (;;) {
		char text[384];
		snprintf(text, sizeof text, "%s\n%s\n\n%s%s\nY  %s\n\n%s",
			T("Aliens vs. Predator 1 & 2 no Xbox 360"),
			T("Port feito por Matherzin (Rebellion 3dc & Monolith LithTech)"),
			have[0] ? "A  Aliens versus Predator (1999 - Gold Edition)\n" : "",
			have[1] ? "X  Aliens versus Predator 2 (2001 - Monolith)\n" : "",
			T("Op\xc3\xa7\xc3\xb5""es"),
			T("Back + Start: sair"));
		show_message(text);
		avp_audio_pump();
		uint32_t b = 0;
		menu_read(&b);
		if (!b) released = true;
		if (!released) continue;
		if ((b & INPUT_GAMEPAD_A) && have[0]) return 1;
		if ((b & INPUT_GAMEPAD_X) && have[1]) return 2;
		if (b & INPUT_GAMEPAD_Y) { options_screen(NULL, false); released = false; continue; }
		if ((b & INPUT_GAMEPAD_BACK) && (b & INPUT_GAMEPAD_START)) return 0;
	}
}

static int g_rumble_frames = 0;
static int g_prev_hp = -1;

void avp_trigger_rumble(uint16_t left_motor, uint16_t right_motor, int frames)
{
	if (!avp_opt[O_RUMBLE] || frames <= 0) return;
	if (frames > g_rumble_frames) g_rumble_frames = frames;
	INPUT_VIBRATION vib = { .left_motor_speed = left_motor, .right_motor_speed = right_motor };
	XamInputSetState(0, 0, &vib);
}

void avp_stop_rumble(void)
{
	g_rumble_frames = 0;
	INPUT_VIBRATION vib = { 0, 0 };
	XamInputSetState(0, 0, &vib);
}

static void rumble_update(int current_hp)
{
	if (!avp_opt[O_RUMBLE]) {
		if (g_rumble_frames > 0) avp_stop_rumble();
		g_prev_hp = current_hp;
		return;
	}
	if (current_hp >= 0 && g_prev_hp >= 0 && current_hp < g_prev_hp) {
		const int dmg = g_prev_hp - current_hp > 40 ? 40 : g_prev_hp - current_hp;
		const uint16_t left = (uint16_t)(0x5000 + dmg * 0x2a0);
		avp_trigger_rumble(left, left / 2, 6 + dmg / 3);
	} else if (g_rumble_frames > 0 && --g_rumble_frames == 0) {
		avp_stop_rumble();
	}
	g_prev_hp = current_hp;
}

int main(void)
{
	xsdk_log_write("avp360 v2.0: Aliens vs. Predator 1 & 2 no Xbox 360. Port feito por Matherzin");
	xsdk_thread_move(2);
	bool screen = video_open();
	options_load();
	avp_audio_init();

	// Os jogos ficam em game:\avp1 e game:\avp2 (dados do proprio jogador); cada um roda no seu .xex.
	bool have[2] = { game_present(kAvp1Files), game_present(kAvp2Files) };
	xsdk_log_write("avp360: AvP1=%s, AvP2=%s", have[0] ? "found" : "missing", have[1] ? "found" : "missing");

	if (!have[0] && !have[1]) {
		xsdk_log_write("avp360: no game found");
		for (;;) {
			show_message(T("Nenhum jogo encontrado nesta pasta.\n\n"
				"Coloque aqui os seus proprios arquivos originais:\n"
				"  avp1\\  (avp_huds.rif, language.txt, fastfiles do AvP 1 Gold)\n"
				"  avp2\\  (AVP2.REZ, AVP2DLL.REZ, AVP2L.REZ do AvP 2)\n\n"
				"Veja o LEIA-ME.  Back + Start: sair"));
			uint32_t b = 0;
			pad_bits(0, &b);
			if (!screen || ((b & INPUT_GAMEPAD_BACK) && (b & INPUT_GAMEPAD_START))) return 1;
		}
	}

	menu_input_open();
	const int game_id = choose_game(have);
	if (!game_id) {
		avp_stop_rumble();
		avp_audio_shutdown();
		return 0;
	}

	// Os jogos de verdade rodam em .xex proprios, ao lado deste: avp1.xex (motor da Rebellion via NakedAVP) e
	// avp2.xex (LithTech 2.2 do AVP2 Reconstructed). Cada um volta ao menu ao sair.
	if (game_id == 1 || game_id == 2) {
		const char *xex = game_id == 1 ? "game:\\avp1.xex" : "game:\\avp2.xex";
		avp_stop_rumble();
		avp_audio_shutdown();
		xsdk_log_write("avp360: abrindo %s", xex);
		xsdk_launch(xex, NULL, 0);
		xsdk_log_write("avp360: %s nao abriu (errno %d)", xex, errno);
		show_message(game_id == 1
			? T("N\xc3\xa3o encontrei o avp1.xex ao lado do AvP360.xex.\n\nCopie a pasta build inteira para o console.")
			: T("N\xc3\xa3o encontrei o avp2.xex ao lado do AvP360.xex.\n\nCopie a pasta build inteira para o console."));
		for (;;) {
			uint32_t b = 0;
			pad_bits(0, &b);
			if (b & (INPUT_GAMEPAD_B | INPUT_GAMEPAD_BACK)) return 1;
		}
	}
	return 0;
}
