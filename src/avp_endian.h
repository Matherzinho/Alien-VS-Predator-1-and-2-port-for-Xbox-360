// avp360: Conversao Little-Endian (arquivos de PC/Xbox Classico) <-> Big-Endian (PowerPC Xenon)
// Usado na leitura de arquivos .RIF, .FFL, .BM, .WAV (AvP 1) e .REZ, .DAT, .ABC, .LTA (AvP 2).
#ifndef AVP_ENDIAN_H
#define AVP_ENDIAN_H

#include <stdint.h>
#include <string.h>

static inline uint16_t avp_bswap16(uint16_t x) { return __builtin_bswap16(x); }
static inline uint32_t avp_bswap32(uint32_t x) { return __builtin_bswap32(x); }
static inline uint64_t avp_bswap64(uint64_t x) { return __builtin_bswap64(x); }

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
static inline uint16_t le16(uint16_t x) { return avp_bswap16(x); }
static inline uint32_t le32(uint32_t x) { return avp_bswap32(x); }
static inline uint64_t le64(uint64_t x) { return avp_bswap64(x); }
#else
static inline uint16_t le16(uint16_t x) { return x; }
static inline uint32_t le32(uint32_t x) { return x; }
static inline uint64_t le64(uint64_t x) { return x; }
#endif

// Leitura direta Little-Endian de ponteiro arbitrario (usa lhbrx/lwbrx no PowerPC Xenon)
static inline uint16_t load_le16(const void *p)
{
#if defined(__PPC__) || defined(__powerpc__)
	uint16_t v;
	__asm__("lhbrx %0, 0, %1" : "=r"(v) : "r"(p) : "memory");
	return v;
#else
	uint16_t v;
	memcpy(&v, p, sizeof v);
	return le16(v);
#endif
}

static inline uint32_t load_le32(const void *p)
{
#if defined(__PPC__) || defined(__powerpc__)
	uint32_t v;
	__asm__("lwbrx %0, 0, %1" : "=r"(v) : "r"(p) : "memory");
	return v;
#else
	uint32_t v;
	memcpy(&v, p, sizeof v);
	return le32(v);
#endif
}

static inline float load_lef32(const void *p)
{
	uint32_t u = load_le32(p);
	float f;
	memcpy(&f, &u, sizeof f);
	return f;
}

static inline void swap_le32_array(void *buf, size_t count)
{
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	uint32_t *p = (uint32_t *)buf;
	for (size_t i = 0; i < count; i++) p[i] = avp_bswap32(p[i]);
#else
	(void)buf;
	(void)count;
#endif
}

static inline uint16_t avp_le16_to_cpu(uint16_t x) { return le16(x); }
static inline uint32_t avp_le32_to_cpu(uint32_t x) { return le32(x); }
static inline uint16_t avp_read_le16(const void *p) { return load_le16(p); }
static inline uint32_t avp_read_le32(const void *p) { return load_le32(p); }

#endif // AVP_ENDIAN_H
