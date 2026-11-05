/* avp360: o subconjunto de OpenGL 1.x que o renderizador do NakedAVP usa (src/renderer/opengl.c e
 * src/frontend/main.c), implementado sobre o xsdk_gpu do Xenos.
 *
 * O jogo entrega vertices ja transformados (x, y, z, w homogeneos), UV e cor RGBA8 em arrays do cliente, e desenha com
 * glDrawElements(GL_TRIANGLES, GL_UNSIGNED_SHORT). Aqui cada draw copia os vertices usados e os indices para um anel de
 * memoria da GPU, escolhe os estados (blend, profundidade, cull, alfa) e desenha com world.vert/world.frag.
 *
 * Quadro: comeca no primeiro clear/draw depois de um xgl_swap, termina no xgl_swap com o resolve para o front buffer.
 * Cor e profundidade ficam na eDRAM em 1280x720. */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL_opengl.h>

#include <xsdk/gpu.h>
#include <xsdk/log.h>
#include <xsdk/present.h>
#include <xsdk/shader.h>
#include <xsdk/texture.h>

#include "avp1_shaders.h"
#include "oglfunc.h"

void avp1_log(const char *fmt, ...);

enum {
	SCREEN_W = 1280,
	SCREEN_H = 720,
	MAX_TEXTURES = 4096,
	/* Anel de vertices e indices: 3 segmentos, um por quadro em voo. */
	RING_SEGMENTS = 3,
	RING_SEGMENT_BYTES = 6 << 20,
	VERTEX_WORDS = 7, /* x y z w u v cor */
	PENDING_FREES = 1024,
};

typedef struct {
	XsdkTexture tex;
	bool used, allocated, dirty_sampler;
	GLint min_filter, mag_filter, wrap_s, wrap_t;
	GLfloat anisotropy;
	XsdkSamplerState sampler;
} XglTexture;

typedef struct {
	GLint size;
	GLsizei stride;
	const void *ptr;
	bool enabled;
} XglArray;

static struct {
	bool open, offscreen, in_frame;
	uint32_t present_w, present_h, frame_address;
	XsdkTexture offscreen_tex;
	XsdkShader vs, ps;
	uint32_t depth_tile;

	/* Estado do OpenGL. */
	GLint vp[4];
	GLfloat clear[4];
	bool blend, depth_test, texture_2d, alpha_test, cull, polygon_offset;
	GLenum blend_src, blend_dst, depth_func, cull_face, front_face, env_mode;
	bool depth_mask;
	GLfloat alpha_ref;
	GLfloat poly_factor, poly_units;
	GLfloat color[4];
	XglArray va, ca, ta;
	GLuint bound;

	XglTexture textures[MAX_TEXTURES];
	XsdkTexture white;
	XsdkSamplerState white_sampler;

	/* Anel. */
	volatile uint32_t *ring;
	uint32_t ring_address;
	uint32_t segment, segment_used;
	uint32_t segment_fence[RING_SEGMENTS];

	struct { XsdkTexture tex; uint32_t fence; } pending[PENDING_FREES];
	uint32_t pending_count;

	uint32_t frames, draws;
} g;

static uint32_t float_bits(float f)
{
	uint32_t u;
	memcpy(&u, &f, 4);
	return u;
}

static uint32_t blend_factor(GLenum f)
{
	switch (f) {
	case GL_ZERO: return XSDK_BLEND_ZERO;
	case GL_ONE: return XSDK_BLEND_ONE;
	case GL_SRC_COLOR: return XSDK_BLEND_SRC_COLOR;
	case GL_ONE_MINUS_SRC_COLOR: return XSDK_BLEND_ONE_MINUS_SRC_COLOR;
	case GL_SRC_ALPHA: return XSDK_BLEND_SRC_ALPHA;
	case GL_ONE_MINUS_SRC_ALPHA: return XSDK_BLEND_ONE_MINUS_SRC_ALPHA;
	case GL_DST_COLOR: return XSDK_BLEND_DST_COLOR;
	case GL_ONE_MINUS_DST_COLOR: return XSDK_BLEND_ONE_MINUS_DST_COLOR;
	case GL_DST_ALPHA: return XSDK_BLEND_DST_ALPHA;
	case GL_ONE_MINUS_DST_ALPHA: return XSDK_BLEND_ONE_MINUS_DST_ALPHA;
	case GL_SRC_ALPHA_SATURATE: return XSDK_BLEND_SRC_ALPHA_SATURATE;
	}
	return XSDK_BLEND_ONE;
}

/* GL_NEVER..GL_ALWAYS (0x200..0x207) estao na mesma ordem que XSDK_COMPARE_*. */
static uint32_t compare(GLenum f)
{
	return f >= GL_NEVER && f <= GL_ALWAYS ? f - GL_NEVER : XSDK_COMPARE_LESS_EQUAL;
}

static void free_pending(bool all)
{
	uint32_t keep = 0;
	for (uint32_t i = 0; i < g.pending_count; i++) {
		if (all || xsdk_gpu_fence_done(g.pending[i].fence))
			xsdk_texture_free(&g.pending[i].tex);
		else
			g.pending[keep++] = g.pending[i];
	}
	g.pending_count = keep;
}

static void defer_free(XsdkTexture *tex)
{
	if (g.pending_count == PENDING_FREES) {
		xsdk_gpu_wait(xsdk_gpu_fence(), 1000);
		free_pending(true);
	}
	g.pending[g.pending_count].tex = *tex;
	g.pending[g.pending_count].fence = xsdk_gpu_fence();
	g.pending_count++;
	memset(tex, 0, sizeof *tex);
}

static void set_viewport_now(void)
{
	/* glViewport conta y de baixo para cima; o Xenos, de cima para baixo. */
	const float y = (float)(SCREEN_H - g.vp[1] - g.vp[3]);
	xsdk_gpu_set_viewport((float)g.vp[0], y, (float)g.vp[2], (float)g.vp[3], 0.0f, 1.0f);
}

static void begin_frame(void)
{
	if (g.in_frame) return;
	g.frame_address = g.offscreen_tex.address;
	if (!g.offscreen && !NT_SUCCESS(xsdk_present_begin(&g.frame_address, 5000))) {
		avp1_log("xsdk_present_begin expirou");
		return;
	}
	/* O segmento do anel que este quadro vai usar foi desenhado ha RING_SEGMENTS quadros. */
	g.segment = (g.segment + 1) % RING_SEGMENTS;
	if (g.segment_fence[g.segment]) xsdk_gpu_wait(g.segment_fence[g.segment], 1000);
	g.segment_used = 0;
	free_pending(false);

	xsdk_gpu_set_target_at(SCREEN_W, SCREEN_H, 0, g.depth_tile);
	set_viewport_now();
	g.in_frame = true;
}

/* Reserva bytes no segmento do quadro; se acabar, espera a GPU e recomeca o segmento. */
static volatile uint32_t *ring_take(uint32_t bytes, uint32_t *address)
{
	bytes = (bytes + 63u) & ~63u;
	if (g.segment_used + bytes > RING_SEGMENT_BYTES) {
		avp1_log("anel cheio no quadro %u: esperando a GPU", g.frames);
		xsdk_gpu_wait(xsdk_gpu_fence(), 1000);
		g.segment_used = 0;
	}
	const uint32_t offset = g.segment * RING_SEGMENT_BYTES + g.segment_used;
	g.segment_used += bytes;
	*address = g.ring_address + offset;
	return g.ring + offset / 4;
}

static void apply_states(void)
{
	XsdkBlend b = {
		.color_source = XSDK_BLEND_ONE, .color_dest = XSDK_BLEND_ZERO, .color_op = 0,
		.alpha_source = XSDK_BLEND_ONE, .alpha_dest = XSDK_BLEND_ZERO, .alpha_op = 0,
		.write_mask = XSDK_WRITE_ALL,
	};
	if (g.blend) {
		b.color_source = b.alpha_source = blend_factor(g.blend_src);
		b.color_dest = b.alpha_dest = blend_factor(g.blend_dst);
	}
	XsdkBlendState bs;
	xsdk_gpu_blend_state(&bs, &b);
	xsdk_gpu_set_blend(&bs);

	XsdkDepthStencil d = {
		.depth_test = g.depth_test,
		.depth_write = g.depth_test && g.depth_mask,
		.depth_compare = compare(g.depth_func),
	};
	XsdkDepthStencilState ds;
	xsdk_gpu_depth_stencil_state(&ds, &d);
	xsdk_gpu_set_depth_stencil(&ds);

	XsdkRaster r = {
		.cull = !g.cull ? XSDK_CULL_NONE : g.cull_face == GL_FRONT ? XSDK_CULL_FRONT : XSDK_CULL_BACK,
		.front_clockwise = g.front_face == GL_CW,
		.depth_bias = g.polygon_offset ? g.poly_units / 16777216.0f : 0.0f,
		.depth_bias_slope = g.polygon_offset ? g.poly_factor : 0.0f,
		.fill = XSDK_FILL_SOLID,
	};
	XsdkRasterState rs;
	xsdk_gpu_raster_state(&rs, &r);
	xsdk_gpu_set_raster(&rs);
}

static void bind_texture_for_draw(void)
{
	XglTexture *t = g.texture_2d && g.bound < MAX_TEXTURES ? &g.textures[g.bound] : NULL;
	if (!t || !t->allocated) {
		xsdk_gpu_set_texture(WORLD_FRAG_SAMPLER_0_0, &g.white, &g.white_sampler);
		return;
	}
	if (t->dirty_sampler) {
		const bool linear_mag = t->mag_filter != GL_NEAREST;
		const bool linear_min = t->min_filter != GL_NEAREST && t->min_filter != GL_NEAREST_MIPMAP_NEAREST;
		XsdkSampler s = {
			.mag_filter = linear_mag ? XSDK_FILTER_LINEAR : XSDK_FILTER_POINT,
			.min_filter = linear_min ? XSDK_FILTER_LINEAR : XSDK_FILTER_POINT,
			.mip_filter = XSDK_FILTER_POINT,
			.address_u = t->wrap_s == GL_REPEAT ? XSDK_ADDRESS_WRAP : XSDK_ADDRESS_CLAMP,
			.address_v = t->wrap_t == GL_REPEAT ? XSDK_ADDRESS_WRAP : XSDK_ADDRESS_CLAMP,
			.max_anisotropy = t->anisotropy > 1.0f ? (uint32_t)t->anisotropy : 0,
		};
		xsdk_texture_sampler_state(&t->sampler, &s);
		t->dirty_sampler = false;
	}
	xsdk_gpu_set_texture(WORLD_FRAG_SAMPLER_0_0, &t->tex, &t->sampler);
}

static void draw(const GLushort *indices, uint32_t count)
{
	if (!g.open || count == 0 || !g.va.enabled || !g.va.ptr) return;
	begin_frame();
	if (!g.in_frame) return;

	uint32_t max_index = 0;
	for (uint32_t i = 0; i < count; i++)
		if (indices[i] > max_index) max_index = indices[i];
	const uint32_t verts = max_index + 1;

	uint32_t vaddr, iaddr;
	volatile uint32_t *v = ring_take(verts * VERTEX_WORDS * 4, &vaddr);
	const uint32_t white = 0xffffffffu;
	uint32_t const_color = white;
	if (!g.ca.enabled) {
		const uint32_t r = (uint32_t)(g.color[0] * 255.0f + 0.5f), gg = (uint32_t)(g.color[1] * 255.0f + 0.5f);
		const uint32_t b = (uint32_t)(g.color[2] * 255.0f + 0.5f), a = (uint32_t)(g.color[3] * 255.0f + 0.5f);
		/* UBYTE4N le x no byte de baixo: a palavra vai como A B G R. */
		const_color = a << 24 | b << 16 | gg << 8 | r;
	}
	const GLsizei vstride = g.va.stride ? g.va.stride : g.va.size * 4;
	const GLsizei tstride = g.ta.stride ? g.ta.stride : 8;
	const GLsizei cstride = g.ca.stride ? g.ca.stride : 4;
	for (uint32_t i = 0; i < verts; i++, v += VERTEX_WORDS) {
		const float *p = (const float *)((const uint8_t *)g.va.ptr + (size_t)i * vstride);
		v[0] = float_bits(p[0]);
		v[1] = float_bits(p[1]);
		v[2] = float_bits(g.va.size > 2 ? p[2] : 0.0f);
		v[3] = float_bits(g.va.size > 3 ? p[3] : 1.0f);
		if (g.ta.enabled && g.ta.ptr) {
			const float *t = (const float *)((const uint8_t *)g.ta.ptr + (size_t)i * tstride);
			v[4] = float_bits(t[0]);
			v[5] = float_bits(t[1]);
		} else {
			v[4] = v[5] = 0;
		}
		if (g.ca.enabled && g.ca.ptr) {
			const uint8_t *c = (const uint8_t *)g.ca.ptr + (size_t)i * cstride;
			v[6] = (uint32_t)c[3] << 24 | (uint32_t)c[2] << 16 | (uint32_t)c[1] << 8 | c[0];
		} else {
			v[6] = const_color;
		}
	}
	volatile uint32_t *ix = ring_take(count * 2 + 4, &iaddr);
	for (uint32_t i = 0; i + 1 < count; i += 2) ix[i / 2] = (uint32_t)indices[i] << 16 | indices[i + 1];
	if (count & 1) ix[count / 2] = (uint32_t)indices[count - 1] << 16;
	xsdk_gpu_invalidate(vaddr, verts * VERTEX_WORDS * 4);
	xsdk_gpu_invalidate(iaddr, count * 2 + 4);

	xsdk_gpu_set_shaders(&g.vs, &g.ps);
	xsdk_built_shader_set_literals(&world_vert);
	xsdk_built_shader_set_literals(&world_frag);
	const bool textured = g.texture_2d && g.bound < MAX_TEXTURES && g.textures[g.bound].allocated;
	const float k[4] = {
		0.0f,
		textured && g.env_mode == GL_COMBINE ? 1.0f : 0.0f,
		textured && g.env_mode == GL_REPLACE ? 1.0f : 0.0f,
		g.alpha_test ? g.alpha_ref : -1.0f,
	};
	xsdk_gpu_set_pixel_constants(WORLD_FRAG_PUSH, k, 1);
	apply_states();
	bind_texture_for_draw();
	xsdk_gpu_set_vertices(0, vaddr, verts * VERTEX_WORDS * 4);
	xsdk_gpu_draw_indexed(XSDK_PRIMITIVE_TRIANGLE_LIST, iaddr, count, false);
	g.draws++;
}

/* ---- Funcoes GL ---- */

static void APIENTRY x_AlphaFunc(GLenum func, GLclampf ref) { (void)func; g.alpha_ref = ref; }
static void APIENTRY x_BindTexture(GLenum target, GLuint tex) { (void)target; g.bound = tex; }
static void APIENTRY x_BlendFunc(GLenum s, GLenum d) { g.blend_src = s; g.blend_dst = d; }
static void APIENTRY x_ClearColor(GLclampf r, GLclampf gg, GLclampf b, GLclampf a)
{
	g.clear[0] = r; g.clear[1] = gg; g.clear[2] = b; g.clear[3] = a;
}
static void APIENTRY x_Color4f(GLfloat r, GLfloat gg, GLfloat b, GLfloat a)
{
	g.color[0] = r; g.color[1] = gg; g.color[2] = b; g.color[3] = a;
}
static void APIENTRY x_ColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *p)
{
	(void)type; g.ca.size = size; g.ca.stride = stride; g.ca.ptr = p;
}
static void APIENTRY x_CullFace(GLenum mode) { g.cull_face = mode; }
static void APIENTRY x_DepthFunc(GLenum f) { g.depth_func = f; }
static void APIENTRY x_DepthMask(GLboolean m) { g.depth_mask = m; }
static void APIENTRY x_DepthRange(GLclampd n, GLclampd f) { (void)n; (void)f; }

static void set_cap(GLenum cap, bool on)
{
	switch (cap) {
	case GL_BLEND: g.blend = on; break;
	case GL_DEPTH_TEST: g.depth_test = on; break;
	case GL_TEXTURE_2D: g.texture_2d = on; break;
	case GL_ALPHA_TEST: g.alpha_test = on; break;
	case GL_CULL_FACE: g.cull = on; break;
	case GL_POLYGON_OFFSET_FILL: g.polygon_offset = on; break;
	}
}
static void APIENTRY x_Enable(GLenum cap) { set_cap(cap, true); }
static void APIENTRY x_Disable(GLenum cap) { set_cap(cap, false); }

static XglArray *client_array(GLenum a)
{
	switch (a) {
	case GL_VERTEX_ARRAY: return &g.va;
	case GL_COLOR_ARRAY: return &g.ca;
	case GL_TEXTURE_COORD_ARRAY: return &g.ta;
	}
	return NULL;
}
static void APIENTRY x_EnableClientState(GLenum a) { XglArray *c = client_array(a); if (c) c->enabled = true; }
static void APIENTRY x_DisableClientState(GLenum a) { XglArray *c = client_array(a); if (c) c->enabled = false; }

static void APIENTRY x_DrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
	if (mode != GL_TRIANGLES || type != GL_UNSIGNED_SHORT) {
		static int warned;
		if (!warned++) avp1_log("glDrawElements nao suportado: modo 0x%x tipo 0x%x", mode, type);
		return;
	}
	draw((const GLushort *)indices, (uint32_t)count);
}

static void APIENTRY x_Clear(GLbitfield mask)
{
	if (!g.open) return;
	begin_frame();
	if (!g.in_frame) return;
	/* Um retangulo de tela cheia faz o clear: profundidade sempre escrita no fundo, cor so se pedida. */
	static const float quad[4][4] = { { -1, -1, 1, 1 }, { 1, -1, 1, 1 }, { 1, 1, 1, 1 }, { -1, 1, 1, 1 } };
	static const GLushort idx[6] = { 0, 1, 2, 0, 2, 3 };
	const XglArray va = g.va, ca = g.ca, ta = g.ta;
	const bool blend = g.blend, dt = g.depth_test, dm = g.depth_mask, tex = g.texture_2d, at = g.alpha_test, cull = g.cull;
	const GLenum df = g.depth_func;
	GLfloat color[4];
	memcpy(color, g.color, sizeof color);
	const GLint vp[4] = { g.vp[0], g.vp[1], g.vp[2], g.vp[3] };

	g.va = (XglArray){ 4, 16, quad, true };
	g.ca.enabled = g.ta.enabled = false;
	g.blend = g.texture_2d = g.alpha_test = g.cull = false;
	g.depth_test = (mask & GL_DEPTH_BUFFER_BIT) != 0;
	g.depth_mask = true;
	g.depth_func = GL_ALWAYS;
	memcpy(g.color, g.clear, sizeof g.color);
	g.vp[0] = 0; g.vp[1] = 0; g.vp[2] = SCREEN_W; g.vp[3] = SCREEN_H;
	set_viewport_now();
	if (!(mask & GL_COLOR_BUFFER_BIT)) {
		/* So profundidade: mesmo quad, sem escrever cor. */
		g.blend = true;
		g.blend_src ^= 0; /* mantem */
		const GLenum s = g.blend_src, d = g.blend_dst;
		g.blend_src = GL_ZERO;
		g.blend_dst = GL_ONE;
		draw(idx, 6);
		g.blend_src = s;
		g.blend_dst = d;
	} else {
		draw(idx, 6);
	}
	g.va = va; g.ca = ca; g.ta = ta;
	g.blend = blend; g.depth_test = dt; g.depth_mask = dm; g.texture_2d = tex; g.alpha_test = at; g.cull = cull;
	g.depth_func = df;
	memcpy(g.color, color, sizeof color);
	memcpy(g.vp, vp, sizeof vp);
	set_viewport_now();
}

static void APIENTRY x_FrontFace(GLenum mode) { g.front_face = mode; }

static void APIENTRY x_GenTextures(GLsizei n, GLuint *out)
{
	for (GLsizei i = 0; i < n; i++) {
		out[i] = 0;
		for (GLuint t = 1; t < MAX_TEXTURES; t++) {
			if (!g.textures[t].used) {
				memset(&g.textures[t], 0, sizeof g.textures[t]);
				g.textures[t].used = true;
				g.textures[t].min_filter = GL_NEAREST_MIPMAP_LINEAR;
				g.textures[t].mag_filter = GL_LINEAR;
				g.textures[t].wrap_s = g.textures[t].wrap_t = GL_REPEAT;
				g.textures[t].dirty_sampler = true;
				out[i] = t;
				break;
			}
		}
		if (!out[i]) avp1_log("glGenTextures: acabaram os nomes de textura");
	}
}

static void APIENTRY x_DeleteTextures(GLsizei n, const GLuint *names)
{
	for (GLsizei i = 0; i < n; i++) {
		const GLuint t = names[i];
		if (t == 0 || t >= MAX_TEXTURES || !g.textures[t].used) continue;
		if (g.textures[t].allocated) defer_free(&g.textures[t].tex);
		g.textures[t].used = g.textures[t].allocated = false;
		if (g.bound == t) g.bound = 0;
	}
}

static GLenum APIENTRY x_GetError(void) { return GL_NO_ERROR; }

static void APIENTRY x_GetFloatv(GLenum p, GLfloat *v)
{
	if (p == GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT) *v = 16.0f;
	else *v = 0.0f;
}

static void APIENTRY x_GetIntegerv(GLenum p, GLint *v)
{
	switch (p) {
	case GL_MAX_TEXTURE_SIZE: *v = 4096; break;
	case GL_VIEWPORT: memcpy(v, g.vp, sizeof g.vp); break;
	default: *v = 0; break;
	}
}

static const GLubyte *APIENTRY x_GetString(GLenum name)
{
	switch (name) {
	case GL_VENDOR: return (const GLubyte *)"avp360";
	case GL_RENDERER: return (const GLubyte *)"Xenos (xsdk)";
	case GL_VERSION: return (const GLubyte *)"1.3 avp360";
	case GL_EXTENSIONS: return (const GLubyte *)"GL_EXT_texture_filter_anisotropic";
	}
	return (const GLubyte *)"";
}

static void APIENTRY x_GetTexParameterfv(GLenum t, GLenum p, GLfloat *v) { (void)t; (void)p; *v = 0.0f; }
static void APIENTRY x_Hint(GLenum t, GLenum m) { (void)t; (void)m; }
static void APIENTRY x_PixelStorei(GLenum p, GLint v) { (void)p; (void)v; }
static void APIENTRY x_PolygonOffset(GLfloat f, GLfloat u) { g.poly_factor = f; g.poly_units = u; }
static void APIENTRY x_ReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum t, GLvoid *p)
{
	(void)x; (void)y; (void)f; (void)t;
	/* Screenshots (PrintScreen) nao existem no console: devolve preto. */
	if (p) memset(p, 0, (size_t)w * h * 3);
}
static void APIENTRY x_ShadeModel(GLenum m) { (void)m; }
static void APIENTRY x_TexEnvf(GLenum t, GLenum p, GLfloat v) { (void)t; (void)p; (void)v; }
static void APIENTRY x_TexEnvfv(GLenum t, GLenum p, const GLfloat *v) { (void)t; (void)p; (void)v; }
static void APIENTRY x_TexEnvi(GLenum t, GLenum p, GLint v)
{
	(void)t;
	if (p == GL_TEXTURE_ENV_MODE) g.env_mode = (GLenum)v;
}

static XglTexture *bound_texture(void)
{
	return g.bound && g.bound < MAX_TEXTURES && g.textures[g.bound].used ? &g.textures[g.bound] : NULL;
}

static void APIENTRY x_TexParameteri(GLenum target, GLenum p, GLint v)
{
	(void)target;
	XglTexture *t = bound_texture();
	if (!t) return;
	switch (p) {
	case GL_TEXTURE_MIN_FILTER: t->min_filter = v; break;
	case GL_TEXTURE_MAG_FILTER: t->mag_filter = v; break;
	case GL_TEXTURE_WRAP_S: t->wrap_s = v; break;
	case GL_TEXTURE_WRAP_T: t->wrap_t = v; break;
	default: return;
	}
	t->dirty_sampler = true;
}

static void APIENTRY x_TexParameterf(GLenum target, GLenum p, GLfloat v)
{
	XglTexture *t = bound_texture();
	if (t && p == GL_TEXTURE_MAX_ANISOTROPY_EXT) {
		t->anisotropy = v;
		t->dirty_sampler = true;
		return;
	}
	x_TexParameteri(target, p, (GLint)v);
}

/* Converte uma linha de texels do formato do GL para A8R8G8B8 (palavra 0xAARRGGBB). */
static void convert_row(uint32_t *dst, const void *src, GLsizei w, GLenum format, GLenum type)
{
	if (type == GL_UNSIGNED_SHORT_5_6_5) {
		const uint16_t *s = (const uint16_t *)src;
		for (GLsizei x = 0; x < w; x++) {
			const uint32_t p = s[x];
			const uint32_t r = (p >> 11) & 31, gg = (p >> 5) & 63, b = p & 31;
			dst[x] = 0xff000000u | (r << 3 | r >> 2) << 16 | (gg << 2 | gg >> 4) << 8 | (b << 3 | b >> 2);
		}
		return;
	}
	const uint8_t *s = (const uint8_t *)src;
	if (format == GL_RGB) {
		for (GLsizei x = 0; x < w; x++, s += 3)
			dst[x] = 0xff000000u | (uint32_t)s[0] << 16 | (uint32_t)s[1] << 8 | s[2];
	} else {
		for (GLsizei x = 0; x < w; x++, s += 4)
			dst[x] = (uint32_t)s[3] << 24 | (uint32_t)s[0] << 16 | (uint32_t)s[1] << 8 | s[2];
	}
}

static uint32_t bytes_per_texel(GLenum format, GLenum type)
{
	if (type == GL_UNSIGNED_SHORT_5_6_5) return 2;
	return format == GL_RGB ? 3 : 4;
}

static void upload_rect(XglTexture *t, GLint xo, GLint yo, GLsizei w, GLsizei h, GLenum format, GLenum type,
	const void *pixels)
{
	if (!pixels || w <= 0 || h <= 0) return;
	if (xo < 0 || yo < 0 || (uint32_t)(xo + w) > t->tex.width || (uint32_t)(yo + h) > t->tex.height) return;
	const uint32_t bpt = bytes_per_texel(format, type);
	uint32_t row[4096];
	for (GLsizei y = 0; y < h; y++) {
		convert_row(row, (const uint8_t *)pixels + (size_t)y * w * bpt, w, format, type);
		volatile uint32_t *d = t->tex.texels + (uint32_t)(yo + y) * t->tex.pitch + (uint32_t)xo;
		for (GLsizei x = 0; x < w; x++) d[x] = row[x];
	}
	__asm__ volatile("sync");
	xsdk_gpu_invalidate(t->tex.address, t->tex.size_bytes);
}

static void APIENTRY x_TexImage2D(GLenum target, GLint level, GLint internal, GLsizei w, GLsizei h, GLint border,
	GLenum format, GLenum type, const GLvoid *pixels)
{
	(void)target; (void)internal; (void)border;
	XglTexture *t = bound_texture();
	if (!t || level != 0 || w <= 0 || h <= 0 || w > 4096 || h > 4096) return;
	if (t->allocated && (t->tex.width != (uint32_t)w || t->tex.height != (uint32_t)h)) {
		defer_free(&t->tex);
		t->allocated = false;
	}
	if (!t->allocated) {
		NTSTATUS st = xsdk_texture_create(&t->tex, XSDK_FORMAT_A8R8G8B8, (uint32_t)w, (uint32_t)h, 1, false);
		if (!NT_SUCCESS(st)) {
			avp1_log("textura %dx%d falhou: 0x%08X", w, h, (unsigned)st);
			return;
		}
		t->allocated = true;
	}
	upload_rect(t, 0, 0, w, h, format, type, pixels);
}

static void APIENTRY x_TexSubImage2D(GLenum target, GLint level, GLint xo, GLint yo, GLsizei w, GLsizei h,
	GLenum format, GLenum type, const GLvoid *pixels)
{
	(void)target;
	XglTexture *t = bound_texture();
	if (t && t->allocated && level == 0) upload_rect(t, xo, yo, w, h, format, type, pixels);
}

static void APIENTRY x_TexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *p)
{
	(void)type; g.ta.size = size; g.ta.stride = stride; g.ta.ptr = p;
}
static void APIENTRY x_VertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *p)
{
	(void)type; g.va.size = size; g.va.stride = stride; g.va.ptr = p;
}
static void APIENTRY x_Viewport(GLint x, GLint y, GLsizei w, GLsizei h)
{
	g.vp[0] = x; g.vp[1] = y; g.vp[2] = w; g.vp[3] = h;
	if (g.in_frame) set_viewport_now();
}

/* Os ponteiros que oglfunc.c definia. */
PFNGLALPHAFUNCPROC pglAlphaFunc;
PFNGLBINDTEXTUREPROC pglBindTexture;
PFNGLBLENDFUNCPROC pglBlendFunc;
PFNGLCLEARPROC pglClear;
PFNGLCLEARCOLORPROC pglClearColor;
PFNGLCOLOR4FPROC pglColor4f;
PFNGLCOLORPOINTERPROC pglColorPointer;
PFNGLCULLFACEPROC pglCullFace;
PFNGLDELETETEXTURESPROC pglDeleteTextures;
PFNGLDEPTHFUNCPROC pglDepthFunc;
PFNGLDEPTHMASKPROC pglDepthMask;
PFNGLDEPTHRANGEPROC pglDepthRange;
PFNGLDISABLEPROC pglDisable;
PFNGLDISABLECLIENTSTATEPROC pglDisableClientState;
PFNGLDRAWELEMENTSPROC pglDrawElements;
PFNGLENABLEPROC pglEnable;
PFNGLENABLECLIENTSTATEPROC pglEnableClientState;
PFNGLFRONTFACEPROC pglFrontFace;
PFNGLGENTEXTURESPROC pglGenTextures;
PFNGLGETERRORPROC pglGetError;
PFNGLGETFLOATVPROC pglGetFloatv;
PFNGLGETINTEGERVPROC pglGetIntegerv;
PFNGLGETSTRINGPROC pglGetString;
PFNGLGETTEXPARAMETERFVPROC pglGetTexParameterfv;
PFNGLHINTPROC pglHint;
PFNGLPIXELSTOREIPROC pglPixelStorei;
PFNGLPOLYGONOFFSETPROC pglPolygonOffset;
PFNGLREADPIXELSPROC pglReadPixels;
PFNGLSHADEMODELPROC pglShadeModel;
PFNGLTEXCOORDPOINTERPROC pglTexCoordPointer;
PFNGLTEXENVFPROC pglTexEnvf;
PFNGLTEXENVFVPROC pglTexEnvfv;
PFNGLTEXENVIPROC pglTexEnvi;
PFNGLTEXIMAGE2DPROC pglTexImage2D;
PFNGLTEXPARAMETERFPROC pglTexParameterf;
PFNGLTEXPARAMETERIPROC pglTexParameteri;
PFNGLTEXSUBIMAGE2DPROC pglTexSubImage2D;
PFNGLVERTEXPOINTERPROC pglVertexPointer;
PFNGLVIEWPORTPROC pglViewport;

void load_ogl_functions(int mode)
{
	(void)mode;
	pglAlphaFunc = x_AlphaFunc;
	pglBindTexture = x_BindTexture;
	pglBlendFunc = x_BlendFunc;
	pglClear = x_Clear;
	pglClearColor = x_ClearColor;
	pglColor4f = x_Color4f;
	pglColorPointer = x_ColorPointer;
	pglCullFace = x_CullFace;
	pglDeleteTextures = x_DeleteTextures;
	pglDepthFunc = x_DepthFunc;
	pglDepthMask = x_DepthMask;
	pglDepthRange = x_DepthRange;
	pglDisable = x_Disable;
	pglDisableClientState = x_DisableClientState;
	pglDrawElements = x_DrawElements;
	pglEnable = x_Enable;
	pglEnableClientState = x_EnableClientState;
	pglFrontFace = x_FrontFace;
	pglGenTextures = x_GenTextures;
	pglGetError = x_GetError;
	pglGetFloatv = x_GetFloatv;
	pglGetIntegerv = x_GetIntegerv;
	pglGetString = x_GetString;
	pglGetTexParameterfv = x_GetTexParameterfv;
	pglHint = x_Hint;
	pglPixelStorei = x_PixelStorei;
	pglPolygonOffset = x_PolygonOffset;
	pglReadPixels = x_ReadPixels;
	pglShadeModel = x_ShadeModel;
	pglTexCoordPointer = x_TexCoordPointer;
	pglTexEnvf = x_TexEnvf;
	pglTexEnvfv = x_TexEnvfv;
	pglTexEnvi = x_TexEnvi;
	pglTexImage2D = x_TexImage2D;
	pglTexParameterf = x_TexParameterf;
	pglTexParameteri = x_TexParameteri;
	pglTexSubImage2D = x_TexSubImage2D;
	pglVertexPointer = x_VertexPointer;
	pglViewport = x_Viewport;
}

static const XsdkVertexElement world_layout[] = {
	{ .usage = XSDK_USAGE_POSITION, .format = XSDK_VERTEX_FLOAT4, .offset_bytes = 0 },
	{ .usage = XSDK_USAGE_COLOR, .format = XSDK_VERTEX_UBYTE4N, .offset_bytes = 24 },
	{ .usage = XSDK_USAGE_TEXCOORD, .format = XSDK_VERTEX_FLOAT2, .offset_bytes = 16 },
};

int xgl_open(void)
{
	if (g.open) return 1;
	NTSTATUS st = xsdk_gpu_open();
	if (!NT_SUCCESS(st)) { avp1_log("xsdk_gpu_open falhou: 0x%08X", (unsigned)st); return 0; }
	st = xsdk_present_open(XSDK_DISPLAY_720P, 3, 1, &g.present_w, &g.present_h);
	if (!NT_SUCCESS(st)) {
		/* Xenia sem tela: desenha numa textura fora da tela. */
		avp1_log("sem display (0x%08X): desenhando fora da tela", (unsigned)st);
		g.present_w = SCREEN_W;
		g.present_h = SCREEN_H;
		st = xsdk_texture_create(&g.offscreen_tex, XSDK_FORMAT_A8R8G8B8, SCREEN_W, SCREEN_H, 1, true);
		if (!NT_SUCCESS(st)) return 0;
		xsdk_texture_base_at_corner(&g.offscreen_tex);
		g.offscreen = true;
	}
	st = xsdk_shader_load(&g.vs, world_vert.container, world_vert.container_bytes);
	if (NT_SUCCESS(st)) st = xsdk_shader_load(&g.ps, world_frag.container, world_frag.container_bytes);
	if (NT_SUCCESS(st))
		st = xsdk_shader_set_vertex_layout(&g.vs, world_layout, 3, (const uint32_t[]){ VERTEX_WORDS * 4 });
	if (!NT_SUCCESS(st)) { avp1_log("shaders falharam: 0x%08X", (unsigned)st); return 0; }

	g.ring = xsdk_gpu_alloc(RING_SEGMENTS * RING_SEGMENT_BYTES);
	if (!g.ring) { avp1_log("sem memoria para o anel de vertices"); return 0; }
	g.ring_address = xsdk_gpu_address(g.ring);

	st = xsdk_texture_create(&g.white, XSDK_FORMAT_A8R8G8B8, 1, 1, 1, false);
	if (!NT_SUCCESS(st)) return 0;
	g.white.texels[0] = 0xffffffffu;
	xsdk_gpu_invalidate(g.white.address, g.white.size_bytes);
	XsdkSampler s = { .mag_filter = XSDK_FILTER_POINT, .min_filter = XSDK_FILTER_POINT, .mip_filter = XSDK_FILTER_POINT,
		.address_u = XSDK_ADDRESS_CLAMP, .address_v = XSDK_ADDRESS_CLAMP };
	xsdk_texture_sampler_state(&g.white_sampler, &s);

	g.depth_tile = xsdk_gpu_target_tiles(SCREEN_W, SCREEN_H);
	xsdk_gpu_set_depth_clear(1.0f, 0);
	g.vp[2] = SCREEN_W;
	g.vp[3] = SCREEN_H;
	g.blend_src = GL_ONE;
	g.blend_dst = GL_ZERO;
	g.depth_func = GL_LESS;
	g.depth_mask = true;
	g.cull_face = GL_BACK;
	g.front_face = GL_CCW;
	g.env_mode = GL_MODULATE;
	g.color[0] = g.color[1] = g.color[2] = g.color[3] = 1.0f;
	g.open = true;
	avp1_log("xenos_gl pronto: %ux%u%s", g.present_w, g.present_h, g.offscreen ? " (fora da tela)" : "");
	return 1;
}

void xgl_screen_size(int *width, int *height)
{
	if (width) *width = SCREEN_W;
	if (height) *height = SCREEN_H;
}

void xgl_swap(void)
{
	if (!g.open) return;
	begin_frame();
	if (!g.in_frame) return;
	xsdk_gpu_resolve(g.frame_address, g.present_w, g.present_h, 0xff000000);
	if (!g.offscreen) xsdk_present_end();
	g.segment_fence[g.segment] = xsdk_gpu_fence();
	if (g.offscreen) xsdk_gpu_wait(g.segment_fence[g.segment], 1000);
	g.in_frame = false;
	if (++g.frames % 600 == 1) avp1_log("quadro %u: %u draws", g.frames, g.draws);
	g.draws = 0;
}
