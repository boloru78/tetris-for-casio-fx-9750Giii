/* scores.h — Meilleurs scores et statistiques. */
#ifndef SCORES_H
#define SCORES_H

#include <stdint.h>

/* Nombre de meilleurs scores conservés. */
#define SCORES_COUNT 5

typedef struct {
    uint32_t score; /* 0 : place libre */
    uint16_t lines;
    uint8_t level;  /* niveau atteint */
} score_entry_t;

typedef struct {
    score_entry_t best[SCORES_COUNT]; /* du meilleur au moins bon */
    uint16_t played;                  /* parties terminées */
    uint32_t total_lines;             /* lignes effacées en tout */
} scores_t;

/* Efface tous les scores et statistiques. */
void scores_reset(scores_t *s);

/* Enregistre une partie terminée. Renvoie sa place dans le classement
 * (0 = meilleur score), ou -1 si elle n'y entre pas. À score égal, la
 * partie la plus ancienne reste devant. */
int scores_record(scores_t *s, uint32_t score, int lines, int level);

#endif /* SCORES_H */
