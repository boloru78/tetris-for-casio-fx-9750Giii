/* piece.h — Les 7 pièces (tétrominos), leurs rotations et les décalages
 * essayés quand une rotation est bloquée.
 *
 * On suit le « Super Rotation System » (SRS) des Tetris récents : mêmes
 * formes, mêmes positions d'apparition et mêmes décalages, pour que le jeu
 * réagisse comme les joueurs s'y attendent. Les coordonnées sont en cases,
 * x vers la droite et y vers le bas. */
#ifndef PIECE_H
#define PIECE_H

#include <stdint.h>

typedef enum {
    PIECE_I,
    PIECE_J,
    PIECE_L,
    PIECE_O,
    PIECE_S,
    PIECE_T,
    PIECE_Z,
    PIECE_COUNT,
    PIECE_NONE = 0xff, /* réserve vide */
} piece_type_t;

/* Orientations : 0 à l'apparition, puis quarts de tour vers la droite. */
#define ROT_COUNT 4

/* Une case, relative au coin haut-gauche de la boîte de la pièce. */
typedef struct {
    int8_t x, y;
} cell_t;

/* Les 4 cases de la pièce [type] dans l'orientation [rot]. */
void piece_cells(int type, int rot, cell_t out[4]);

/* Décalages à essayer, dans l'ordre, pour faire tourner la pièce [type] de
 * l'orientation [from] vers [from] + 1 (dir = 1) ou [from] - 1 (dir = -1).
 * Le premier essai est toujours (0, 0). */
#define KICK_TESTS 5
void piece_kicks(int type, int from, int dir, cell_t out[KICK_TESTS]);

#endif /* PIECE_H */
