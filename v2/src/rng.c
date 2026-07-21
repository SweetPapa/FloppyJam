#include "rng.h"
#include <string.h>

static uint64_t splitmix64(uint64_t *x)
{
    uint64_t z = (*x += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

void rng_seed(Rng *r, uint64_t seed)
{
    uint64_t x = seed + 0xA0761D6478BD642FULL;
    r->s = splitmix64(&x);
    if (r->s == 0) r->s = 0x853C49E6748FEA9BULL;
}

Rng rng_stream(uint32_t seed_code, uint32_t stream)
{
    Rng r;
    rng_seed(&r, ((uint64_t)seed_code << 20) ^ (0x9E3779B97F4A7C15ULL * (stream + 1)));
    return r;
}

uint64_t rng_u64(Rng *r)
{
    /* xorshift64* — tiny, fast, good enough for a party game */
    uint64_t x = r->s;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    r->s = x;
    return x * 0x2545F4914F6CDD1DULL;
}

uint32_t rng_u32(Rng *r) { return (uint32_t)(rng_u64(r) >> 32); }

float rng_f01(Rng *r) { return (float)(rng_u32(r) >> 8) / 16777216.0f; }

int rng_range(Rng *r, int lo, int hi)
{
    if (hi <= lo) return lo;
    uint32_t span = (uint32_t)(hi - lo + 1);
    return lo + (int)(rng_u32(r) % span);
}

void rng_shuffle(Rng *r, int *arr, int n)
{
    for (int i = n - 1; i > 0; i--) {
        int j = rng_range(r, 0, i);
        int t = arr[i]; arr[i] = arr[j]; arr[j] = t;
    }
}

uint32_t seed_hash32(uint64_t v)
{
    uint64_t x = v;
    return (uint32_t)(splitmix64(&x) % SEED_CODE_MAX);
}

void seed_to_string(uint32_t code, char out[SEED_LEN + 1])
{
    static const char *A = SEED_ALPHABET;
    code %= SEED_CODE_MAX;
    for (int i = SEED_LEN - 1; i >= 0; i--) {
        out[i] = A[code % SEED_ALPHABET_N];
        code /= SEED_ALPHABET_N;
    }
    out[SEED_LEN] = 0;
}

uint32_t seed_from_string(const char *s)
{
    static const char *A = SEED_ALPHABET;
    uint32_t code = 0;
    for (int i = 0; i < SEED_LEN; i++) {
        char c = s[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        const char *p = strchr(A, c);
        int idx = p ? (int)(p - A) : 0;
        code = code * SEED_ALPHABET_N + (uint32_t)idx;
    }
    return code % SEED_CODE_MAX;
}

uint32_t seed_from_date(int year, int month, int day)
{
    uint64_t v = (uint64_t)year * 10000u + (uint64_t)month * 100u + (uint64_t)day;
    return seed_hash32(v ^ 0x6A09E667F3BCC908ULL);
}
