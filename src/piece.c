/* piece.c — Formes des pièces et décalages de rotation (SRS). */
#include "piece.h"

/* Formes à l'apparition, dans leur boîte :
 *
 *   I  ....   J  #..   L  ..#   O  .##.   S  .##   T  .#.   Z  ##.
 *      ####      ###      ###      .##.      ##.      ###      .##
 *      ....      ...      ...                ...      ...      ...
 *      ....
 *
 * La boîte fait 4×4 cases pour I, 3×3 pour les autres ; O ne tourne pas. */
static cell_t const SHAPES[PIECE_COUNT][4] = {
    [PIECE_I] = { { 0, 1 }, { 1, 1 }, { 2, 1 }, { 3, 1 } },
    [PIECE_J] = { { 0, 0 }, { 0, 1 }, { 1, 1 }, { 2, 1 } },
    [PIECE_L] = { { 2, 0 }, { 0, 1 }, { 1, 1 }, { 2, 1 } },
    [PIECE_O] = { { 1, 0 }, { 2, 0 }, { 1, 1 }, { 2, 1 } },
    [PIECE_S] = { { 1, 0 }, { 2, 0 }, { 0, 1 }, { 1, 1 } },
    [PIECE_T] = { { 1, 0 }, { 0, 1 }, { 1, 1 }, { 2, 1 } },
    [PIECE_Z] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 2, 1 } },
};

void piece_cells(int type, int rot, cell_t out[4])
{
    int size = (type == PIECE_I) ? 4 : 3;
    for (int i = 0; i < 4; i++) {
        int x = SHAPES[type][i].x, y = SHAPES[type][i].y;
        if (type != PIECE_O) {
            /* Quart de tour vers la droite dans la boîte : (x, y) devient
             * (taille - 1 - y, x). */
            for (int r = 0; r < (rot & 3); r++) {
                int nx = size - 1 - y;
                y = x;
                x = nx;
            }
        }
        out[i] = (cell_t){ x, y };
    }
}

/* Tables du SRS, recopiées de la référence (tetris.wiki/Super_Rotation_System)
 * avec sa convention : y vers le HAUT. piece_kicks() inverse y.
 * Indices : [orientation de départ][0 = vers la droite, 1 = vers la gauche]. */
static int8_t const KICKS_JLSTZ[ROT_COUNT][2][KICK_TESTS][2] = {
    { /* 0 */
        { { 0, 0 }, { -1, 0 }, { -1, 1 }, { 0, -2 }, { -1, -2 } }, /* 0→R */
        { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, -2 }, { 1, -2 } },    /* 0→L */
    },
    { /* R */
        { { 0, 0 }, { 1, 0 }, { 1, -1 }, { 0, 2 }, { 1, 2 } },     /* R→2 */
        { { 0, 0 }, { 1, 0 }, { 1, -1 }, { 0, 2 }, { 1, 2 } },     /* R→0 */
    },
    { /* 2 */
        { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, -2 }, { 1, -2 } },    /* 2→L */
        { { 0, 0 }, { -1, 0 }, { -1, 1 }, { 0, -2 }, { -1, -2 } }, /* 2→R */
    },
    { /* L */
        { { 0, 0 }, { -1, 0 }, { -1, -1 }, { 0, 2 }, { -1, 2 } },  /* L→0 */
        { { 0, 0 }, { -1, 0 }, { -1, -1 }, { 0, 2 }, { -1, 2 } },  /* L→2 */
    },
};

static int8_t const KICKS_I[ROT_COUNT][2][KICK_TESTS][2] = {
    { /* 0 */
        { { 0, 0 }, { -2, 0 }, { 1, 0 }, { -2, -1 }, { 1, 2 } },   /* 0→R */
        { { 0, 0 }, { -1, 0 }, { 2, 0 }, { -1, 2 }, { 2, -1 } },   /* 0→L */
    },
    { /* R */
        { { 0, 0 }, { -1, 0 }, { 2, 0 }, { -1, 2 }, { 2, -1 } },   /* R→2 */
        { { 0, 0 }, { 2, 0 }, { -1, 0 }, { 2, 1 }, { -1, -2 } },   /* R→0 */
    },
    { /* 2 */
        { { 0, 0 }, { 2, 0 }, { -1, 0 }, { 2, 1 }, { -1, -2 } },   /* 2→L */
        { { 0, 0 }, { 1, 0 }, { -2, 0 }, { 1, -2 }, { -2, 1 } },   /* 2→R */
    },
    { /* L */
        { { 0, 0 }, { 1, 0 }, { -2, 0 }, { 1, -2 }, { -2, 1 } },   /* L→0 */
        { { 0, 0 }, { -2, 0 }, { 1, 0 }, { -2, -1 }, { 1, 2 } },   /* L→2 */
    },
};

void piece_kicks(int type, int from, int dir, cell_t out[KICK_TESTS])
{
    int8_t const (*table)[2] =
        (type == PIECE_I ? KICKS_I : KICKS_JLSTZ)[from & 3][dir < 0];
    for (int i = 0; i < KICK_TESTS; i++) {
        /* O ne tourne pas : seul l'essai (0, 0) compte. */
        if (type == PIECE_O)
            out[i] = (cell_t){ 0, 0 };
        else
            out[i] = (cell_t){ table[i][0], -table[i][1] };
    }
}
