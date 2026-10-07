/* tetris.c — Logique d'une partie. */
#include "tetris.h"
#include <string.h>

/* Une ligne entière, en fraction de ligne (voir tetris_t.gravity). */
#define ROW 65536u

/* Vitesse de chute de chaque niveau, en lignes par image (sur 65536),
 * d'après la formule des Tetris récents : une ligne toutes les
 * (0,8 - (niveau - 1) × 0,007) ^ (niveau - 1) secondes. */
static uint32_t const GRAVITY[LEVEL_MAX] = {
      1092, /* niveau  1 :  1000.0 ms par ligne */
      1377, /* niveau  2 :   793.0 ms par ligne */
      1768, /* niveau  3 :   617.8 ms par ligne */
      2311, /* niveau  4 :   472.7 ms par ligne */
      3075, /* niveau  5 :   355.2 ms par ligne */
      4169, /* niveau  6 :   262.0 ms par ligne */
      5759, /* niveau  7 :   189.7 ms par ligne */
      8107, /* niveau  8 :   134.7 ms par ligne */
     11634, /* niveau  9 :    93.9 ms par ligne */
     17026, /* niveau 10 :    64.2 ms par ligne */
     25416, /* niveau 11 :    43.0 ms par ligne */
     38709, /* niveau 12 :    28.2 ms par ligne */
     60169, /* niveau 13 :    18.2 ms par ligne */
     95483, /* niveau 14 :    11.4 ms par ligne */
    154742, /* niveau 15 :     7.1 ms par ligne */
};

static int clamp_level(int level)
{
    if (level < 1)
        return 1;
    return level > LEVEL_MAX ? LEVEL_MAX : level;
}

uint32_t tetris_gravity(int level)
{
    return GRAVITY[clamp_level(level) - 1];
}

uint32_t tetris_line_score(int lines, int level)
{
    static uint16_t const POINTS[5] = { 0, 100, 300, 500, 800 };
    if (lines < 0 || lines > 4)
        return 0;
    return POINTS[lines] * (uint32_t)level;
}

//---
// Pièces suivantes : le sac de 7
//
// Chaque série de 7 pièces contient chaque forme une fois, dans un ordre
// aléatoire. On n'attend donc jamais une barre (I) plus de 12 pièces, et on
// n'en reçoit jamais trois de suite.
//---

static void refill_bag(tetris_t *t)
{
    for (int i = 0; i < PIECE_COUNT; i++)
        t->bag[i] = i;
    /* Mélange de Fisher-Yates. */
    for (int i = PIECE_COUNT - 1; i > 0; i--) {
        int j = rng_below(&t->rng, i + 1);
        uint8_t tmp = t->bag[i];
        t->bag[i] = t->bag[j];
        t->bag[j] = tmp;
    }
    t->bag_left = PIECE_COUNT;
}

static int draw_from_bag(tetris_t *t)
{
    if (t->bag_left == 0)
        refill_bag(t);
    return t->bag[PIECE_COUNT - t->bag_left--];
}

/* Prend la première pièce suivante et complète la file. */
static int pop_next(tetris_t *t)
{
    int type = t->next[0];
    memmove(t->next, t->next + 1, NEXT_COUNT - 1);
    t->next[NEXT_COUNT - 1] = draw_from_bag(t);
    return type;
}

//---
// Mouvements de la pièce
//---

bool tetris_fits(tetris_t const *t, int type, int rot, int x, int y)
{
    cell_t c[4];
    piece_cells(type, rot, c);
    for (int i = 0; i < 4; i++) {
        int cx = x + c[i].x, cy = y + c[i].y;
        if (cx < 0 || cx >= BOARD_W || cy < 0 || cy >= BOARD_H)
            return false;
        if (t->cells[cy][cx])
            return false;
    }
    return true;
}

static bool on_ground(tetris_t const *t)
{
    return !tetris_fits(t, t->type, t->rot, t->x, t->y + 1);
}

static bool try_move(tetris_t *t, int dx, int dy)
{
    if (!tetris_fits(t, t->type, t->rot, t->x + dx, t->y + dy))
        return false;
    t->x += dx;
    t->y += dy;
    return true;
}

/* La pièce a atteint une ligne plus basse que jamais : elle a de nouveau
 * droit à tous ses déplacements au sol. */
static void check_lowest(tetris_t *t)
{
    if (t->y > t->lowest_y) {
        t->lowest_y = t->y;
        t->lock_resets = 0;
        t->lock_timer = 0;
    }
}

/* Un déplacement ou une rotation a réussi. Si la pièce était ou est au sol,
 * le temps avant qu'elle se fige repart de zéro, un nombre limité de fois
 * pour qu'on ne puisse pas la faire tourner indéfiniment. */
static void moved(tetris_t *t)
{
    check_lowest(t);
    if ((t->lock_timer > 0 || on_ground(t))
            && t->lock_resets < LOCK_RESETS_MAX) {
        t->lock_timer = 0;
        t->lock_resets++;
    }
}

/* Rotation avec les décalages du SRS : la pièce peut « sauter » d'une case
 * pour tourner contre un mur ou dans un trou. */
static void try_rotate(tetris_t *t, int dir)
{
    int to = (t->rot + dir) & 3;
    cell_t kicks[KICK_TESTS];
    piece_kicks(t->type, t->rot, dir, kicks);
    for (int i = 0; i < KICK_TESTS; i++) {
        int x = t->x + kicks[i].x, y = t->y + kicks[i].y;
        if (tetris_fits(t, t->type, to, x, y)) {
            t->x = x;
            t->y = y;
            t->rot = to;
            moved(t);
            return;
        }
    }
}

int tetris_ghost_y(tetris_t const *t)
{
    int y = t->y;
    while (tetris_fits(t, t->type, t->rot, t->x, y + 1))
        y++;
    return y;
}

//---
// Apparition et verrouillage
//---

/* Fait apparaître une pièce en haut du plateau. Renvoie faux si elle n'a
 * pas la place : la partie est perdue. */
static bool spawn(tetris_t *t, int type)
{
    t->type = type;
    t->rot = 0;
    t->x = 3; /* colonnes 3 à 5 (3 à 6 pour I, 4 et 5 pour O) */
    t->y = 0;
    t->gravity = 0;
    t->lock_timer = 0;
    t->lock_resets = 0;
    if (!tetris_fits(t, type, 0, t->x, t->y))
        return false;
    /* Comme dans les Tetris récents, la pièce descend aussitôt d'une ligne
     * si elle le peut : elle apparaît en haut de la partie visible. */
    if (tetris_fits(t, type, 0, t->x, t->y + 1))
        t->y++;
    t->lowest_y = t->y;
    return true;
}

static void spawn_next(tetris_t *t)
{
    t->hold_used = false;
    t->state = spawn(t, pop_next(t)) ? TETRIS_FALLING : TETRIS_OVER;
}

/* Fige la pièce dans le plateau, puis compte les lignes complètes. */
static void lock_piece(tetris_t *t)
{
    cell_t c[4];
    piece_cells(t->type, t->rot, c);
    bool visible = false;
    for (int i = 0; i < 4; i++) {
        int cy = t->y + c[i].y;
        t->cells[cy][t->x + c[i].x] = t->type + 1;
        visible |= cy >= BOARD_HIDDEN;
    }
    /* Pièce figée entièrement au-dessus de la partie visible : perdu. */
    if (!visible) {
        t->state = TETRIS_OVER;
        return;
    }

    uint32_t rows = 0;
    int count = 0;
    for (int y = 0; y < BOARD_H; y++) {
        bool full = true;
        for (int x = 0; x < BOARD_W && full; x++)
            full = t->cells[y][x] != 0;
        if (full) {
            rows |= 1u << y;
            count++;
        }
    }
    if (count == 0) {
        spawn_next(t);
        return;
    }

    /* Les points comptent au niveau en cours, avant un éventuel passage au
     * niveau suivant. */
    t->score += tetris_line_score(count, t->level);
    t->lines = (t->lines > UINT16_MAX - count) ? UINT16_MAX
        : t->lines + count;
    int level = 1 + t->lines / LINES_PER_LEVEL;
    if (level < t->start_level)
        level = t->start_level;
    t->level = clamp_level(level);

    t->last_clear = count;
    t->clear_rows = rows;
    t->clear_timer = CLEAR_FRAMES;
    t->state = TETRIS_CLEARING;
}

/* Supprime les lignes complètes ; celles du dessus descendent. */
static void remove_rows(tetris_t *t)
{
    int dst = BOARD_H - 1;
    for (int src = BOARD_H - 1; src >= 0; src--) {
        if (t->clear_rows & (1u << src))
            continue;
        if (dst != src)
            memcpy(t->cells[dst], t->cells[src], BOARD_W);
        dst--;
    }
    for (; dst >= 0; dst--)
        memset(t->cells[dst], 0, BOARD_W);
    t->clear_rows = 0;
}

//---
// Une image de jeu
//---

/* Déplacements gauche / droite : un appui déplace la pièce tout de suite ;
 * une flèche maintenue la déplace encore après DAS_FRAMES images, puis
 * toutes les ARR_FRAMES images. Le dernier appui l'emporte si les deux
 * flèches sont enfoncées. La répétition continue de se « charger » pendant
 * l'effacement des lignes ([can_move] faux). */
static void handle_shift(tetris_t *t, unsigned held, unsigned pressed,
    bool can_move)
{
    int dir = 0;
    if (pressed & BTN_LEFT)
        dir = -1;
    if (pressed & BTN_RIGHT)
        dir = 1;
    if (dir) {
        t->das_dir = dir;
        t->das_timer = 0;
        if (can_move && try_move(t, dir, 0))
            moved(t);
        return;
    }

    unsigned button = (t->das_dir < 0) ? BTN_LEFT : BTN_RIGHT;
    if (t->das_dir == 0 || !(held & button)) {
        /* Flèche relâchée : l'autre est peut-être encore enfoncée. */
        t->das_dir = (held & BTN_LEFT) ? -1 : (held & BTN_RIGHT) ? 1 : 0;
        t->das_timer = 0;
        return;
    }

    /* Le compteur boucle entre DAS_FRAMES et DAS_FRAMES + ARR_FRAMES - 1 :
     * un déplacement chaque fois qu'il vaut DAS_FRAMES. */
    if (++t->das_timer >= DAS_FRAMES + ARR_FRAMES)
        t->das_timer = DAS_FRAMES;
    if (can_move && t->das_timer == DAS_FRAMES && try_move(t, t->das_dir, 0))
        moved(t);
}

/* Met la pièce en réserve (une fois par pièce) et prend celle qui y était,
 * ou la suivante si la réserve était vide. */
static void hold_piece(tetris_t *t)
{
    if (t->hold_used)
        return;
    int type = (t->hold == PIECE_NONE) ? pop_next(t) : t->hold;
    t->hold = t->type;
    if (!spawn(t, type)) {
        t->state = TETRIS_OVER;
        return;
    }
    t->hold_used = true;
}

/* Gravité, chute douce et verrouillage au sol. */
static void fall(tetris_t *t, unsigned held, unsigned pressed)
{
    uint32_t g = tetris_gravity(t->level);
    bool soft = held & BTN_SOFT_DROP;
    if (soft && g < SOFT_DROP_GRAVITY)
        g = SOFT_DROP_GRAVITY;
    /* Un appui bref sur la chute douce fait descendre d'une ligne (aux
     * niveaux rapides, g dépasse déjà une ligne par image). */
    if ((pressed & BTN_SOFT_DROP) && g < ROW && t->gravity < ROW - g) {
        t->gravity = ROW - g;
        soft = true;
    }

    t->gravity += g;
    while (t->gravity >= ROW) {
        t->gravity -= ROW;
        if (!try_move(t, 0, 1)) {
            t->gravity = 0;
            break;
        }
        /* Chute douce : un point par ligne descendue. */
        if (soft)
            t->score++;
        check_lowest(t);
    }

    /* Le temps au sol ne compte que lorsque la pièce touche quelque chose. */
    if (on_ground(t) && ++t->lock_timer >= LOCK_FRAMES)
        lock_piece(t);
}

void tetris_step(tetris_t *t, unsigned held, unsigned pressed)
{
    if (t->state == TETRIS_OVER)
        return;
    if (t->frames < TETRIS_MAX_FRAMES)
        t->frames++;

    if (t->state == TETRIS_CLEARING) {
        handle_shift(t, held, pressed, false);
        if (--t->clear_timer == 0) {
            remove_rows(t);
            spawn_next(t);
        }
        return;
    }

    if (pressed & BTN_HOLD) {
        hold_piece(t);
        if (t->state == TETRIS_OVER)
            return;
    }
    if (pressed & BTN_ROTATE_CW)
        try_rotate(t, 1);
    if (pressed & BTN_ROTATE_CCW)
        try_rotate(t, -1);
    handle_shift(t, held, pressed, true);

    if (pressed & BTN_HARD_DROP) {
        /* Chute immédiate : deux points par ligne, et la pièce se fige. */
        int rows = 0;
        while (try_move(t, 0, 1))
            rows++;
        t->score += 2 * rows;
        lock_piece(t);
        return;
    }
    fall(t, held, pressed);
}

void tetris_new(tetris_t *t, int start_level, uint32_t seed)
{
    memset(t, 0, sizeof *t);
    t->start_level = t->level = clamp_level(start_level);
    rng_seed(&t->rng, seed);
    t->hold = PIECE_NONE;
    for (int i = 0; i < NEXT_COUNT; i++)
        t->next[i] = draw_from_bag(t);
    spawn_next(t);
}

bool tetris_in_progress(tetris_t const *t)
{
    return t->state != TETRIS_OVER;
}
