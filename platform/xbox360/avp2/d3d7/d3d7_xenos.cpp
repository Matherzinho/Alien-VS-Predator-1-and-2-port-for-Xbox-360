// avp360: o DirectDraw 7 e o Direct3D 7 que o d3d.ren do LithTech 2.2 usa, sobre o Xenos (xdev.c).
//
// Superficies: memoria do sistema no formato que o d3d.ren pediu. Texturas sobem para a GPU (A8R8G8B8, ou DXT como
// estao) na primeira vez que sao usadas depois de mudar. O alvo de render (back buffer / "offscreen") e a eDRAM: o que
// vai para ele (Blt de uma superficie, colorfill) vira um desenho; Lock dele devolve uma copia que nao volta para a tela.
// Desenho: os vertices pre-transformados (D3DFVF_XYZRHW) vao como estao; os do caminho TnL (D3DFVF_XYZ) sao
// transformados aqui pelas matrizes do SetTransform. Estados de render e dos dois estagios de textura viram as
// constantes de tl.vert/tl.frag.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "d3d.h"
#include "xdev.h"

// GUIDs: so precisam ser distintos (o d3d.ren so os compara com os que recebe daqui).
const GUID IID_IDirectDraw4 = { 0x9c59509a, 0x39bd, 0x11d1, { 0x8c, 0x4a, 0x00, 0xc0, 0x4f, 0xd9, 0x30, 0xc5 } };
const GUID IID_IDirectDraw7 = { 0x15e65ec0, 0x3b9c, 0x11d2, { 0xb9, 0x2f, 0x00, 0x60, 0x97, 0x97, 0xea, 0x5b } };
const GUID IID_IDirectDrawGammaControl = { 0x69c11c3e, 0xb46b, 0x11d1, { 0xad, 0x7a, 0x00, 0xc0, 0x4f, 0xc2, 0x9b, 0x4e } };
const GUID IID_IDirect3D7 = { 0xf5049e77, 0x4861, 0x11d2, { 0xa4, 0x07, 0x00, 0xa0, 0xc9, 0x06, 0x29, 0xa8 } };
const GUID IID_IDirect3DHALDevice = { 0x84e63de0, 0x46aa, 0x11cf, { 0x81, 0x6f, 0x00, 0x00, 0xc0, 0x20, 0x15, 0x6e } };
const GUID IID_IDirect3DTnLHalDevice = { 0xf5049e78, 0x4861, 0x11d2, { 0xa4, 0x07, 0x00, 0xa0, 0xc9, 0x06, 0x29, 0xa8 } };
const GUID IID_IDirect3DMMXDevice = { 0x881949a1, 0xd6f3, 0x11d0, { 0x89, 0xab, 0x00, 0xa0, 0xc9, 0x05, 0x41, 0x29 } };
const GUID IID_IDirect3DRefDevice = { 0x50936643, 0x13e9, 0x11d1, { 0x89, 0xaa, 0x00, 0xa0, 0xc9, 0x05, 0x41, 0x29 } };
const GUID IID_IDirect3DRGBDevice = { 0xa4665c60, 0x2673, 0x11cf, { 0xa3, 0x1a, 0x00, 0xaa, 0x00, 0xb9, 0x33, 0x56 } };

static bool same_guid(const GUID &a, const GUID &b) { return memcmp(&a, &b, sizeof a) == 0; }
#ifdef __cplusplus
// REFIID pode ser referencia ou ponteiro conforme o windows.h; esta forma serve aos dois.
static const GUID &as_guid(const GUID &g) { return g; }
static const GUID &as_guid(const GUID *g) { return *g; }
#endif

static uint32_t g_ScreenW = 1280, g_ScreenH = 720;

// ------------------------------------------------------------------------------------------------- formatos

static void pf_rgb(DDPIXELFORMAT *pf, DWORD bits, DWORD a, DWORD r, DWORD g, DWORD b)
{
	memset(pf, 0, sizeof *pf);
	pf->dwSize = sizeof *pf;
	pf->dwFlags = DDPF_RGB | (a ? DDPF_ALPHAPIXELS : 0);
	pf->dwRGBBitCount = bits;
	pf->dwRBitMask = r;
	pf->dwGBitMask = g;
	pf->dwBBitMask = b;
	pf->dwRGBAlphaBitMask = a;
}

static void pf_fourcc(DDPIXELFORMAT *pf, DWORD fourcc)
{
	memset(pf, 0, sizeof *pf);
	pf->dwSize = sizeof *pf;
	pf->dwFlags = DDPF_FOURCC;
	pf->dwFourCC = fourcc;
}

static const DWORD FOURCC_DXT1 = MAKEFOURCC('D', 'X', 'T', '1');
static const DWORD FOURCC_DXT3 = MAKEFOURCC('D', 'X', 'T', '3');
static const DWORD FOURCC_DXT5 = MAKEFOURCC('D', 'X', 'T', '5');

static uint32_t dxt_block_bytes(DWORD fourcc) { return fourcc == FOURCC_DXT1 ? 8 : 16; }

// Bits de um canal de 0..mask para 0..255.
static inline uint32_t expand(uint32_t v, uint32_t mask)
{
	if (!mask)
		return 0;
	uint32_t shift = 0;
	while (!((mask >> shift) & 1))
		shift++;
	uint32_t m = mask >> shift;
	return ((v & mask) >> shift) * 255u / m;
}

// ------------------------------------------------------------------------------------------------- COM minimo

#define COM_REFCOUNT() \
	ULONG m_Ref; \
	ULONG AddRef() { return ++m_Ref; } \
	ULONG Release() { ULONG r = --m_Ref; if (r == 0) delete this; return r; }

// ------------------------------------------------------------------------------------------------- superficies

enum SurfKind { SK_PLAIN, SK_TEXTURE, SK_PRIMARY, SK_TARGET, SK_ZBUFFER };

struct XClipper : IDirectDrawClipper
{
	COM_REFCOUNT()
	XClipper() : m_Ref(1) {}
	HRESULT QueryInterface(REFIID, LPVOID *ppv) { *ppv = NULL; return E_NOINTERFACE; }
	HRESULT SetHWnd(DWORD, HWND) { return DD_OK; }
};

struct XGamma : IDirectDrawGammaControl
{
	COM_REFCOUNT()
	DDGAMMARAMP m_Ramp;
	XGamma() : m_Ref(1) { memset(&m_Ramp, 0, sizeof m_Ramp); }
	HRESULT QueryInterface(REFIID, LPVOID *ppv) { *ppv = NULL; return E_NOINTERFACE; }
	HRESULT GetGammaRamp(DWORD, LPDDGAMMARAMP r) { *r = m_Ramp; return DD_OK; }
	HRESULT SetGammaRamp(DWORD, LPDDGAMMARAMP r) { m_Ramp = *r; return DD_OK; }
};

struct XSurface;
static void draw_surface_to_target(XSurface *src, const RECT *srcRect, const RECT *dstRect, bool colorKey);
static void clear_target(const RECT *rect, DWORD color, bool doColor, bool doDepth, float z, bool doStencil,
	DWORD stencil);
static void present_frame();

struct XSurface : IDirectDrawSurface7
{
	COM_REFCOUNT()
	SurfKind m_Kind;
	DWORD m_Width, m_Height, m_Caps, m_Caps2;
	DDPIXELFORMAT m_PF;
	uint8_t *m_Bits;
	LONG m_Pitch;
	uint32_t m_Bytes;
	XSurface *m_pNextMip;		// cadeia de mipmaps (dona: a superficie de cima)
	XSurface *m_pBack;			// primaria com cadeia de flip
	XSurface *m_pZ;				// profundidade anexada (so marca)
	bool m_bOwnedByParent;
	bool m_bHasKey;
	DDCOLORKEY m_Key;
	DWORD m_Priority, m_LOD;
	// Copia na GPU (texturas e superficies desenhadas no alvo).
	uint32_t m_GpuTex;
	bool m_bDirty;
	XGamma *m_pGamma;

	XSurface() : m_Ref(1), m_Kind(SK_PLAIN), m_Width(0), m_Height(0), m_Caps(0), m_Caps2(0), m_Bits(NULL), m_Pitch(0),
		m_Bytes(0), m_pNextMip(NULL), m_pBack(NULL), m_pZ(NULL), m_bOwnedByParent(false), m_bHasKey(false),
		m_Priority(0), m_LOD(0), m_GpuTex(0), m_bDirty(true), m_pGamma(NULL)
	{
		memset(&m_PF, 0, sizeof m_PF);
		memset(&m_Key, 0, sizeof m_Key);
	}

	~XSurface()
	{
		if (m_pNextMip)
		{
			m_pNextMip->m_bOwnedByParent = false;
			delete m_pNextMip;
		}
		if (m_pBack && m_pBack != this)
			delete m_pBack;
		if (m_GpuTex)
			xdev_tex_free(m_GpuTex);
		if (m_pGamma)
			m_pGamma->Release();
		free(m_Bits);
	}

	bool IsCompressed() const { return (m_PF.dwFlags & DDPF_FOURCC) != 0; }

	bool Allocate()
	{
		if (IsCompressed())
		{
			uint32_t bw = (m_Width + 3) / 4, bh = (m_Height + 3) / 4;
			m_Bytes = bw * bh * dxt_block_bytes(m_PF.dwFourCC);
			m_Pitch = (LONG)m_Bytes;	// DX7: a pitch de uma superficie comprimida e o tamanho dela inteira
		}
		else
		{
			uint32_t bpp = m_PF.dwRGBBitCount ? m_PF.dwRGBBitCount : 32;
			m_Pitch = (LONG)(((m_Width * bpp / 8) + 3) & ~3u);
			m_Bytes = (uint32_t)m_Pitch * m_Height;
		}
		m_Bits = (uint8_t *)calloc(1, m_Bytes ? m_Bytes : 4);
		return m_Bits != NULL;
	}

	// IUnknown
	HRESULT QueryInterface(REFIID riid, LPVOID *ppv)
	{
		if (same_guid(as_guid(riid), IID_IDirectDrawGammaControl))
		{
			if (!m_pGamma)
				m_pGamma = new XGamma;
			m_pGamma->AddRef();
			*ppv = (IDirectDrawGammaControl *)m_pGamma;
			return DD_OK;
		}
		*ppv = NULL;
		return E_NOINTERFACE;
	}

	HRESULT AddAttachedSurface(LPDIRECTDRAWSURFACE7 s) { m_pZ = (XSurface *)s; return DD_OK; }
	HRESULT DeleteAttachedSurface(DWORD, LPDIRECTDRAWSURFACE7 s)
	{
		if (!s || (XSurface *)s == m_pZ)
			m_pZ = NULL;
		return DD_OK;
	}

	HRESULT EnumAttachedSurfaces(LPVOID ctx, LPDDENUMSURFACESCALLBACK7 fn)
	{
		XSurface *list[3] = { m_pBack, m_pNextMip, m_pZ };
		for (int i = 0; i < 3; i++)
		{
			if (!list[i] || list[i] == this)
				continue;
			DDSURFACEDESC2 d;
			memset(&d, 0, sizeof d);
			d.dwSize = sizeof d;
			list[i]->GetSurfaceDesc(&d);
			list[i]->AddRef();
			if (fn(list[i], &d, ctx) == DDENUMRET_CANCEL)
				break;
		}
		return DD_OK;
	}

	HRESULT GetAttachedSurface(LPDDSCAPS2 caps, LPDIRECTDRAWSURFACE7 *out)
	{
		XSurface *s = NULL;
		if (caps->dwCaps & DDSCAPS_MIPMAP)
			s = m_pNextMip;
		else if (caps->dwCaps & (DDSCAPS_BACKBUFFER | DDSCAPS_3DDEVICE))
			s = m_pBack;
		else if (caps->dwCaps & DDSCAPS_ZBUFFER)
			s = m_pZ;
		*out = s;
		if (!s)
			return DDERR_NOTFOUND;
		s->AddRef();
		return DD_OK;
	}

	HRESULT GetCaps(LPDDSCAPS2 caps)
	{
		memset(caps, 0, sizeof *caps);
		caps->dwCaps = m_Caps;
		caps->dwCaps2 = m_Caps2;
		return DD_OK;
	}

	HRESULT GetPixelFormat(LPDDPIXELFORMAT pf) { *pf = m_PF; return DD_OK; }

	HRESULT GetSurfaceDesc(LPDDSURFACEDESC2 d)
	{
		DWORD size = d->dwSize ? d->dwSize : sizeof *d;
		memset(d, 0, sizeof *d);
		d->dwSize = size;
		d->dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT | DDSD_PITCH;
		d->dwWidth = m_Width;
		d->dwHeight = m_Height;
		d->lPitch = m_Pitch;
		d->ddpfPixelFormat = m_PF;
		d->ddsCaps.dwCaps = m_Caps;
		d->ddsCaps.dwCaps2 = m_Caps2;
		if (m_bHasKey)
		{
			d->dwFlags |= DDSD_CKSRCBLT;
			d->ddckCKSrcBlt = m_Key;
		}
		uint32_t mips = 0;
		for (XSurface *s = this; s; s = s->m_pNextMip)
			mips++;
		if (mips > 1)
		{
			d->dwFlags |= DDSD_MIPMAPCOUNT;
			d->dwMipMapCount = mips;
		}
		if (m_pBack && m_pBack != this)
		{
			d->dwFlags |= DDSD_BACKBUFFERCOUNT;
			d->dwBackBufferCount = 1;
		}
		return DD_OK;
	}

	HRESULT IsLost() { return DD_OK; }
	HRESULT Restore() { return DD_OK; }

	HRESULT Lock(LPRECT rect, LPDDSURFACEDESC2 d, DWORD, HANDLE)
	{
		if (!m_Bits)
			return DDERR_CANTLOCKSURFACE;
		if (m_Kind == SK_TARGET)
		{
			static int warned;
			if (!warned++)
				xdev_log("Lock do alvo de render: a copia devolvida nao volta para a tela (aviso unico)");
		}
		GetSurfaceDesc(d);
		d->dwFlags |= DDSD_LPSURFACE;
		uint8_t *p = m_Bits;
		if (rect && !IsCompressed())
			p += rect->top * m_Pitch + rect->left * (m_PF.dwRGBBitCount / 8);
		d->lpSurface = p;
		return DD_OK;
	}

	HRESULT Unlock(LPRECT)
	{
		m_bDirty = true;
		return DD_OK;
	}

	HRESULT SetClipper(LPDIRECTDRAWCLIPPER) { return DD_OK; }

	HRESULT SetColorKey(DWORD flags, LPDDCOLORKEY key)
	{
		if (!(flags & DDCKEY_SRCBLT))
			return DD_OK;
		m_bHasKey = key != NULL;
		if (key)
			m_Key = *key;
		m_bDirty = true;
		return DD_OK;
	}

	HRESULT SetPriority(DWORD p) { m_Priority = p; return DD_OK; }
	HRESULT GetPriority(LPDWORD p) { *p = m_Priority; return DD_OK; }
	HRESULT SetLOD(DWORD lod) { m_LOD = lod; return DD_OK; }
	HRESULT GetLOD(LPDWORD lod) { *lod = m_LOD; return DD_OK; }

	HRESULT Flip(LPDIRECTDRAWSURFACE7, DWORD)
	{
		present_frame();
		return DD_OK;
	}

	bool IsTarget() const { return m_Kind == SK_TARGET || m_Kind == SK_PRIMARY; }

	void Fill(const RECT *r, DWORD color)
	{
		if (!m_Bits || IsCompressed())
			return;
		LONG x0 = r ? r->left : 0, y0 = r ? r->top : 0;
		LONG x1 = r ? r->right : (LONG)m_Width, y1 = r ? r->bottom : (LONG)m_Height;
		if (x0 < 0) x0 = 0;
		if (y0 < 0) y0 = 0;
		if (x1 > (LONG)m_Width) x1 = m_Width;
		if (y1 > (LONG)m_Height) y1 = m_Height;
		uint32_t bpp = m_PF.dwRGBBitCount / 8;
		for (LONG y = y0; y < y1; y++)
		{
			uint8_t *row = m_Bits + y * m_Pitch + x0 * bpp;
			for (LONG x = x0; x < x1; x++, row += bpp)
			{
				if (bpp == 4) *(uint32_t *)row = color;
				else if (bpp == 2) *(uint16_t *)row = (uint16_t)color;
				else *row = (uint8_t)color;
			}
		}
		m_bDirty = true;
	}

	// Copia de memoria para memoria (mesmo formato; esticando por amostragem quando os tamanhos diferem).
	void CopyFrom(XSurface *src, const RECT *sr, const RECT *dr, bool key)
	{
		if (!m_Bits || !src->m_Bits)
			return;
		if (IsCompressed() || src->IsCompressed())
		{
			if (src->m_Bytes == m_Bytes && !sr && !dr)
				memcpy(m_Bits, src->m_Bits, m_Bytes);
			m_bDirty = true;
			return;
		}
		RECT s = sr ? *sr : RECT{ 0, 0, (LONG)src->m_Width, (LONG)src->m_Height };
		RECT d = dr ? *dr : RECT{ 0, 0, (LONG)m_Width, (LONG)m_Height };
		uint32_t bpp = m_PF.dwRGBBitCount / 8, sbpp = src->m_PF.dwRGBBitCount / 8;
		if (bpp != sbpp || bpp == 0)
			return;
		LONG dw = d.right - d.left, dh = d.bottom - d.top, sw = s.right - s.left, sh = s.bottom - s.top;
		if (dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0)
			return;
		for (LONG y = 0; y < dh; y++)
		{
			LONG dy = d.top + y, sy = s.top + y * sh / dh;
			if (dy < 0 || dy >= (LONG)m_Height || sy < 0 || sy >= (LONG)src->m_Height)
				continue;
			for (LONG x = 0; x < dw; x++)
			{
				LONG dx = d.left + x, sx = s.left + x * sw / dw;
				if (dx < 0 || dx >= (LONG)m_Width || sx < 0 || sx >= (LONG)src->m_Width)
					continue;
				const uint8_t *sp = src->m_Bits + sy * src->m_Pitch + sx * bpp;
				uint8_t *dp = m_Bits + dy * m_Pitch + dx * bpp;
				uint32_t v = bpp == 4 ? *(const uint32_t *)sp : bpp == 2 ? *(const uint16_t *)sp : *sp;
				if (key && src->m_bHasKey && v >= src->m_Key.dwColorSpaceLowValue && v <= src->m_Key.dwColorSpaceHighValue)
					continue;
				memcpy(dp, sp, bpp);
			}
		}
		m_bDirty = true;
	}

	HRESULT Blt(LPRECT dst, LPDIRECTDRAWSURFACE7 srcI, LPRECT srcRect, DWORD flags, LPDDBLTFX fx)
	{
		XSurface *src = (XSurface *)srcI;
		if (flags & DDBLT_COLORFILL)
		{
			DWORD c = fx ? fx->dwFillColor : 0;
			if (IsTarget())
				clear_target(dst, c, true, false, 1.0f, false, 0);
			else
				Fill(dst, c);
			return DD_OK;
		}
		if (!src)
			return DDERR_INVALIDPARAMS;
		if (m_Kind == SK_PRIMARY && src->m_Kind == SK_TARGET)
		{
			present_frame();	// janela: o back buffer copiado para a primaria e o fim do quadro
			return DD_OK;
		}
		if (IsTarget())
		{
			if (!src->IsTarget())
				draw_surface_to_target(src, srcRect, dst, (flags & DDBLT_KEYSRC) != 0);
			return DD_OK;
		}
		if (src->IsTarget())
		{
			Fill(dst, 0);	// leitura da tela: nao ha copia da eDRAM na memoria
			return DD_OK;
		}
		CopyFrom(src, srcRect, dst, (flags & DDBLT_KEYSRC) != 0);
		return DD_OK;
	}

	HRESULT BltFast(DWORD x, DWORD y, LPDIRECTDRAWSURFACE7 srcI, LPRECT srcRect, DWORD flags)
	{
		XSurface *src = (XSurface *)srcI;
		if (!src)
			return DDERR_INVALIDPARAMS;
		LONG w = srcRect ? srcRect->right - srcRect->left : (LONG)src->m_Width;
		LONG h = srcRect ? srcRect->bottom - srcRect->top : (LONG)src->m_Height;
		RECT d = { (LONG)x, (LONG)y, (LONG)x + w, (LONG)y + h };
		return Blt(&d, src, srcRect, (flags & DDBLTFAST_SRCCOLORKEY) ? DDBLT_KEYSRC : 0, NULL);
	}

	// Sobe a superficie (e seus mipmaps) para a GPU se mudou. Devolve a textura.
	uint32_t Gpu()
	{
		bool dirty = false;
		for (XSurface *s = this; s; s = s->m_pNextMip)
			dirty |= s->m_bDirty;
		if (!dirty && m_GpuTex)
			return m_GpuTex;
		if (m_GpuTex)
			xdev_tex_free(m_GpuTex);

		uint32_t levels = 0;
		for (XSurface *s = this; s; s = s->m_pNextMip)
			levels++;
		uint32_t format = XDEV_FMT_ARGB8;
		if (IsCompressed())
			format = m_PF.dwFourCC == FOURCC_DXT1 ? XDEV_FMT_DXT1 : m_PF.dwFourCC == FOURCC_DXT3 ? XDEV_FMT_DXT3 : XDEV_FMT_DXT5;
		m_GpuTex = xdev_tex_create(format, m_Width, m_Height, levels);
		if (!m_GpuTex)
			return 0;

		uint32_t level = 0;
		for (XSurface *s = this; s; s = s->m_pNextMip, level++)
		{
			s->m_bDirty = false;
			if (!s->m_Bits)
				continue;
			if (IsCompressed())
			{
				// Os blocos vem do PC em palavras de 16 bits little-endian; o Xenos le DXT em palavras big-endian
				// (o xsdk configura a troca 8-in-16, como para os formatos de 16 bits).
				uint32_t bw = (s->m_Width + 3) / 4;
				uint8_t *swapped = (uint8_t *)malloc(s->m_Bytes);
				if (!swapped)
					continue;
				for (uint32_t b = 0; b + 1 < s->m_Bytes; b += 2)
				{
					swapped[b] = s->m_Bits[b + 1];
					swapped[b + 1] = s->m_Bits[b];
				}
				xdev_tex_upload(m_GpuTex, level, swapped, bw * dxt_block_bytes(m_PF.dwFourCC));
				free(swapped);
				continue;
			}
			uint32_t *argb = (uint32_t *)malloc(s->m_Width * s->m_Height * 4);
			if (!argb)
				continue;
			const uint32_t bpp = s->m_PF.dwRGBBitCount / 8;
			const DWORD am = s->m_PF.dwRGBAlphaBitMask, rm = s->m_PF.dwRBitMask, gm = s->m_PF.dwGBitMask,
				bm = s->m_PF.dwBBitMask;
			const bool hasAlpha = (s->m_PF.dwFlags & DDPF_ALPHAPIXELS) && am;
			for (uint32_t y = 0; y < s->m_Height; y++)
			{
				const uint8_t *row = s->m_Bits + y * s->m_Pitch;
				for (uint32_t x = 0; x < s->m_Width; x++)
				{
					uint32_t v = bpp == 4 ? ((const uint32_t *)row)[x] : bpp == 2 ? ((const uint16_t *)row)[x] : row[x];
					uint32_t a = hasAlpha ? expand(v, am) : 255u;
					if (m_bHasKey && v >= m_Key.dwColorSpaceLowValue && v <= m_Key.dwColorSpaceHighValue)
						a = 0;
					argb[y * s->m_Width + x] = a << 24 | expand(v, rm) << 16 | expand(v, gm) << 8 | expand(v, bm);
				}
			}
			xdev_tex_upload(m_GpuTex, level, argb, s->m_Width * 4);
			free(argb);
		}
		return m_GpuTex;
	}
};

// ------------------------------------------------------------------------------------------------- dispositivo

struct XVertexBuffer : IDirect3DVertexBuffer7
{
	COM_REFCOUNT()
	D3DVERTEXBUFFERDESC m_Desc;
	uint8_t *m_Data;
	uint32_t m_Stride;
	XVertexBuffer() : m_Ref(1), m_Data(NULL), m_Stride(0) { memset(&m_Desc, 0, sizeof m_Desc); }
	~XVertexBuffer() { free(m_Data); }
	HRESULT QueryInterface(REFIID, LPVOID *ppv) { *ppv = NULL; return E_NOINTERFACE; }
	HRESULT Lock(DWORD, LPVOID *data, LPDWORD size)
	{
		*data = m_Data;
		if (size)
			*size = m_Stride * m_Desc.dwNumVertices;
		return D3D_OK;
	}
	HRESULT Unlock() { return D3D_OK; }
	HRESULT GetVertexBufferDesc(LPD3DVERTEXBUFFERDESC d) { *d = m_Desc; return D3D_OK; }
	HRESULT Optimize(IDirect3DDevice7 *, DWORD) { return D3D_OK; }
};

static uint32_t fvf_stride(DWORD fvf)
{
	uint32_t s = (fvf & D3DFVF_XYZRHW) ? 16 : (fvf & D3DFVF_XYZ) ? 12 : 0;
	if (fvf & D3DFVF_NORMAL) s += 12;
	if (fvf & D3DFVF_DIFFUSE) s += 4;
	if (fvf & D3DFVF_SPECULAR) s += 4;
	s += 8 * ((fvf >> D3DFVF_TEXCOUNT_SHIFT) & 0xf);
	return s;
}

static inline float dw_float(DWORD d) { float f; memcpy(&f, &d, 4); return f; }

// D3DCOLOR (0xAARRGGBB) -> palavra A B G R do UBYTE4N.
static inline uint32_t to_abgr(DWORD c) { return (c & 0xff00ff00u) | ((c >> 16) & 0xffu) | ((c & 0xffu) << 16); }

struct XDevice;
static XDevice *g_pDevice;

struct XDevice : IDirect3DDevice7
{
	COM_REFCOUNT()
	DWORD m_RS[D3DRENDERSTATE_COUNT];
	DWORD m_TSS[2][D3DTSS_COUNT];
	XSurface *m_Tex[2];
	XSurface *m_Target;
	D3DMATRIX m_World, m_View, m_Proj;
	D3DVIEWPORT7 m_VP;
	XDevVertex *m_Scratch;
	uint32_t m_ScratchCount;

	XDevice(XSurface *target) : m_Ref(1), m_Target(target), m_Scratch(NULL), m_ScratchCount(0)
	{
		memset(m_RS, 0, sizeof m_RS);
		memset(m_TSS, 0, sizeof m_TSS);
		m_Tex[0] = m_Tex[1] = NULL;
		// Os padroes do Direct3D 7.
		m_RS[D3DRENDERSTATE_ZENABLE] = 1;
		m_RS[D3DRENDERSTATE_ZWRITEENABLE] = 1;
		m_RS[D3DRENDERSTATE_ZFUNC] = D3DCMP_LESSEQUAL;
		m_RS[D3DRENDERSTATE_SRCBLEND] = D3DBLEND_ONE;
		m_RS[D3DRENDERSTATE_DESTBLEND] = D3DBLEND_ZERO;
		m_RS[D3DRENDERSTATE_CULLMODE] = D3DCULL_CCW;
		m_RS[D3DRENDERSTATE_ALPHAFUNC] = D3DCMP_ALWAYS;
		m_RS[D3DRENDERSTATE_FILLMODE] = D3DFILL_SOLID;
		m_RS[D3DRENDERSTATE_STENCILFUNC] = D3DCMP_ALWAYS;
		m_RS[D3DRENDERSTATE_STENCILFAIL] = m_RS[D3DRENDERSTATE_STENCILZFAIL] = m_RS[D3DRENDERSTATE_STENCILPASS] =
			D3DSTENCILOP_KEEP;
		m_RS[D3DRENDERSTATE_STENCILMASK] = m_RS[D3DRENDERSTATE_STENCILWRITEMASK] = 0xffffffffu;
		m_RS[D3DRENDERSTATE_TEXTUREFACTOR] = 0xffffffffu;
		m_RS[D3DRENDERSTATE_FOGEND] = 0x3f800000u;
		m_RS[D3DRENDERSTATE_CLIPPING] = 1;
		for (int s = 0; s < 2; s++)
		{
			m_TSS[s][D3DTSS_COLOROP] = s == 0 ? D3DTOP_MODULATE : D3DTOP_DISABLE;
			m_TSS[s][D3DTSS_ALPHAOP] = s == 0 ? D3DTOP_SELECTARG1 : D3DTOP_DISABLE;
			m_TSS[s][D3DTSS_COLORARG1] = m_TSS[s][D3DTSS_ALPHAARG1] = D3DTA_TEXTURE;
			m_TSS[s][D3DTSS_COLORARG2] = m_TSS[s][D3DTSS_ALPHAARG2] = D3DTA_CURRENT;
			m_TSS[s][D3DTSS_TEXCOORDINDEX] = s;
			m_TSS[s][D3DTSS_ADDRESS] = m_TSS[s][D3DTSS_ADDRESSU] = m_TSS[s][D3DTSS_ADDRESSV] = D3DTADDRESS_WRAP;
			m_TSS[s][D3DTSS_MAGFILTER] = D3DTFG_POINT;
			m_TSS[s][D3DTSS_MINFILTER] = D3DTFN_POINT;
			m_TSS[s][D3DTSS_MIPFILTER] = D3DTFP_NONE;
			m_TSS[s][D3DTSS_MAXANISOTROPY] = 1;
		}
		static const D3DMATRIX id = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
		m_World = m_View = m_Proj = id;
		m_VP.dwX = m_VP.dwY = 0;
		m_VP.dwWidth = g_ScreenW;
		m_VP.dwHeight = g_ScreenH;
		m_VP.dvMinZ = 0.0f;
		m_VP.dvMaxZ = 1.0f;
		g_pDevice = this;
	}

	~XDevice()
	{
		free(m_Scratch);
		if (g_pDevice == this)
			g_pDevice = NULL;
	}

	HRESULT QueryInterface(REFIID, LPVOID *ppv) { *ppv = NULL; return E_NOINTERFACE; }

	HRESULT GetCaps(LPD3DDEVICEDESC7 d)
	{
		memset(d, 0, sizeof *d);
		d->dwDevCaps = D3DDEVCAPS_HWRASTERIZATION | D3DDEVCAPS_TLVERTEXVIDEOMEMORY;
		D3DPRIMCAPS *c = &d->dpcTriCaps;
		c->dwSize = sizeof *c;
		c->dwRasterCaps = D3DPRASTERCAPS_DITHER | D3DPRASTERCAPS_ZTEST | D3DPRASTERCAPS_FOGVERTEX |
			D3DPRASTERCAPS_FOGTABLE | D3DPRASTERCAPS_MIPMAPLODBIAS | D3DPRASTERCAPS_ANISOTROPY;
		c->dwZCmpCaps = c->dwAlphaCmpCaps = 0xff;
		c->dwSrcBlendCaps = c->dwDestBlendCaps = D3DPBLENDCAPS_ALL;
		c->dwShadeCaps = 0xffffffffu;
		c->dwTextureCaps = D3DPTEXTURECAPS_PERSPECTIVE | D3DPTEXTURECAPS_POW2 | D3DPTEXTURECAPS_ALPHA;
		c->dwTextureFilterCaps = D3DPTFILTERCAPS_NEAREST | D3DPTFILTERCAPS_LINEAR | D3DPTFILTERCAPS_MIPNEAREST |
			D3DPTFILTERCAPS_MIPLINEAR | D3DPTFILTERCAPS_LINEARMIPNEAREST | D3DPTFILTERCAPS_LINEARMIPLINEAR;
		c->dwTextureBlendCaps = D3DPTBLENDCAPS_ALL;
		c->dwTextureAddressCaps = 0x1f;
		d->dpcLineCaps = *c;
		d->dwDeviceRenderBitDepth = DDBD_16 | DDBD_32;
		d->dwDeviceZBufferBitDepth = DDBD_16 | DDBD_24 | DDBD_32;
		d->dwMinTextureWidth = d->dwMinTextureHeight = 1;
		d->dwMaxTextureWidth = d->dwMaxTextureHeight = 2048;
		d->dwMaxTextureRepeat = 2048;
		d->dwMaxTextureAspectRatio = 2048;
		d->dwMaxAnisotropy = 16;
		d->dwStencilCaps = 0xff;
		d->dwFVFCaps = 2;
		d->dwTextureOpCaps = 0xffffffffu;
		d->wMaxTextureBlendStages = 2;
		d->wMaxSimultaneousTextures = 2;
		d->dvMaxVertexW = 1e10f;
		d->deviceGUID = IID_IDirect3DHALDevice;
		return D3D_OK;
	}

	HRESULT EnumTextureFormats(LPD3DENUMPIXELFORMATSCALLBACK fn, LPVOID ctx)
	{
		DDPIXELFORMAT pf[8];
		pf_rgb(&pf[0], 32, 0xff000000u, 0xff0000u, 0xff00u, 0xffu);
		pf_rgb(&pf[1], 32, 0, 0xff0000u, 0xff00u, 0xffu);
		pf_rgb(&pf[2], 16, 0, 0xf800u, 0x07e0u, 0x001fu);
		pf_rgb(&pf[3], 16, 0x8000u, 0x7c00u, 0x03e0u, 0x001fu);
		pf_rgb(&pf[4], 16, 0xf000u, 0x0f00u, 0x00f0u, 0x000fu);
		pf_fourcc(&pf[5], FOURCC_DXT1);
		pf_fourcc(&pf[6], FOURCC_DXT3);
		pf_fourcc(&pf[7], FOURCC_DXT5);
		for (int i = 0; i < 8; i++)
			if (fn(&pf[i], ctx) == D3DENUMRET_CANCEL)
				break;
		return D3D_OK;
	}

	HRESULT BeginScene() { return D3D_OK; }
	HRESULT EndScene() { return D3D_OK; }
	HRESULT SetRenderTarget(LPDIRECTDRAWSURFACE7 t, DWORD) { if (t) m_Target = (XSurface *)t; return D3D_OK; }
	HRESULT GetRenderTarget(LPDIRECTDRAWSURFACE7 *t) { *t = m_Target; if (m_Target) m_Target->AddRef(); return D3D_OK; }

	HRESULT Clear(DWORD count, LPD3DRECT rects, DWORD flags, D3DCOLOR color, D3DVALUE z, DWORD stencil)
	{
		bool c = (flags & D3DCLEAR_TARGET) != 0, d = (flags & D3DCLEAR_ZBUFFER) != 0, s = (flags & D3DCLEAR_STENCIL) != 0;
		if (!count || !rects)
		{
			RECT r = { (LONG)m_VP.dwX, (LONG)m_VP.dwY, (LONG)(m_VP.dwX + m_VP.dwWidth), (LONG)(m_VP.dwY + m_VP.dwHeight) };
			clear_target(&r, color, c, d, z, s, stencil);
			return D3D_OK;
		}
		for (DWORD i = 0; i < count; i++)
		{
			RECT r = { rects[i].x1, rects[i].y1, rects[i].x2, rects[i].y2 };
			clear_target(&r, color, c, d, z, s, stencil);
		}
		return D3D_OK;
	}

	HRESULT SetTransform(D3DTRANSFORMSTATETYPE t, LPD3DMATRIX m)
	{
		if (t == D3DTRANSFORMSTATE_WORLD) m_World = *m;
		else if (t == D3DTRANSFORMSTATE_VIEW) m_View = *m;
		else if (t == D3DTRANSFORMSTATE_PROJECTION) m_Proj = *m;
		return D3D_OK;
	}
	HRESULT GetTransform(D3DTRANSFORMSTATETYPE t, LPD3DMATRIX m)
	{
		*m = t == D3DTRANSFORMSTATE_WORLD ? m_World : t == D3DTRANSFORMSTATE_VIEW ? m_View : m_Proj;
		return D3D_OK;
	}
	HRESULT SetViewport(LPD3DVIEWPORT7 vp) { m_VP = *vp; return D3D_OK; }
	HRESULT GetViewport(LPD3DVIEWPORT7 vp) { *vp = m_VP; return D3D_OK; }

	HRESULT SetRenderState(D3DRENDERSTATETYPE t, DWORD v)
	{
		if (t < D3DRENDERSTATE_COUNT)
			m_RS[t] = v;
		return D3D_OK;
	}
	HRESULT GetRenderState(D3DRENDERSTATETYPE t, LPDWORD v)
	{
		*v = t < D3DRENDERSTATE_COUNT ? m_RS[t] : 0;
		return D3D_OK;
	}

	HRESULT GetTexture(DWORD stage, LPDIRECTDRAWSURFACE7 *t)
	{
		*t = stage < 2 ? m_Tex[stage] : NULL;
		if (*t)
			(*t)->AddRef();
		return D3D_OK;
	}
	HRESULT SetTexture(DWORD stage, LPDIRECTDRAWSURFACE7 t)
	{
		if (stage < 2)
			m_Tex[stage] = (XSurface *)t;
		return D3D_OK;
	}
	HRESULT GetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE t, LPDWORD v)
	{
		*v = stage < 2 && t < D3DTSS_COUNT ? m_TSS[stage][t] : 0;
		return D3D_OK;
	}
	HRESULT SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE t, DWORD v)
	{
		if (stage < 2 && t < D3DTSS_COUNT)
		{
			m_TSS[stage][t] = v;
			if (t == D3DTSS_ADDRESS)
				m_TSS[stage][D3DTSS_ADDRESSU] = m_TSS[stage][D3DTSS_ADDRESSV] = v;
		}
		return D3D_OK;
	}
	HRESULT ValidateDevice(LPDWORD passes) { if (passes) *passes = 1; return D3D_OK; }

	HRESULT Load(LPDIRECTDRAWSURFACE7 dst, LPPOINT pt, LPDIRECTDRAWSURFACE7 src, LPRECT rect, DWORD)
	{
		XSurface *d = (XSurface *)dst, *s = (XSurface *)src;
		if (!d || !s)
			return DDERR_INVALIDPARAMS;
		for (; d && s; d = d->m_pNextMip, s = s->m_pNextMip)
		{
			if (pt && rect)
			{
				RECT dr = { pt->x, pt->y, pt->x + (rect->right - rect->left), pt->y + (rect->bottom - rect->top) };
				d->CopyFrom(s, rect, &dr, false);
				break;
			}
			d->CopyFrom(s, rect, rect, false);
			if (rect)
				break;
		}
		return D3D_OK;
	}

	HRESULT GetInfo(DWORD, LPVOID info, DWORD size)
	{
		memset(info, 0, size);
		return D3D_OK;
	}

	// ---- desenho

	XDevVertex *Scratch(uint32_t n)
	{
		if (n > m_ScratchCount)
		{
			free(m_Scratch);
			m_ScratchCount = n + 256;
			m_Scratch = (XDevVertex *)malloc(m_ScratchCount * sizeof(XDevVertex));
		}
		return m_Scratch;
	}

	// Converte vertices do FVF do jogo para os da GPU; false se algum caiu atras da camera (TnL sem recorte).
	bool Convert(DWORD fvf, const void *data, uint32_t count, XDevVertex *out)
	{
		const uint32_t stride = fvf_stride(fvf);
		const uint32_t ntex = (fvf >> D3DFVF_TEXCOUNT_SHIFT) & 0xf;
		const bool tl = (fvf & D3DFVF_XYZRHW) != 0;
		D3DMATRIX wvp;
		if (!tl)
			Multiply3(m_World, m_View, m_Proj, &wvp);
		bool ok = true;
		for (uint32_t i = 0; i < count; i++)
		{
			const float *f = (const float *)((const uint8_t *)data + (size_t)i * stride);
			const DWORD *w = (const DWORD *)f;
			XDevVertex *v = &out[i];
			uint32_t k;
			if (tl)
			{
				v->x = f[0];
				v->y = f[1];
				v->z = f[2];
				v->rhw = f[3];
				k = 4;
			}
			else
			{
				float x = f[0], y = f[1], z = f[2];
				float cx = x * wvp._11 + y * wvp._21 + z * wvp._31 + wvp._41;
				float cy = x * wvp._12 + y * wvp._22 + z * wvp._32 + wvp._42;
				float cz = x * wvp._13 + y * wvp._23 + z * wvp._33 + wvp._43;
				float cw = x * wvp._14 + y * wvp._24 + z * wvp._34 + wvp._44;
				if (cw < 1e-5f)
				{
					ok = false;
					cw = 1e-5f;
				}
				float rhw = 1.0f / cw;
				v->x = m_VP.dwX + (1.0f + cx * rhw) * 0.5f * m_VP.dwWidth;
				v->y = m_VP.dwY + (1.0f - cy * rhw) * 0.5f * m_VP.dwHeight;
				v->z = m_VP.dvMinZ + cz * rhw * (m_VP.dvMaxZ - m_VP.dvMinZ);
				v->rhw = rhw;
				k = 3;
			}
			if (fvf & D3DFVF_NORMAL)
				k += 3;
			v->diffuse = (fvf & D3DFVF_DIFFUSE) ? to_abgr(w[k++]) : 0xffffffffu;
			v->specular = (fvf & D3DFVF_SPECULAR) ? to_abgr(w[k++]) : 0xff000000u;
			v->u0 = ntex > 0 ? f[k] : 0.0f;
			v->v0 = ntex > 0 ? f[k + 1] : 0.0f;
			v->u1 = ntex > 1 ? f[k + 2] : v->u0;
			v->v1 = ntex > 1 ? f[k + 3] : v->v0;
			// O segundo estagio pode ler o primeiro par (TEXCOORDINDEX).
			if ((m_TSS[1][D3DTSS_TEXCOORDINDEX] & 0xffff) == 0)
			{
				v->u1 = v->u0;
				v->v1 = v->v0;
			}
		}
		return ok;
	}

	static void Multiply(const D3DMATRIX &a, const D3DMATRIX &b, D3DMATRIX *o)
	{
		const float *pa = &a._11, *pb = &b._11;
		float *po = &o->_11;
		for (int r = 0; r < 4; r++)
			for (int c = 0; c < 4; c++)
				po[r * 4 + c] = pa[r * 4 + 0] * pb[0 * 4 + c] + pa[r * 4 + 1] * pb[1 * 4 + c] + pa[r * 4 + 2] * pb[2 * 4 + c] +
					pa[r * 4 + 3] * pb[3 * 4 + c];
	}
	static void Multiply3(const D3DMATRIX &a, const D3DMATRIX &b, const D3DMATRIX &c, D3DMATRIX *o)
	{
		D3DMATRIX ab;
		Multiply(a, b, &ab);
		Multiply(ab, c, o);
	}

	void Draw(D3DPRIMITIVETYPE prim, DWORD fvf, const void *verts, uint32_t count, const uint16_t *indices, uint32_t nidx)
	{
		if (!count)
			return;
		XDevVertex *v = Scratch(count);
		if (!v)
			return;
		Convert(fvf, verts, count, v);
		XDevState st;
		BuildState(&st);
		xdev_draw(&st, prim, v, count, indices, nidx);
	}

	HRESULT DrawPrimitive(D3DPRIMITIVETYPE type, DWORD fvf, LPVOID verts, DWORD count, DWORD)
	{
		Draw(type, fvf, verts, count, NULL, 0);
		return D3D_OK;
	}
	HRESULT DrawIndexedPrimitive(D3DPRIMITIVETYPE type, DWORD fvf, LPVOID verts, DWORD count, LPWORD idx, DWORD nidx,
		DWORD)
	{
		Draw(type, fvf, verts, count, (const uint16_t *)idx, nidx);
		return D3D_OK;
	}
	HRESULT DrawPrimitiveVB(D3DPRIMITIVETYPE type, LPDIRECT3DVERTEXBUFFER7 vbI, DWORD start, DWORD count, DWORD)
	{
		XVertexBuffer *vb = (XVertexBuffer *)vbI;
		if (!vb || start + count > vb->m_Desc.dwNumVertices)
			return DDERR_INVALIDPARAMS;
		Draw(type, vb->m_Desc.dwFVF, vb->m_Data + (size_t)start * vb->m_Stride, count, NULL, 0);
		return D3D_OK;
	}
	HRESULT DrawIndexedPrimitiveVB(D3DPRIMITIVETYPE type, LPDIRECT3DVERTEXBUFFER7 vbI, DWORD start, DWORD count,
		LPWORD idx, DWORD nidx, DWORD)
	{
		XVertexBuffer *vb = (XVertexBuffer *)vbI;
		if (!vb || start + count > vb->m_Desc.dwNumVertices)
			return DDERR_INVALIDPARAMS;
		Draw(type, vb->m_Desc.dwFVF, vb->m_Data + (size_t)start * vb->m_Stride, count, (const uint16_t *)idx, nidx);
		return D3D_OK;
	}

	// ---- estados -> constantes dos shaders

	static void ArgWeights(DWORD a, float sel[4], float mods[4])
	{
		sel[0] = sel[1] = sel[2] = sel[3] = 0.0f;
		mods[0] = mods[1] = mods[2] = mods[3] = 0.0f;
		switch (a & D3DTA_SELECTMASK)
		{
		case D3DTA_DIFFUSE: sel[0] = 1.0f; break;
		case D3DTA_CURRENT: sel[1] = 1.0f; break;
		case D3DTA_TEXTURE: sel[2] = 1.0f; break;
		case D3DTA_TFACTOR: sel[3] = 1.0f; break;
		case D3DTA_SPECULAR: mods[0] = 1.0f; break;
		}
		if (a & D3DTA_COMPLEMENT) mods[1] = 1.0f;
		if (a & D3DTA_ALPHAREPLICATE) mods[2] = 1.0f;
	}

	// Uma operacao (cor ou alfa) de um estagio em 7 constantes (ver tl.frag).
	static void OpWeights(DWORD op, DWORD a1, DWORD a2, bool stageOn, float c[7][4])
	{
		memset(c, 0, 7 * 4 * sizeof(float));
		ArgWeights(a1, c[0], c[1]);
		ArgWeights(a2, c[2], c[3]);
		float *w0 = c[4], *w1 = c[5], *f = c[6];
		w1[2] = 1.0f;					// escala
		w1[3] = stageOn ? 1.0f : 0.0f;	// ligado
		switch (op)
		{
		case D3DTOP_SELECTARG1: w0[0] = 1; break;
		case D3DTOP_SELECTARG2: w0[1] = 1; break;
		case D3DTOP_MODULATE: w0[2] = 1; break;
		case D3DTOP_MODULATE2X: w0[2] = 1; w1[2] = 2; break;
		case D3DTOP_MODULATE4X: w0[2] = 1; w1[2] = 4; break;
		case D3DTOP_ADD: w0[0] = w0[1] = 1; break;
		case D3DTOP_ADDSIGNED: w0[0] = w0[1] = 1; w0[3] = -0.5f; break;
		case D3DTOP_ADDSIGNED2X: w0[0] = w0[1] = 1; w0[3] = -0.5f; w1[2] = 2; break;
		case D3DTOP_SUBTRACT: w0[0] = 1; w0[1] = -1; break;
		case D3DTOP_ADDSMOOTH: w0[0] = w0[1] = 1; w0[2] = -1; break;
		case D3DTOP_BLENDDIFFUSEALPHA: w1[0] = 1; f[0] = 1; break;
		case D3DTOP_BLENDTEXTUREALPHA: w1[0] = 1; f[1] = 1; break;
		case D3DTOP_BLENDCURRENTALPHA: w1[0] = 1; f[2] = 1; break;
		case D3DTOP_MODULATEALPHA_ADDCOLOR: w0[0] = 1; w1[1] = 1; break;
		case D3DTOP_DISABLE: w1[3] = 0; break;
		default: w0[2] = 1; break;	// BUMPENVMAP, DOTPRODUCT3...: modula
		}
	}

	void BuildState(XDevState *s)
	{
		memset(s, 0, sizeof *s);
		const DWORD *rs = m_RS;
		s->color_write = 1;
		s->blend = rs[D3DRENDERSTATE_ALPHABLENDENABLE] != 0;
		s->src_blend = (uint8_t)rs[D3DRENDERSTATE_SRCBLEND];
		s->dst_blend = (uint8_t)rs[D3DRENDERSTATE_DESTBLEND];
		s->depth_test = rs[D3DRENDERSTATE_ZENABLE] != 0;
		s->depth_write = rs[D3DRENDERSTATE_ZENABLE] != 0 && rs[D3DRENDERSTATE_ZWRITEENABLE] != 0;
		s->depth_func = (uint8_t)rs[D3DRENDERSTATE_ZFUNC];
		s->cull = (uint8_t)rs[D3DRENDERSTATE_CULLMODE];
		s->wireframe = rs[D3DRENDERSTATE_FILLMODE] == D3DFILL_WIREFRAME;
		s->stencil = rs[D3DRENDERSTATE_STENCILENABLE] != 0;
		s->stencil_func = (uint8_t)rs[D3DRENDERSTATE_STENCILFUNC];
		s->stencil_fail = (uint8_t)rs[D3DRENDERSTATE_STENCILFAIL];
		s->stencil_zfail = (uint8_t)rs[D3DRENDERSTATE_STENCILZFAIL];
		s->stencil_pass = (uint8_t)rs[D3DRENDERSTATE_STENCILPASS];
		s->stencil_ref = (uint8_t)rs[D3DRENDERSTATE_STENCILREF];
		s->stencil_mask = (uint8_t)rs[D3DRENDERSTATE_STENCILMASK];
		s->stencil_write_mask = (uint8_t)rs[D3DRENDERSTATE_STENCILWRITEMASK];
		s->depth_bias = rs[D3DRENDERSTATE_ZBIAS] ? -(float)rs[D3DRENDERSTATE_ZBIAS] / 65536.0f : 0.0f;
		s->scissor_x = (int32_t)m_VP.dwX;
		s->scissor_y = (int32_t)m_VP.dwY;
		s->scissor_w = (int32_t)m_VP.dwWidth;
		s->scissor_h = (int32_t)m_VP.dwHeight;

		// Vertice: pixel -> NDC com o meio pixel do D3D; nevoa.
		s->vs[0][0] = 2.0f / g_ScreenW;
		s->vs[0][1] = -2.0f / g_ScreenH;
		s->vs[0][2] = -1.0f - 1.0f / g_ScreenW;
		s->vs[0][3] = 1.0f + 1.0f / g_ScreenH;
		s->vs[1][0] = dw_float(rs[D3DRENDERSTATE_FOGSTART]);
		s->vs[1][1] = dw_float(rs[D3DRENDERSTATE_FOGEND]);
		if (rs[D3DRENDERSTATE_FOGENABLE])
			s->vs[1][2] = rs[D3DRENDERSTATE_FOGTABLEMODE] != D3DFOG_NONE ? 1.0f : 2.0f;

		// Estagios.
		bool stage1 = m_TSS[0][D3DTSS_COLOROP] != D3DTOP_DISABLE && m_TSS[1][D3DTSS_COLOROP] != D3DTOP_DISABLE;
		for (int st = 0; st < 2; st++)
		{
			bool on = st == 0 ? m_TSS[0][D3DTSS_COLOROP] != D3DTOP_DISABLE : stage1;
			OpWeights(m_TSS[st][D3DTSS_COLOROP], m_TSS[st][D3DTSS_COLORARG1], m_TSS[st][D3DTSS_COLORARG2], on,
				(float(*)[4])s->ps[st * 14]);
			DWORD aop = m_TSS[st][D3DTSS_ALPHAOP];
			OpWeights(aop == D3DTOP_DISABLE ? D3DTOP_SELECTARG2 : aop, m_TSS[st][D3DTSS_ALPHAARG1],
				aop == D3DTOP_DISABLE ? D3DTA_CURRENT : m_TSS[st][D3DTSS_ALPHAARG2], on, (float(*)[4])s->ps[st * 14 + 7]);
		}
		DWORD tf = rs[D3DRENDERSTATE_TEXTUREFACTOR];
		s->ps[28][0] = ((tf >> 16) & 0xff) / 255.0f;
		s->ps[28][1] = ((tf >> 8) & 0xff) / 255.0f;
		s->ps[28][2] = (tf & 0xff) / 255.0f;
		s->ps[28][3] = (tf >> 24) / 255.0f;
		DWORD fc = rs[D3DRENDERSTATE_FOGCOLOR];
		s->ps[29][0] = ((fc >> 16) & 0xff) / 255.0f;
		s->ps[29][1] = ((fc >> 8) & 0xff) / 255.0f;
		s->ps[29][2] = (fc & 0xff) / 255.0f;

		// Teste de alfa: (maior, maior ou igual, sempre, nunca). A chave de cor (COLORKEYENABLE) vira alfa 0 nos texels.
		float ref = (rs[D3DRENDERSTATE_ALPHAREF] & 0xff) / 255.0f;
		float *at = s->ps[30];
		bool keyed = rs[D3DRENDERSTATE_COLORKEYENABLE] && m_Tex[0] && m_Tex[0]->m_bHasKey;
		if (rs[D3DRENDERSTATE_ALPHATESTENABLE])
		{
			switch (rs[D3DRENDERSTATE_ALPHAFUNC])
			{
			case D3DCMP_NEVER: at[3] = 1; break;
			case D3DCMP_GREATER: at[0] = 1; break;
			case D3DCMP_GREATEREQUAL: at[1] = 1; break;
			case D3DCMP_LESS: case D3DCMP_LESSEQUAL: case D3DCMP_EQUAL: case D3DCMP_NOTEQUAL:
				at[2] = 1;	// raros no d3d.ren: deixa passar
				break;
			default: at[2] = 1; break;
			}
		}
		else if (keyed)
		{
			at[0] = 1;
			ref = 0.0f;
		}
		else
		{
			at[2] = 1;
		}
		s->ps[29][3] = ref;
		s->ps[31][0] = rs[D3DRENDERSTATE_SPECULARENABLE] ? 1.0f : 0.0f;

		// Amostradores.
		for (int st = 0; st < 2; st++)
		{
			XDevSampler *sm = &s->samplers[st];
			XSurface *t = m_Tex[st];
			if (!t || (st == 1 && !stage1))
				continue;
			sm->tex = t->Gpu();
			const DWORD *ts = m_TSS[st];
			sm->mag_linear = ts[D3DTSS_MAGFILTER] != D3DTFG_POINT;
			sm->min_linear = ts[D3DTSS_MINFILTER] != D3DTFN_POINT;
			sm->mip_on = ts[D3DTSS_MIPFILTER] != D3DTFP_NONE && t->m_pNextMip;
			sm->mip_linear = ts[D3DTSS_MIPFILTER] == D3DTFP_LINEAR;
			sm->clamp_u = ts[D3DTSS_ADDRESSU] == D3DTADDRESS_CLAMP;
			sm->clamp_v = ts[D3DTSS_ADDRESSV] == D3DTADDRESS_CLAMP;
			sm->max_aniso = (uint8_t)(ts[D3DTSS_MAXANISOTROPY] > 16 ? 16 : ts[D3DTSS_MAXANISOTROPY]);
			sm->lod_bias = dw_float(ts[D3DTSS_MIPMAPLODBIAS]);
		}
	}
};

// Retangulo de cor/profundidade/estencil: um quad com os estados certos, como o Clear do D3D faz na eDRAM.
static void clear_target(const RECT *rect, DWORD color, bool doColor, bool doDepth, float z, bool doStencil, DWORD stencil)
{
	if (!doColor && !doDepth && !doStencil)
		return;
	float x0 = rect ? (float)rect->left : 0.0f, y0 = rect ? (float)rect->top : 0.0f;
	float x1 = rect ? (float)rect->right : (float)g_ScreenW, y1 = rect ? (float)rect->bottom : (float)g_ScreenH;
	XDevVertex v[4];
	const uint32_t c = to_abgr(color | 0xff000000u);
	const float xs[4] = { x0, x1, x1, x0 }, ys[4] = { y0, y0, y1, y1 };
	for (int i = 0; i < 4; i++)
	{
		v[i].x = xs[i] - 0.5f;
		v[i].y = ys[i] - 0.5f;
		v[i].z = z;
		v[i].rhw = 1.0f;
		v[i].diffuse = c;
		v[i].specular = 0xff000000u;
		v[i].u0 = v[i].v0 = v[i].u1 = v[i].v1 = 0.0f;
	}
	XDevState s;
	memset(&s, 0, sizeof s);
	s.color_write = doColor;
	s.depth_test = 0;
	s.depth_write = doDepth;
	s.depth_func = D3DCMP_ALWAYS;
	s.cull = D3DCULL_NONE;
	s.stencil = doStencil;
	s.stencil_func = D3DCMP_ALWAYS;
	s.stencil_fail = s.stencil_zfail = D3DSTENCILOP_KEEP;
	s.stencil_pass = D3DSTENCILOP_REPLACE;
	s.stencil_ref = (uint8_t)stencil;
	s.stencil_mask = s.stencil_write_mask = 0xff;
	s.vs[0][0] = 2.0f / g_ScreenW;
	s.vs[0][1] = -2.0f / g_ScreenH;
	s.vs[0][2] = -1.0f - 1.0f / g_ScreenW;
	s.vs[0][3] = 1.0f + 1.0f / g_ScreenH;
	// Estagio 0: SELECTARG1(DIFUSA) na cor e no alfa; estagio 1 desligado; teste de alfa: sempre.
	XDevice::OpWeights(D3DTOP_SELECTARG1, D3DTA_DIFFUSE, D3DTA_CURRENT, true, (float(*)[4])s.ps[0]);
	XDevice::OpWeights(D3DTOP_SELECTARG1, D3DTA_DIFFUSE, D3DTA_CURRENT, true, (float(*)[4])s.ps[7]);
	XDevice::OpWeights(D3DTOP_DISABLE, D3DTA_TEXTURE, D3DTA_CURRENT, false, (float(*)[4])s.ps[14]);
	XDevice::OpWeights(D3DTOP_DISABLE, D3DTA_TEXTURE, D3DTA_CURRENT, false, (float(*)[4])s.ps[21]);
	s.ps[30][2] = 1.0f;
	xdev_draw(&s, XDEV_PRIM_TRIANGLE_FAN, v, 4, NULL, 0);
}

// Uma superficie de memoria desenhada no alvo (Blt para a tela: HUD, menus, superficies do jogo).
static void draw_surface_to_target(XSurface *src, const RECT *srcRect, const RECT *dstRect, bool colorKey)
{
	uint32_t tex = src->Gpu();
	if (!tex)
		return;
	RECT sr = srcRect ? *srcRect : RECT{ 0, 0, (LONG)src->m_Width, (LONG)src->m_Height };
	RECT dr = dstRect ? *dstRect : RECT{ 0, 0, (LONG)g_ScreenW, (LONG)g_ScreenH };
	float u0 = (float)sr.left / src->m_Width, v0 = (float)sr.top / src->m_Height;
	float u1 = (float)sr.right / src->m_Width, v1 = (float)sr.bottom / src->m_Height;
	const float xs[4] = { (float)dr.left, (float)dr.right, (float)dr.right, (float)dr.left };
	const float ys[4] = { (float)dr.top, (float)dr.top, (float)dr.bottom, (float)dr.bottom };
	const float us[4] = { u0, u1, u1, u0 }, vs[4] = { v0, v0, v1, v1 };
	XDevVertex v[4];
	for (int i = 0; i < 4; i++)
	{
		v[i].x = xs[i] - 0.5f;
		v[i].y = ys[i] - 0.5f;
		v[i].z = 0.0f;
		v[i].rhw = 1.0f;
		v[i].diffuse = 0xffffffffu;
		v[i].specular = 0xff000000u;
		v[i].u0 = v[i].u1 = us[i];
		v[i].v0 = v[i].v1 = vs[i];
	}
	XDevState s;
	memset(&s, 0, sizeof s);
	s.color_write = 1;
	s.cull = D3DCULL_NONE;
	s.depth_func = D3DCMP_ALWAYS;
	s.vs[0][0] = 2.0f / g_ScreenW;
	s.vs[0][1] = -2.0f / g_ScreenH;
	s.vs[0][2] = -1.0f - 1.0f / g_ScreenW;
	s.vs[0][3] = 1.0f + 1.0f / g_ScreenH;
	XDevice::OpWeights(D3DTOP_SELECTARG1, D3DTA_TEXTURE, D3DTA_CURRENT, true, (float(*)[4])s.ps[0]);
	XDevice::OpWeights(D3DTOP_SELECTARG1, D3DTA_TEXTURE, D3DTA_CURRENT, true, (float(*)[4])s.ps[7]);
	XDevice::OpWeights(D3DTOP_DISABLE, D3DTA_TEXTURE, D3DTA_CURRENT, false, (float(*)[4])s.ps[14]);
	XDevice::OpWeights(D3DTOP_DISABLE, D3DTA_TEXTURE, D3DTA_CURRENT, false, (float(*)[4])s.ps[21]);
	if (colorKey && src->m_bHasKey)
		s.ps[30][0] = 1.0f;	// alfa > 0: os texels da chave sobem com alfa 0
	else
		s.ps[30][2] = 1.0f;
	s.samplers[0].tex = tex;
	s.samplers[0].clamp_u = s.samplers[0].clamp_v = 1;
	xdev_draw(&s, XDEV_PRIM_TRIANGLE_FAN, v, 4, NULL, 0);
}

static void present_frame()
{
	xdev_present();
}

// ------------------------------------------------------------------------------------------------- Direct3D 7

struct XD3D : IDirect3D7
{
	COM_REFCOUNT()
	XD3D() : m_Ref(1) {}
	HRESULT QueryInterface(REFIID, LPVOID *ppv) { *ppv = NULL; return E_NOINTERFACE; }

	HRESULT CreateDevice(REFCLSID, LPDIRECTDRAWSURFACE7 target, LPDIRECT3DDEVICE7 *out)
	{
		*out = new XDevice((XSurface *)target);
		return D3D_OK;
	}

	HRESULT CreateVertexBuffer(LPD3DVERTEXBUFFERDESC desc, LPDIRECT3DVERTEXBUFFER7 *out, DWORD)
	{
		XVertexBuffer *vb = new XVertexBuffer;
		vb->m_Desc = *desc;
		vb->m_Stride = fvf_stride(desc->dwFVF);
		vb->m_Data = (uint8_t *)calloc(desc->dwNumVertices ? desc->dwNumVertices : 1, vb->m_Stride ? vb->m_Stride : 4);
		if (!vb->m_Data)
		{
			delete vb;
			*out = NULL;
			return DDERR_OUTOFMEMORY;
		}
		*out = vb;
		return D3D_OK;
	}

	HRESULT EnumZBufferFormats(REFCLSID, LPD3DENUMPIXELFORMATSCALLBACK fn, LPVOID ctx)
	{
		DDPIXELFORMAT pf;
		memset(&pf, 0, sizeof pf);
		pf.dwSize = sizeof pf;
		pf.dwFlags = DDPF_ZBUFFER | DDPF_STENCILBUFFER;
		pf.dwZBufferBitDepth = 32;
		pf.dwStencilBitDepth = 8;
		pf.dwZBitMask = 0xffffff00u;
		pf.dwStencilBitMask = 0xffu;
		if (fn(&pf, ctx) == D3DENUMRET_CANCEL)
			return D3D_OK;
		pf.dwFlags = DDPF_ZBUFFER;
		pf.dwZBufferBitDepth = 16;
		pf.dwStencilBitDepth = 0;
		pf.dwZBitMask = 0xffffu;
		pf.dwStencilBitMask = 0;
		fn(&pf, ctx);
		return D3D_OK;
	}
};

// ------------------------------------------------------------------------------------------------- DirectDraw

struct XDDraw : IDirectDraw, IDirectDraw4, IDirectDraw7
{
	COM_REFCOUNT()
	DWORD m_Bpp;
	XDDraw() : m_Ref(1), m_Bpp(32) {}

	HRESULT QueryInterface(REFIID riid, LPVOID *ppv)
	{
		const GUID &g = as_guid(riid);
		if (same_guid(g, IID_IDirectDraw7))
			*ppv = (IDirectDraw7 *)this;
		else if (same_guid(g, IID_IDirectDraw4))
			*ppv = (IDirectDraw4 *)this;
		else if (same_guid(g, IID_IDirect3D7))
		{
			*ppv = (IDirect3D7 *)new XD3D;
			return DD_OK;
		}
		else
		{
			*ppv = NULL;
			return E_NOINTERFACE;
		}
		AddRef();
		return DD_OK;
	}

	void ScreenFormat(DDPIXELFORMAT *pf)
	{
		if (m_Bpp == 16)
			pf_rgb(pf, 16, 0, 0xf800u, 0x07e0u, 0x001fu);
		else
			pf_rgb(pf, 32, 0, 0xff0000u, 0xff00u, 0xffu);
	}

	HRESULT CreateClipper(DWORD, LPDIRECTDRAWCLIPPER *out, void *) { *out = new XClipper; return DD_OK; }

	HRESULT CreateSurface(LPDDSURFACEDESC2 d, LPDIRECTDRAWSURFACE7 *out, void *)
	{
		*out = NULL;
		DWORD caps = (d->dwFlags & DDSD_CAPS) ? d->ddsCaps.dwCaps : 0;
		XSurface *s = new XSurface;
		s->m_Caps = caps;
		s->m_Caps2 = (d->dwFlags & DDSD_CAPS) ? d->ddsCaps.dwCaps2 : 0;
		s->m_Width = (d->dwFlags & DDSD_WIDTH) ? d->dwWidth : g_ScreenW;
		s->m_Height = (d->dwFlags & DDSD_HEIGHT) ? d->dwHeight : g_ScreenH;
		if (d->dwFlags & DDSD_PIXELFORMAT)
			s->m_PF = d->ddpfPixelFormat;
		else
			ScreenFormat(&s->m_PF);
		if (d->dwFlags & DDSD_CKSRCBLT)
		{
			s->m_bHasKey = true;
			s->m_Key = d->ddckCKSrcBlt;
		}

		if (caps & DDSCAPS_ZBUFFER)
		{
			s->m_Kind = SK_ZBUFFER;
			*out = s;	// a profundidade de verdade e a da eDRAM
			return DD_OK;
		}
		if (caps & DDSCAPS_PRIMARYSURFACE)
		{
			s->m_Kind = SK_PRIMARY;
			s->m_Width = g_ScreenW;
			s->m_Height = g_ScreenH;
			ScreenFormat(&s->m_PF);
			if (!s->Allocate())
				goto oom;
			if (caps & DDSCAPS_FLIP)
			{
				XSurface *b = new XSurface;
				b->m_Kind = SK_TARGET;
				b->m_Caps = DDSCAPS_BACKBUFFER | DDSCAPS_3DDEVICE | DDSCAPS_VIDEOMEMORY;
				b->m_Width = g_ScreenW;
				b->m_Height = g_ScreenH;
				b->m_PF = s->m_PF;
				b->m_bOwnedByParent = true;
				if (!b->Allocate())
				{
					delete b;
					goto oom;
				}
				s->m_pBack = b;
			}
			*out = s;
			return DD_OK;
		}
		if ((caps & DDSCAPS_3DDEVICE) && !(caps & DDSCAPS_TEXTURE))
		{
			s->m_Kind = SK_TARGET;	// back buffer de janela (o "offscreen" do d3d.ren)
			s->m_Width = g_ScreenW;
			s->m_Height = g_ScreenH;
			ScreenFormat(&s->m_PF);
		}
		else if (caps & DDSCAPS_TEXTURE)
		{
			s->m_Kind = SK_TEXTURE;
		}
		if (!s->Allocate())
			goto oom;

		if ((caps & DDSCAPS_MIPMAP) && (d->dwFlags & DDSD_MIPMAPCOUNT) && d->dwMipMapCount > 1)
		{
			XSurface *prev = s;
			DWORD w = s->m_Width, h = s->m_Height;
			for (DWORD i = 1; i < d->dwMipMapCount; i++)
			{
				w = w > 1 ? w / 2 : 1;
				h = h > 1 ? h / 2 : 1;
				XSurface *m = new XSurface;
				m->m_Kind = SK_TEXTURE;
				m->m_Caps = caps & ~DDSCAPS_COMPLEX;
				m->m_Caps2 = s->m_Caps2;
				m->m_Width = w;
				m->m_Height = h;
				m->m_PF = s->m_PF;
				m->m_bOwnedByParent = true;
				m->m_bHasKey = s->m_bHasKey;
				m->m_Key = s->m_Key;
				if (!m->Allocate())
				{
					delete m;
					break;
				}
				prev->m_pNextMip = m;
				prev = m;
			}
		}
		*out = s;
		return DD_OK;
	oom:
		delete s;
		return DDERR_OUTOFMEMORY;
	}

	HRESULT EnumDisplayModes(DWORD, LPDDSURFACEDESC2, LPVOID ctx, LPDDENUMMODESCALLBACK2 fn)
	{
		// O Xbox apresenta em 1280x720; o d3d.ren desenha nessa resolucao.
		static const DWORD bpps[2] = { 32, 16 };
		for (int i = 0; i < 2; i++)
		{
			DDSURFACEDESC2 d;
			memset(&d, 0, sizeof d);
			d.dwSize = sizeof d;
			d.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
			d.dwWidth = g_ScreenW;
			d.dwHeight = g_ScreenH;
			if (bpps[i] == 16)
				pf_rgb(&d.ddpfPixelFormat, 16, 0, 0xf800u, 0x07e0u, 0x001fu);
			else
				pf_rgb(&d.ddpfPixelFormat, 32, 0, 0xff0000u, 0xff00u, 0xffu);
			if (fn(&d, ctx) == DDENUMRET_CANCEL)
				break;
		}
		return DD_OK;
	}

	HRESULT GetCaps(LPDDCAPS hal, LPDDCAPS hel)
	{
		DDCAPS *list[2] = { hal, hel };
		for (int i = 0; i < 2; i++)
		{
			if (!list[i])
				continue;
			DWORD size = list[i]->dwSize ? list[i]->dwSize : sizeof(DDCAPS);
			memset(list[i], 0, sizeof(DDCAPS));
			list[i]->dwSize = size;
			list[i]->dwCaps = i == 0 ? DDCAPS_3D : 0;
			list[i]->dwZBufferBitDepths = DDBD_16 | DDBD_24 | DDBD_32;
			list[i]->dwVidMemTotal = list[i]->dwVidMemFree = 256u << 20;
			list[i]->ddsCaps.dwCaps = DDSCAPS_3DDEVICE | DDSCAPS_TEXTURE | DDSCAPS_ZBUFFER | DDSCAPS_MIPMAP |
				DDSCAPS_FLIP | DDSCAPS_PRIMARYSURFACE | DDSCAPS_VIDEOMEMORY;
		}
		return DD_OK;
	}

	HRESULT GetAvailableVidMem(LPDDSCAPS2, LPDWORD total, LPDWORD free)
	{
		if (total) *total = 256u << 20;
		if (free) *free = 200u << 20;
		return DD_OK;
	}

	HRESULT GetDeviceIdentifier(LPDDDEVICEIDENTIFIER2 id, DWORD)
	{
		memset(id, 0, sizeof *id);
		strcpy(id->szDriver, "xenos");
		strcpy(id->szDescription, "Xbox 360 Xenos (avp360)");
		id->dwVendorId = 0x1414;
		id->dwDeviceId = 0x360;
		return DD_OK;
	}

	HRESULT RestoreDisplayMode() { return DD_OK; }
	HRESULT SetCooperativeLevel(HWND, DWORD) { return DD_OK; }
	HRESULT SetDisplayMode(DWORD, DWORD, DWORD bpp, DWORD, DWORD)
	{
		m_Bpp = bpp == 16 ? 16 : 32;
		return DD_OK;
	}
};

static bool open_device()
{
	static bool opened, ok;
	if (!opened)
	{
		opened = true;
		ok = xdev_open(&g_ScreenW, &g_ScreenH) != 0;
	}
	return ok;
}

HRESULT WINAPI DirectDrawCreate(GUID *, LPDIRECTDRAW *out, void *)
{
	XDDraw *dd = new XDDraw;
	*out = (IDirectDraw *)dd;
	return DD_OK;
}

HRESULT WINAPI DirectDrawCreateEx(GUID *, LPVOID *out, REFIID, void *)
{
	if (!open_device())
	{
		*out = NULL;
		return DDERR_NODIRECTDRAWHW;
	}
	XDDraw *dd = new XDDraw;
	*out = (IDirectDraw7 *)dd;
	return DD_OK;
}

HRESULT WINAPI DirectDrawEnumerateA(LPDDENUMCALLBACKA fn, LPVOID ctx)
{
	fn(NULL, (LPSTR) "Xbox 360 Xenos", (LPSTR) "display", ctx);
	return DD_OK;
}

HRESULT WINAPI DirectDrawEnumerateExA(LPDDENUMCALLBACKEXA fn, LPVOID ctx, DWORD)
{
	fn(NULL, (LPSTR) "Xbox 360 Xenos", (LPSTR) "display", ctx, NULL);
	return DD_OK;
}
