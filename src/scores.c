/* scores.c — Meilleurs scores et statistiques. */
#include "scores.h"
#include <string.h>

void scores_reset(scores_t *s)
{
    memset(s, 0, sizeof *s);
}

int scores_record(scores_t *s, uint32_t score, int lines, int level)
{
    if (s->played < UINT16_MAX)
        s->played++;
    if (lines > 0)
        s->total_lines += lines;

    /* Une partie sans aucun point n'entre pas au classement. */
    if (score == 0)
        return -1;

    int rank = 0;
    while (rank < SCORES_COUNT && s->best[rank].score >= score)
        rank++;
    if (rank == SCORES_COUNT)
        return -1;

    /* Les scores suivants reculent d'une place ; le dernier sort. */
    memmove(&s->best[rank + 1], &s->best[rank],
        (SCORES_COUNT - 1 - rank) * sizeof *s->best);
    s->best[rank] = (score_entry_t){ score, lines, level };
    return rank;
}
