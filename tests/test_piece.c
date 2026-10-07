/* test_piece.c — Formes des pièces et décalages de rotation (piece.c). */
#include "test.h"
#include "piece.h"
#include <stdbool.h>

/* Vrai si la case (x, y) fait partie de [cells]. */
static bool has_cell(cell_t const cells[4], int x, int y)
{
    for (int i = 0; i < 4; i++)
        if (cells[i].x == x && cells[i].y == y)
            return true;
    return false;
}

static void check_shapes(void)
{
    for (int type = 0; type < PIECE_COUNT; type++) {
        int size = (type == PIECE_I) ? 4 : 3;
        for (int rot = 0; rot < ROT_COUNT; rot++) {
            cell_t c[4];
            piece_cells(type, rot, c);
            for (int i = 0; i < 4; i++) {
                /* Cases dans la boîte, toutes différentes. */
                CHECK(c[i].x >= 0 && c[i].x < size);
                CHECK(c[i].y >= 0 && c[i].y < size);
                for (int j = 0; j < i; j++)
                    CHECK(c[i].x != c[j].x || c[i].y != c[j].y);
            }
            /* Quatre quarts de tour ramènent à la forme de départ. */
            cell_t again[4];
            piece_cells(type, rot + 4, again);
            for (int i = 0; i < 4; i++)
                CHECK(has_cell(c, again[i].x, again[i].y));
        }
    }

    /* Quelques orientations connues du SRS. */
    cell_t c[4];
    piece_cells(PIECE_I, 1, c); /* I vertical, 3e colonne de sa boîte */
    for (int y = 0; y < 4; y++)
        CHECK(has_cell(c, 2, y));
    piece_cells(PIECE_I, 2, c); /* I horizontal, 3e ligne */
    for (int x = 0; x < 4; x++)
        CHECK(has_cell(c, x, 2));
    piece_cells(PIECE_T, 0, c); /* T pointe vers le haut */
    CHECK(has_cell(c, 1, 0) && has_cell(c, 0, 1) && has_cell(c, 1, 1)
        && has_cell(c, 2, 1));
    piece_cells(PIECE_T, 1, c); /* T pointe vers la droite */
    CHECK(has_cell(c, 1, 0) && has_cell(c, 1, 1) && has_cell(c, 2, 1)
        && has_cell(c, 1, 2));
    piece_cells(PIECE_J, 1, c);
    CHECK(has_cell(c, 1, 0) && has_cell(c, 2, 0) && has_cell(c, 1, 1)
        && has_cell(c, 1, 2));
    /* O ne bouge pas en tournant. */
    for (int rot = 0; rot < ROT_COUNT; rot++) {
        piece_cells(PIECE_O, rot, c);
        CHECK(has_cell(c, 1, 0) && has_cell(c, 2, 0) && has_cell(c, 1, 1)
            && has_cell(c, 2, 1));
    }
}

static void check_kicks(void)
{
    for (int type = 0; type < PIECE_COUNT; type++) {
        for (int from = 0; from < ROT_COUNT; from++) {
            for (int dir = -1; dir <= 1; dir += 2) {
                cell_t k[KICK_TESTS], back[KICK_TESTS];
                piece_kicks(type, from, dir, k);
                piece_kicks(type, (from + dir) & 3, -dir, back);
                CHECK(k[0].x == 0 && k[0].y == 0);
                /* Propriété des tables SRS : tourner dans l'autre sens
                 * essaie les décalages opposés. */
                for (int i = 0; i < KICK_TESTS; i++)
                    CHECK(k[i].x == -back[i].x && k[i].y == -back[i].y);
            }
        }
    }

    /* Valeurs de référence, y vers le bas (la référence a y vers le
     * haut). */
    cell_t k[KICK_TESTS];
    piece_kicks(PIECE_T, 0, 1, k); /* 0→R : (-1,0) (-1,+1) (0,-2) (-1,-2) */
    CHECK(k[1].x == -1 && k[1].y == 0);
    CHECK(k[2].x == -1 && k[2].y == -1);
    CHECK(k[3].x == 0 && k[3].y == 2);
    CHECK(k[4].x == -1 && k[4].y == 2);
    piece_kicks(PIECE_I, 0, 1, k); /* 0→R : (-2,0) (+1,0) (-2,-1) (+1,+2) */
    CHECK(k[1].x == -2 && k[1].y == 0);
    CHECK(k[2].x == 1 && k[2].y == 0);
    CHECK(k[3].x == -2 && k[3].y == 1);
    CHECK(k[4].x == 1 && k[4].y == -2);
    piece_kicks(PIECE_O, 2, -1, k);
    for (int i = 0; i < KICK_TESTS; i++)
        CHECK(k[i].x == 0 && k[i].y == 0);
}

void test_piece(void)
{
    check_shapes();
    check_kicks();
}
