/* bot.c — Petit joueur automatique pour la démonstration du README.
 *
 * Il rejoue la même logique que le jeu (src/tetris.c) et écrit, pour
 * chaque pièce, les touches à appuyer dans la syntaxe des scripts du
 * simulateur (sim/README.md). Les touches sont donc exactement celles qu'il
 * faudrait taper sur la calculatrice.
 *
 * Usage : build/bot graine niveau pièces
 *   graine  celle du simulateur au lancement de la partie (SEED:n)
 *   niveau  niveau de départ
 *   pièces  nombre de pièces à jouer
 *
 * Pour chaque pièce, il essaie toutes les orientations et toutes les
 * colonnes, et garde la position que préfère une petite formule (hauteur,
 * trous, relief), en gardant si possible la colonne de droite libre pour y
 * glisser une barre et faire un Tetris. */
#include "tetris.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Images d'attente après chaque touche, pour un rythme humain. */
#define WAIT_TAP 4
#define WAIT_DROP 12

/* Pièce avant laquelle on prend la capture d'une partie en cours. */
#define GAME_SHOT_PIECE 15

/* Une touche du script et le bouton du jeu correspondant (play.c). */
typedef struct {
    char const *name;
    unsigned button;
} bot_key_t;

static bot_key_t const KEY_CW = { "SHIFT", BTN_ROTATE_CW };
static bot_key_t const KEY_CCW = { "ALPHA", BTN_ROTATE_CCW };
static bot_key_t const KEY_LEFT = { "LEFT", BTN_LEFT };
static bot_key_t const KEY_RIGHT = { "RIGHT", BTN_RIGHT };
static bot_key_t const KEY_DROP = { "UP", BTN_HARD_DROP };

/* Vrai une fois la capture du premier Tetris écrite. */
static bool tetris_shot;

/* Appuie sur une touche puis attend [wait] images. */
static void press(tetris_t *t, bot_key_t key, int wait, FILE *out)
{
    tetris_step(t, 0, key.button);
    if (out) {
        fprintf(out, "%s ", key.name);
        /* L'image suivante montre l'annonce du Tetris. */
        if (t->state == TETRIS_CLEARING && t->last_clear == 4
                && !tetris_shot) {
            fprintf(out, "SHOT:tetris ");
            tetris_shot = true;
        }
        fprintf(out, "WAIT:%d ", wait);
    }
    for (int i = 0; i < wait; i++)
        tetris_step(t, 0, 0);
}

/* Attend que les lignes complètes aient disparu. */
static void wait_clear(tetris_t *t, FILE *out)
{
    int frames = 0;
    while (t->state == TETRIS_CLEARING) {
        tetris_step(t, 0, 0);
        frames++;
    }
    if (out && frames)
        fprintf(out, "WAIT:%d ", frames);
}

/* Place la pièce : [rot] quarts de tour, puis jusqu'à la colonne [x], puis
 * chute immédiate. Renvoie faux si la colonne n'est pas atteignable. */
static bool place(tetris_t *t, int rot, int x, FILE *out)
{
    if (rot == 3)
        press(t, KEY_CCW, WAIT_TAP, out);
    else
        for (int i = 0; i < rot; i++)
            press(t, KEY_CW, WAIT_TAP, out);
    if (t->rot != rot)
        return false;
    while (t->x != x) {
        int before = t->x;
        press(t, x < t->x ? KEY_LEFT : KEY_RIGHT, WAIT_TAP, out);
        if (t->x == before)
            return false;
    }
    press(t, KEY_DROP, WAIT_DROP, out);
    wait_clear(t, out);
    return true;
}

/* Note d'un plateau : plus elle est haute, mieux c'est. */
static double evaluate(tetris_t const *t, int lines)
{
    int height[BOARD_W];
    int holes = 0, well_blocks = 0;
    for (int x = 0; x < BOARD_W; x++) {
        height[x] = 0;
        for (int y = 0; y < BOARD_H; y++) {
            if (t->cells[y][x]) {
                if (!height[x])
                    height[x] = BOARD_H - y;
                if (x == BOARD_W - 1)
                    well_blocks++;
            } else if (height[x]) {
                holes++;
            }
        }
    }
    int total = 0, bumps = 0, highest = 0;
    for (int x = 0; x < BOARD_W; x++) {
        total += height[x];
        if (height[x] > highest)
            highest = height[x];
        /* La colonne de droite est un puits : son relief ne compte pas. */
        if (x < BOARD_W - 2)
            bumps += abs(height[x] - height[x + 1]);
    }

    double score = -0.51 * total - 3.0 * holes - 0.25 * bumps;
    /* Un Tetris vaut bien mieux que des lignes isolées, tant que la pile
     * reste basse. */
    if (lines == 4)
        score += 20;
    else if (highest < 12)
        score -= 8.0 * lines;
    else
        score += 2.0 * lines;
    score -= 2.0 * well_blocks;
    if (t->state == TETRIS_OVER)
        score -= 1000;
    return score;
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "usage : %s graine niveau pièces\n", argv[0]);
        return 1;
    }
    uint32_t seed = strtoul(argv[1], NULL, 0);
    int level = atoi(argv[2]), pieces = atoi(argv[3]);

    tetris_t t;
    tetris_new(&t, level, seed);
    for (int n = 0; n < pieces && tetris_in_progress(&t); n++) {
        int best_rot = 0, best_x = t.x;
        double best = -1e9;
        for (int rot = 0; rot < ROT_COUNT; rot++) {
            for (int x = -3; x < BOARD_W; x++) {
                tetris_t copy = t;
                int lines = copy.lines;
                if (!place(&copy, rot, x, NULL))
                    continue;
                double score = evaluate(&copy, copy.lines - lines);
                if (score > best) {
                    best = score;
                    best_rot = rot;
                    best_x = x;
                }
            }
        }
        if (n + 1 == GAME_SHOT_PIECE)
            printf("SHOT:game ");
        int lines = t.lines;
        place(&t, best_rot, best_x, stdout);
        printf("# pièce %d", n + 1);
        if (t.lines - lines == 4)
            printf(", Tetris !");
        printf("\n");
    }
    fprintf(stderr, "bot : %d lignes, score %lu, %s\n", t.lines,
        (unsigned long)t.score, tetris_in_progress(&t) ? "en cours"
        : "perdu");
    return 0;
}
