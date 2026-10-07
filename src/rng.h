/* rng.h — Petit générateur pseudo-aléatoire (xorshift32).
 *
 * On n'utilise pas rand() pour que les grilles soient reproductibles d'une
 * machine à l'autre à partir d'une même graine : les tests et le simulateur
 * en ont besoin. */
#ifndef RNG_H
#define RNG_H

#include <stdint.h>

typedef struct {
    uint32_t state;
} rng_t;

/* Initialise le générateur. Une graine nulle est remplacée par une constante,
 * car xorshift resterait bloqué à zéro. */
void rng_seed(rng_t *rng, uint32_t seed);

/* Renvoie 32 bits pseudo-aléatoires. */
uint32_t rng_next(rng_t *rng);

/* Renvoie un entier uniforme dans [0, bound[ (bound > 0). */
uint32_t rng_below(rng_t *rng, uint32_t bound);

#endif /* RNG_H */
