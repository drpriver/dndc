#ifndef MURMUR_HASH_H
#define MURMUR_HASH_H
#include <stdint.h>
#include <stddef.h>

#ifndef nosan
#if defined(__clang__)
#define nosan \
    __attribute__((no_sanitize("address"))) \
    __attribute__((no_sanitize("nullability"))) \
    __attribute__((no_sanitize("undefined")))
#elif defined(__GNUC__)
#define nosan \
    __attribute__((no_sanitize("address"))) \
    __attribute__((no_sanitize("undefined")))
#else
#define nosan
#endif

#ifndef force_inline
#if defined(__GNUC__) || defined(__clang__)
#define force_inline __attribute__((always_inline))
#else
#define force_inline
#endif
#endif

#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
// cut'n'paste from the wikipedia page on murmur hash
static inline
force_inline
//nosan
uint32_t
murmur_32_scramble(uint32_t k) {
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    return k;
}

static inline
force_inline
//nosan
uint32_t
murmur3_32(const uint8_t* key, size_t len, uint32_t seed){
	uint32_t h = seed;
    uint32_t k;
    /* Read in groups of 4. */
    for (size_t i = len >> 2; i; i--) {
        memcpy(&k, key, sizeof(uint32_t));
        key += sizeof(uint32_t);
        h ^= murmur_32_scramble(k);
        h = (h << 13) | (h >> 19);
        h = h * 5 + 0xe6546b64;
    }
    /* Read the rest. */
    k = 0;
    for (size_t i = len & 3; i; i--) {
        k <<= 8;
        k |= key[i - 1];
    }
    h ^= murmur_32_scramble(k);
    /* Finalize. */
	h ^= len;
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
}
#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
