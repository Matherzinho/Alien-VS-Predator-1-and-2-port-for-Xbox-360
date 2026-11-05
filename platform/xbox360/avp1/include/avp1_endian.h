/* avp360: leitura little-endian dos arquivos do AvP no Xbox 360 (big-endian).
 * AVP_LE(tipo, palavra, ponteiro) devolve um `tipo` lido de `ponteiro`, com cada palavra de `palavra` bytes (2, 4 ou 8)
 * invertida. A copia para uma variavel local tambem evita leituras desalinhadas de double, que o Xenon nao aceita.
 * tools/endian_reads.py escreve essas chamadas no lugar de `*(tipo *)ponteiro`. */
#ifndef AVP1_ENDIAN_H
#define AVP1_ENDIAN_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static inline void avp_le_swap(void *v, size_t size, int word)
{
	uint8_t *b = (uint8_t *)v;
	if (word == 2) {
		for (size_t i = 0; i + 1 < size; i += 2) { uint8_t t = b[i]; b[i] = b[i + 1]; b[i + 1] = t; }
	} else if (word == 4) {
		for (size_t i = 0; i + 3 < size; i += 4) {
			uint32_t w;
			memcpy(&w, b + i, 4);
			w = __builtin_bswap32(w);
			memcpy(b + i, &w, 4);
		}
	} else if (word == 8) {
		for (size_t i = 0; i + 7 < size; i += 8) {
			uint64_t w;
			memcpy(&w, b + i, 8);
			w = __builtin_bswap64(w);
			memcpy(b + i, &w, 8);
		}
	}
}

#define AVP_LE(type, word, ptr) (__extension__({ \
	type avp_le_v_; \
	memcpy((void *)&avp_le_v_, (const void *)(ptr), sizeof(type)); \
	avp_le_swap((void *)&avp_le_v_, sizeof(type), (word)); \
	avp_le_v_; }))

static inline uint32_t avp_le32(const void *p) { uint32_t v; memcpy(&v, p, 4); return __builtin_bswap32(v); }
static inline uint16_t avp_le16(const void *p) { uint16_t v; memcpy(&v, p, 2); return __builtin_bswap16(v); }

/* Troca palavras de 4 bytes de um bloco ja copiado (arrays de int/float vindos de arquivo). */
static inline void avp_le_words(void *p, size_t bytes) { avp_le_swap(p, bytes, 4); }

/* Campos de bits: no x86 o primeiro campo fica no bit 0; no GCC big-endian, no bit 31. Depois de trocar os bytes da
 * palavra, inverter os 32 bits poe cada campo onde o compilador do Xbox o procura. */
static inline uint32_t avp_bitfield_word(uint32_t v)
{
	v = ((v >> 1) & 0x55555555u) | ((v & 0x55555555u) << 1);
	v = ((v >> 2) & 0x33333333u) | ((v & 0x33333333u) << 2);
	v = ((v >> 4) & 0x0F0F0F0Fu) | ((v & 0x0F0F0F0Fu) << 4);
	return __builtin_bswap32(v);
}

/* Texturas de 32 bits do awtexld: o OpenGL quer os bytes R, G, B, A na memoria. No x86 isso e R no bit 0; no
 * PowerPC, R no bit 24. */
#define AVP_TEX_RED_SHIFT 24
#define AVP_TEX_GREEN_SHIFT 16
#define AVP_TEX_BLUE_SHIFT 8
#define AVP_TEX_ALPHA_MASK 0x000000FFu

#ifdef __cplusplus
#include <type_traits>
/* CHUNK_EXTRACT(var, tipo): largura da palavra pelo tipo (char: 0, short: 2, double e int64: 8, resto e structs:
 * 4; ChunkVector, de doubles, tem especializacao abaixo). */
template <typename T> static inline int avp_le_word_of()
{
	return sizeof(T) == 1 ? 0 : sizeof(T) == 2 ? 2 : (std::is_arithmetic<T>::value && sizeof(T) == 8) ? 8 : 4;
}
template <typename T> static inline T avp_le_get(const void *p)
{
	T v;
	memcpy((void *)&v, p, sizeof(T));
	avp_le_swap((void *)&v, sizeof(T), avp_le_word_of<T>());
	return v;
}
#endif

#endif
