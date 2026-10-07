/* test_tetris.c — Règles d'une partie (tetris.c) et scores (scores.c). */
#include "test.h"
#include "scores.h"
#include "tetris.h"
#include <string.h>

/* Avance de [n] images sans toucher au clavier. */
static void idle(tetris_t *t, int n)
{
    for (int i = 0; i < n; i++)
        tetris_step(t, 0, 0);
}

/* Un appui bref sur [button]. */
static void tap(tetris_t *t, unsigned button)
{
    tetris_step(t, 0, button);
}

/* Remplit la ligne [y] du plateau, sauf les colonnes [hole_from] à
 * [hole_to]. */
static void fill_row(tetris_t *t, int y, int hole_from, int hole_to)
{
    for (int x = 0; x < BOARD_W; x++)
        t->cells[y][x] = (x >= hole_from && x <= hole_to) ? 0 : 1;
}

/* Remplace la pièce qui tombe, sans passer par le sac. */
static void set_piece(tetris_t *t, int type, int rot, int x, int y)
{
    t->type = type;
    t->rot = rot;
    t->x = x;
    t->y = y;
    t->lowest_y = y;
    t->lock_timer = 0;
    t->lock_resets = 0;
}

static void check_new_game(void)
{
    tetris_t t;
    tetris_new(&t, 1, 42);
    CHECK(tetris_in_progress(&t));
    CHECK(t.state == TETRIS_FALLING);
    CHECK(t.level == 1 && t.start_level == 1);
    CHECK(t.score == 0 && t.lines == 0);
    CHECK(t.hold == PIECE_NONE);
    CHECK(t.type < PIECE_COUNT);
    for (int i = 0; i < NEXT_COUNT; i++)
        CHECK(t.next[i] < PIECE_COUNT);
    CHECK(tetris_fits(&t, t.type, t.rot, t.x, t.y));
    /* La pièce apparaît au milieu, en haut de la partie visible. */
    CHECK(t.x == 3 && t.y == 1);

    /* Niveau de départ hors limites : ramené entre 1 et LEVEL_MAX. */
    tetris_new(&t, 0, 1);
    CHECK(t.level == 1);
    tetris_new(&t, 99, 1);
    CHECK(t.level == LEVEL_MAX);
}

/* Suite des [n] premières pièces d'une partie (le plateau est vidé après
 * chaque chute pour ne jamais perdre). */
static void piece_sequence(uint32_t seed, int n, uint8_t *out)
{
    tetris_t t;
    tetris_new(&t, 1, seed);
    for (int i = 0; i < n; i++) {
        out[i] = t.type;
        tap(&t, BTN_HARD_DROP);
        if (t.state == TETRIS_CLEARING)
            idle(&t, CLEAR_FRAMES);
        memset(t.cells, 0, sizeof t.cells);
    }
}

static void check_bag(void)
{
    /* Chaque série de 7 pièces contient les 7 formes. */
    for (uint32_t seed = 1; seed <= 50; seed++) {
        uint8_t seq[70];
        piece_sequence(seed, 70, seq);
        for (int bag = 0; bag < 10; bag++) {
            int seen = 0;
            for (int i = 0; i < 7; i++)
                seen |= 1 << seq[bag * 7 + i];
            CHECK(seen == (1 << PIECE_COUNT) - 1);
        }
    }

    /* Même graine, même suite ; autre graine, autre suite. */
    uint8_t a[28], b[28], c[28];
    piece_sequence(123, 28, a);
    piece_sequence(123, 28, b);
    piece_sequence(124, 28, c);
    CHECK(memcmp(a, b, sizeof a) == 0);
    CHECK(memcmp(a, c, sizeof a) != 0);
}

static void check_gravity(void)
{
    tetris_t t;
    tetris_new(&t, 1, 5);
    int y = t.y;
    /* Niveau 1 : une ligne par seconde environ. */
    idle(&t, 55);
    CHECK(t.y == y);
    idle(&t, 10);
    CHECK(t.y == y + 1);

    /* Les niveaux accélèrent, et le niveau 15 fait tomber de plusieurs
     * lignes par image. */
    for (int level = 2; level <= LEVEL_MAX; level++)
        CHECK(tetris_gravity(level) > tetris_gravity(level - 1));
    CHECK(tetris_gravity(LEVEL_MAX) > 2 * 65536);
    CHECK(tetris_gravity(0) == tetris_gravity(1));
    CHECK(tetris_gravity(99) == tetris_gravity(LEVEL_MAX));

    /* Chute douce : plus rapide, un point par ligne. Un appui bref fait
     * descendre d'une ligne tout de suite. */
    tetris_new(&t, 1, 5);
    y = t.y;
    tetris_step(&t, BTN_SOFT_DROP, BTN_SOFT_DROP);
    CHECK(t.y == y + 1);
    CHECK(t.score == 1);
    for (int i = 0; i < 9; i++)
        tetris_step(&t, BTN_SOFT_DROP, 0);
    CHECK(t.y >= y + 5);
    CHECK(t.score == (uint32_t)(t.y - y));
}

static void check_hard_drop(void)
{
    tetris_t t;
    tetris_new(&t, 1, 9);
    int type = t.type, next = t.next[0];
    int ghost = tetris_ghost_y(&t);
    int rows = ghost - t.y;
    CHECK(rows > 15);
    tap(&t, BTN_HARD_DROP);
    /* La pièce est figée au fond, deux points par ligne, puis la suivante
     * apparaît. */
    CHECK(t.score == (uint32_t)(2 * rows));
    CHECK(t.type == next);
    int count = 0;
    for (int x = 0; x < BOARD_W; x++)
        count += t.cells[BOARD_H - 1][x] == type + 1;
    CHECK(count >= 1);
}

static void check_shift(void)
{
    tetris_t t;
    tetris_new(&t, 1, 3);
    set_piece(&t, PIECE_T, 0, 3, 5);

    /* Appui : déplacement immédiat. Maintenu : nouveau déplacement après
     * DAS_FRAMES images, puis toutes les ARR_FRAMES images. */
    tetris_step(&t, BTN_RIGHT, BTN_RIGHT);
    CHECK(t.x == 4);
    for (int i = 1; i < DAS_FRAMES; i++)
        tetris_step(&t, BTN_RIGHT, 0);
    CHECK(t.x == 4);
    tetris_step(&t, BTN_RIGHT, 0);
    CHECK(t.x == 5);
    for (int i = 0; i < ARR_FRAMES; i++)
        tetris_step(&t, BTN_RIGHT, 0);
    CHECK(t.x == 6);
    /* Contre le mur, la pièce s'arrête (T : colonnes x à x + 2). */
    for (int i = 0; i < 20; i++)
        tetris_step(&t, BTN_RIGHT, 0);
    CHECK(t.x == BOARD_W - 3);

    /* Relâcher puis appuyer à gauche : un seul déplacement. */
    idle(&t, 1);
    tap(&t, BTN_LEFT);
    CHECK(t.x == BOARD_W - 4);
    idle(&t, 30);
    CHECK(t.x == BOARD_W - 4);
}

static void check_rotation(void)
{
    tetris_t t;
    tetris_new(&t, 1, 3);

    /* Rotation simple, au milieu du plateau. */
    set_piece(&t, PIECE_T, 0, 3, 8);
    tap(&t, BTN_ROTATE_CW);
    CHECK(t.rot == 1 && t.x == 3 && t.y == 8);
    tap(&t, BTN_ROTATE_CCW);
    tap(&t, BTN_ROTATE_CCW);
    CHECK(t.rot == 3);

    /* I vertical collé au mur droit : en tournant, il se décale vers la
     * gauche (décalage du SRS) au lieu de rester bloqué. */
    set_piece(&t, PIECE_I, 1, BOARD_W - 3, 8); /* colonne x + 2 = 9 */
    CHECK(tetris_fits(&t, PIECE_I, 1, t.x, t.y));
    CHECK(!tetris_fits(&t, PIECE_I, 2, t.x, t.y));
    tap(&t, BTN_ROTATE_CW);
    CHECK(t.rot == 2);
    CHECK(t.x < BOARD_W - 3);

    /* Pièce coincée de toutes parts : la rotation est refusée. */
    tetris_new(&t, 1, 3);
    for (int y = 10; y < BOARD_H; y++)
        fill_row(&t, y, 4, 4);
    set_piece(&t, PIECE_I, 1, 2, 10); /* puits d'une case en colonne 4 */
    CHECK(tetris_fits(&t, PIECE_I, 1, t.x, t.y));
    tap(&t, BTN_ROTATE_CW);
    CHECK(t.rot == 1 && t.x == 2);

    /* O ne tourne pas, mais l'appui ne gêne rien. */
    tetris_new(&t, 1, 3);
    set_piece(&t, PIECE_O, 0, 3, 8);
    tap(&t, BTN_ROTATE_CW);
    CHECK(t.x == 3 && t.y == 8);
}

static void check_lines_and_score(void)
{
    CHECK(tetris_line_score(1, 1) == 100);
    CHECK(tetris_line_score(2, 1) == 300);
    CHECK(tetris_line_score(3, 2) == 1000);
    CHECK(tetris_line_score(4, 5) == 4000);
    CHECK(tetris_line_score(0, 3) == 0);
    CHECK(tetris_line_score(5, 3) == 0);

    /* Une ligne : la barre horizontale complète la ligne du bas. */
    tetris_t t;
    tetris_new(&t, 1, 11);
    fill_row(&t, BOARD_H - 1, 3, 6);
    t.cells[BOARD_H - 2][0] = 1; /* une case au-dessus, qui descendra */
    set_piece(&t, PIECE_I, 0, 3, 1);
    int rows = tetris_ghost_y(&t) - t.y;
    tap(&t, BTN_HARD_DROP);
    CHECK(t.state == TETRIS_CLEARING);
    CHECK(t.clear_rows == 1u << (BOARD_H - 1));
    CHECK(t.lines == 1 && t.last_clear == 1);
    CHECK(t.score == (uint32_t)(2 * rows) + 100);
    idle(&t, CLEAR_FRAMES - 1);
    CHECK(t.state == TETRIS_CLEARING);
    idle(&t, 1);
    CHECK(t.state == TETRIS_FALLING);
    /* La ligne a disparu, la case du dessus est descendue. */
    CHECK(t.cells[BOARD_H - 1][0] == 1);
    for (int x = 1; x < BOARD_W; x++)
        CHECK(t.cells[BOARD_H - 1][x] == 0);

    /* Un Tetris : quatre lignes d'un coup avec une barre verticale. */
    tetris_new(&t, 3, 11);
    for (int y = BOARD_H - 4; y < BOARD_H; y++)
        fill_row(&t, y, 9, 9);
    set_piece(&t, PIECE_I, 1, BOARD_W - 3, 2);
    rows = tetris_ghost_y(&t) - t.y;
    tap(&t, BTN_HARD_DROP);
    CHECK(t.last_clear == 4 && t.lines == 4);
    CHECK(t.score == (uint32_t)(2 * rows) + 800 * 3);
    idle(&t, CLEAR_FRAMES);
    for (int y = 0; y < BOARD_H; y++)
        for (int x = 0; x < BOARD_W; x++)
            CHECK(t.cells[y][x] == 0);
}

static void check_levels(void)
{
    /* On passe au niveau 2 à la 10e ligne. */
    tetris_t t;
    tetris_new(&t, 1, 2);
    for (int i = 0; i < 10; i++) {
        memset(t.cells, 0, sizeof t.cells);
        fill_row(&t, BOARD_H - 1, 3, 6);
        set_piece(&t, PIECE_I, 0, 3, 1);
        tap(&t, BTN_HARD_DROP);
        CHECK(t.level == (i < 9 ? 1 : 2));
        idle(&t, CLEAR_FRAMES);
    }
    CHECK(t.lines == 10);

    /* En commençant au niveau 5, on y reste jusqu'à 50 lignes. */
    tetris_new(&t, 5, 2);
    t.lines = 48;
    fill_row(&t, BOARD_H - 1, 3, 6);
    set_piece(&t, PIECE_I, 0, 3, 1);
    tap(&t, BTN_HARD_DROP);
    CHECK(t.level == 5);
    idle(&t, CLEAR_FRAMES);
    t.lines = 49;
    fill_row(&t, BOARD_H - 1, 3, 6);
    set_piece(&t, PIECE_I, 0, 3, 1);
    tap(&t, BTN_HARD_DROP);
    CHECK(t.level == 6);

    /* Le niveau ne dépasse jamais LEVEL_MAX. */
    idle(&t, CLEAR_FRAMES);
    t.lines = 500;
    fill_row(&t, BOARD_H - 1, 3, 6);
    set_piece(&t, PIECE_I, 0, 3, 1);
    tap(&t, BTN_HARD_DROP);
    CHECK(t.level == LEVEL_MAX);
}

static void check_hold(void)
{
    tetris_t t;
    tetris_new(&t, 1, 8);
    int first = t.type, next = t.next[0];

    /* Réserve vide : la pièce y va, la suivante arrive. */
    tap(&t, BTN_HOLD);
    CHECK(t.hold == first && t.type == next && t.hold_used);
    CHECK(t.x == 3 && t.y == 1 && t.rot == 0);

    /* Une seule fois par pièce. */
    int current = t.type;
    tap(&t, BTN_HOLD);
    CHECK(t.hold == first && t.type == current);

    /* Après la chute, on peut échanger avec la réserve. */
    tap(&t, BTN_HARD_DROP);
    CHECK(!t.hold_used);
    current = t.type;
    tap(&t, BTN_HOLD);
    CHECK(t.type == first && t.hold == current);
}

static void check_lock_delay(void)
{
    tetris_t t;
    tetris_new(&t, 1, 4);
    set_piece(&t, PIECE_T, 0, 3, BOARD_H - 2); /* posée au fond */
    int next = t.next[0];

    /* Au sol, la pièce se fige après LOCK_FRAMES images. */
    idle(&t, LOCK_FRAMES - 1);
    CHECK(t.type == PIECE_T && t.y == BOARD_H - 2);
    idle(&t, 1);
    CHECK(t.type == next);
    CHECK(t.cells[BOARD_H - 1][4] == PIECE_T + 1);

    /* Chaque déplacement au sol relance le délai... */
    tetris_new(&t, 1, 4);
    set_piece(&t, PIECE_T, 0, 3, BOARD_H - 2);
    idle(&t, LOCK_FRAMES - 5);
    tap(&t, BTN_RIGHT);
    idle(&t, LOCK_FRAMES - 5);
    CHECK(t.type == PIECE_T);

    /* ... mais pas plus de LOCK_RESETS_MAX fois. */
    tetris_new(&t, 1, 4);
    set_piece(&t, PIECE_T, 0, 3, BOARD_H - 2);
    int frames = 0;
    for (int i = 0; i < 100 && t.type == PIECE_T; i++) {
        tap(&t, (i % 2) ? BTN_LEFT : BTN_RIGHT);
        idle(&t, 3);
        frames += 4;
    }
    CHECK(t.type != PIECE_T);
    CHECK(frames <= (LOCK_RESETS_MAX + 1) * 4 + LOCK_FRAMES);
}

static void check_game_over(void)
{
    /* Plateau rempli jusqu'à la partie cachée (sans ligne complète) : la
     * pièce se fige entièrement au-dessus de la partie visible. */
    tetris_t t;
    tetris_new(&t, 1, 6);
    for (int y = BOARD_HIDDEN; y < BOARD_H; y++)
        fill_row(&t, y, 0, 0);
    set_piece(&t, PIECE_O, 0, 3, 0);
    tap(&t, BTN_HARD_DROP);
    CHECK(!tetris_in_progress(&t));
    CHECK(t.state == TETRIS_OVER);

    /* La pièce suivante n'a pas la place d'apparaître : toutes les formes
     * ont une case dans la 2e ligne cachée, colonnes 3 à 5. */
    tetris_new(&t, 1, 6);
    for (int x = 3; x <= 6; x++)
        t.cells[1][x] = 1;
    set_piece(&t, PIECE_O, 0, 6, 2); /* O à droite, loin du blocage */
    tap(&t, BTN_HARD_DROP);
    CHECK(t.state == TETRIS_OVER);
    CHECK(t.cells[BOARD_H - 1][7] == PIECE_O + 1);

    /* Une partie terminée ne bouge plus. */
    tetris_t copy = t;
    tetris_step(&t, BTN_LEFT, BTN_LEFT | BTN_HARD_DROP);
    CHECK(memcmp(&t, &copy, sizeof t) == 0);

    /* Sans rien toucher, les pièces s'empilent jusqu'à la fin. */
    tetris_new(&t, 1, 6);
    for (int i = 0; i < 60 * 60 * 10 && tetris_in_progress(&t); i++)
        idle(&t, 1);
    CHECK(!tetris_in_progress(&t));
}

static void check_scores(void)
{
    scores_t s;
    scores_reset(&s);
    CHECK(scores_record(&s, 500, 3, 1) == 0);
    CHECK(scores_record(&s, 900, 6, 1) == 0);
    CHECK(scores_record(&s, 700, 4, 1) == 1);
    /* À égalité, l'ancien score reste devant. */
    CHECK(scores_record(&s, 700, 5, 2) == 2);
    CHECK(s.best[1].lines == 4 && s.best[2].lines == 5);
    CHECK(scores_record(&s, 100, 1, 1) == 4);
    /* Classement plein : un score trop faible n'y entre pas. */
    CHECK(scores_record(&s, 50, 0, 1) == -1);
    CHECK(scores_record(&s, 600, 3, 1) == 3);
    CHECK(s.best[0].score == 900 && s.best[4].score == 500);
    /* Une partie à zéro point compte, mais n'est pas classée. */
    CHECK(scores_record(&s, 0, 0, 1) == -1);
    CHECK(s.played == 8);
    CHECK(s.total_lines == 3 + 6 + 4 + 5 + 1 + 0 + 3);
}

void test_tetris(void)
{
    check_new_game();
    check_bag();
    check_gravity();
    check_hard_drop();
    check_shift();
    check_rotation();
    check_lines_and_score();
    check_levels();
    check_hold();
    check_lock_delay();
    check_game_over();
}

void test_scores(void)
{
    check_scores();
}
