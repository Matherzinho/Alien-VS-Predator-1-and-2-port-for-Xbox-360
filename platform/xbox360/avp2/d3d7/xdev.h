/* avp360: o dispositivo Xenos por baixo do Direct3D 7 do d3d.ren (d3d7_xenos.cpp -> xdev.c).
 * So tipos do C: d3d7_xenos.cpp inclui o windows.h do shim, xdev.c os cabecalhos do xsdk, e os dois nao convivem. */
#ifndef AVP360_XDEV_H
#define AVP360_XDEV_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Vertice que vai para a GPU: posicao de tela (x, y, z, rhw), difusa e especular (palavras A B G R, como o UBYTE4N
 * le), duas UVs. 10 palavras. */
typedef struct {
	float x, y, z, rhw;
	uint32_t diffuse, specular;
	float u0, v0, u1, v1;
} XDevVertex;

enum { XDEV_FMT_ARGB8, XDEV_FMT_DXT1, XDEV_FMT_DXT3, XDEV_FMT_DXT5 };
enum { XDEV_PRIM_POINTS = 1, XDEV_PRIM_LINES, XDEV_PRIM_LINE_STRIP, XDEV_PRIM_TRIANGLES, XDEV_PRIM_TRIANGLE_STRIP,
	XDEV_PRIM_TRIANGLE_FAN }; /* = D3DPT_* */

typedef struct {
	uint32_t tex;		/* de xdev_tex_create; 0 = branca */
	uint8_t mag_linear, min_linear, mip_linear, mip_on;
	uint8_t clamp_u, clamp_v;
	uint8_t max_aniso;
	float lod_bias;
} XDevSampler;

/* Estados de um desenho, com os numeros do Direct3D 7 (D3DBLEND_*, D3DCMP_*, D3DSTENCILOP_*, D3DCULL_*). */
typedef struct {
	uint8_t blend, depth_test, depth_write, stencil, wireframe, color_write;
	uint8_t src_blend, dst_blend, depth_func, cull;
	uint8_t stencil_func, stencil_fail, stencil_zfail, stencil_pass;
	uint8_t stencil_ref, stencil_mask, stencil_write_mask;
	float depth_bias;
	/* Retangulo de recorte (o viewport do D3D), em pixels. */
	int32_t scissor_x, scissor_y, scissor_w, scissor_h;
	float vs[2][4];		/* tl.vert: tela e nevoa */
	float ps[32][4];	/* tl.frag: estagios, fator, nevoa, teste de alfa */
	XDevSampler samplers[2];
} XDevState;

/* Abre a GPU e a tela (1280x720); 0 se falhar. */
int xdev_open(uint32_t *width, uint32_t *height);
void xdev_log(const char *fmt, ...);
/* Mostra o quadro desenhado e prepara o proximo. */
void xdev_present(void);
/* Desenha count vertices (ou nindices indices de 16 bits sobre eles). */
void xdev_draw(const XDevState *state, uint32_t prim, const XDevVertex *verts, uint32_t count, const uint16_t *indices,
	uint32_t nindices);

/* Texturas: levels niveis de mipmap, cada um carregado com xdev_tex_upload. Liberacao adiada ate a GPU largar. */
uint32_t xdev_tex_create(uint32_t format, uint32_t width, uint32_t height, uint32_t levels);
void xdev_tex_upload(uint32_t tex, uint32_t level, const void *data, uint32_t row_bytes);
void xdev_tex_free(uint32_t tex);

#ifdef __cplusplus
}
#endif
#endif
