/* tetris.h — Logique d'une partie : plateau, pièce qui tombe, réserve,
 * pièces suivantes, lignes et score.
 *
 * Ce module ne fait que de la logique, sans clavier ni écran : la partie
 * avance d'une image (1/60 s) à chaque appel de tetris_step(), avec l'état
 * des boutons du jeu. La boucle interactive se trouve dans play.c. Tout
 * est déterministe : avec la même graine et les mêmes boutons, une partie se
 * déroule toujours de la même façon (tests, simulateur). */
#ifndef TETRIS_H
#define TETRIS_H

#include <stdbool.h>
#include <stdint.h>
#include "piece.h"
#include "rng.h"

/* Plateau : 10 colonnes, 20 lignes visibles et 2 lignes cachées au-dessus,
 * où apparaissent les pièces. */
#define BOARD_W 10
#define BOARD_H 22
#define BOARD_HIDDEN 2

/* Nombre de pièces suivantes affichées. */
#define NEXT_COUNT 3

/* Niveaux : la vitesse augmente toutes les 10 lignes, jusqu'au niveau 15. */
#define LEVEL_MAX 15
#define LINES_PER_LEVEL 10

/* Réglages de la prise en main, en images de 1/60 s. */
#define DAS_FRAMES 12        /* délai avant la répétition des déplacements */
#define ARR_FRAMES 3         /* intervalle de répétition */
#define LOCK_FRAMES 30       /* temps au sol avant que la pièce se fige */
#define LOCK_RESETS_MAX 15   /* déplacements au sol qui relancent ce temps */
#define CLEAR_FRAMES 24      /* clignotement des lignes complètes */
#define SOFT_DROP_GRAVITY (65536 / 2) /* chute douce : une ligne / 2 images */

/* La partie s'arrête à 99 h, bien au-delà de ce qu'un joueur tiendra. */
#define TETRIS_MAX_FRAMES (99u * 3600 * 60)

/* Boutons du jeu ; play.c les associe aux touches de la calculatrice. */
enum {
    BTN_LEFT = 1 << 0,
    BTN_RIGHT = 1 << 1,
    BTN_SOFT_DROP = 1 << 2, /* descendre plus vite (maintenu) */
    BTN_HARD_DROP = 1 << 3, /* chute immédiate */
    BTN_ROTATE_CW = 1 << 4, /* quart de tour vers la droite */
    BTN_ROTATE_CCW = 1 << 5,
    BTN_HOLD = 1 << 6,      /* mettre la pièce en réserve */
};

typedef enum {
    TETRIS_FALLING,  /* une pièce tombe */
    TETRIS_CLEARING, /* des lignes complètes clignotent avant de disparaître */
    TETRIS_OVER,     /* partie terminée */
} tetris_state_t;

typedef struct {
    /* Cases du plateau : 0 = vide, sinon type de la pièce + 1. */
    uint8_t cells[BOARD_H][BOARD_W];

    /* Pièce qui tombe : type, orientation, coin haut-gauche de sa boîte. */
    uint8_t type, rot;
    int8_t x, y;

    /* Réserve et pièces suivantes, tirées par sacs de 7 (voir tetris.c). */
    uint8_t hold;           /* PIECE_NONE si la réserve est vide */
    bool hold_used;         /* réserve déjà utilisée pour cette pièce */
    uint8_t next[NEXT_COUNT];
    uint8_t bag[PIECE_COUNT];
    uint8_t bag_left;       /* pièces restantes dans le sac (au début) */
    rng_t rng;

    /* Score. */
    uint32_t score;
    uint16_t lines;
    uint8_t level, start_level;

    /* Minuteries et compteurs, en images. */
    uint8_t state;          /* tetris_state_t */
    uint32_t gravity;       /* fraction de ligne accumulée (sur 65536) */
    uint8_t lock_timer;     /* images passées au sol */
    uint8_t lock_resets;    /* déplacements au sol depuis la ligne la plus
                               basse atteinte */
    int8_t lowest_y;        /* ligne la plus basse atteinte par la pièce */
    int8_t das_dir;         /* direction répétée : -1, 0 ou 1 */
    uint8_t das_timer;      /* images depuis l'appui sur cette direction */
    uint8_t clear_timer;    /* images restantes de clignotement */
    uint32_t clear_rows;    /* lignes en cours d'effacement (bit y) */
    uint8_t last_clear;     /* nombre de lignes du dernier effacement */
    uint32_t frames;        /* durée de la partie */
} tetris_t;

/* Commence une partie au niveau [start_level] (1 à LEVEL_MAX). La graine
 * détermine toute la suite des pièces. */
void tetris_new(tetris_t *t, int start_level, uint32_t seed);

/* Avance d'une image. [held] : boutons enfoncés ; [pressed] : boutons
 * enfoncés pendant cette image (un appui bref peut n'apparaître qu'ici). */
void tetris_step(tetris_t *t, unsigned held, unsigned pressed);

/* Vrai tant que la partie n'est pas terminée : c'est elle que l'on
 * sauvegarde et que l'on peut reprendre. */
bool tetris_in_progress(tetris_t const *t);

/* Vrai si la pièce [type] tient à cette place sans chevaucher le plateau. */
bool tetris_fits(tetris_t const *t, int type, int rot, int x, int y);

/* Ligne où la pièce qui tombe s'arrêterait (pièce fantôme). */
int tetris_ghost_y(tetris_t const *t);

/* Vitesse de chute du niveau [level], en lignes par image (sur 65536). */
uint32_t tetris_gravity(int level);

/* Points rapportés par [lines] lignes effacées d'un coup au niveau
 * [level] (sans la chute rapide). */
uint32_t tetris_line_score(int lines, int level);

#endif /* TETRIS_H */
