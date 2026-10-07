/* rng.c — Générateur xorshift32 (George Marsaglia, 2003). */
#include "rng.h"

void rng_seed(rng_t *rng, uint32_t seed)
{
    /* On mélange la graine pour que deux graines proches donnent des suites
     * très différentes (fonction de hachage de Thomas Wang). */
    seed = (seed ^ 61u) ^ (seed >> 16);
    seed *= 9u;
    seed ^= seed >> 4;
    seed *= 0x27d4eb2du;
    seed ^= seed >> 15;
    rng->state = seed ? seed : 0x2545f491u;
}

uint32_t rng_next(rng_t *rng)
{
    uint32_t x = rng->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng->state = x;
    return x;
}

uint32_t rng_below(rng_t *rng, uint32_t bound)
{
    /* Rejet des valeurs du dernier intervalle incomplet pour éviter le biais
     * du simple modulo. */
    uint32_t limit = UINT32_MAX - (UINT32_MAX % bound);
    uint32_t x;
    do {
        x = rng_next(rng);
    } while (x >= limit);
    return x % bound;
}
