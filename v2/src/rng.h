/* Deterministic RNG. One seed, split into named streams so cosmetic draws
 * never perturb gameplay determinism (§11.2). */
#ifndef G_RNG_H
#define G_RNG_H

#include <stdint.h>

typedef struct { uint64_t s; } Rng;

/* named streams */
enum {
    RNG_STREAM_MUSIC = 1,
    RNG_STREAM_SCHEDULER,
    RNG_STREAM_CARDS,
    RNG_STREAM_COSMETIC,
    RNG_STREAM_ATTRACT
};

#define SEED_ALPHABET "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
#define SEED_ALPHABET_N 36
#define SEED_LEN 6
#define SEED_CODE_MAX 2176782336u /* 36^6 */

void     rng_seed(Rng *r, uint64_t seed);
Rng      rng_stream(uint32_t seed_code, uint32_t stream);
uint32_t rng_u32(Rng *r);
uint64_t rng_u64(Rng *r);
float    rng_f01(Rng *r);
/* inclusive range */
int      rng_range(Rng *r, int lo, int hi);
/* Fisher-Yates over an int array */
void     rng_shuffle(Rng *r, int *arr, int n);

/* seed code <-> 6-char string */
void     seed_to_string(uint32_t code, char out[SEED_LEN + 1]);
uint32_t seed_from_string(const char *s);
uint32_t seed_from_date(int year, int month, int day);
uint32_t seed_hash32(uint64_t v);

#endif
