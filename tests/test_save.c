/* test_save.c — Format du fichier de sauvegarde (save.c). */
#include "test.h"
#include "save.h"
#include <string.h>

/* Une partie avancée de quelques centaines d'images, avec des lignes
 * effacées, une réserve et des pièces au sol. */
static void play_some(tetris_t *t, uint32_t seed)
{
    tetris_new(t, 2, seed);

    /* Une ligne effacée par une barre horizontale. */
    for (int x = 0; x < BOARD_W; x++)
        t->cells[BOARD_H - 1][x] = (x >= 3 && x <= 6) ? 0 : 1 + x % 7;
    t->type = PIECE_I;
    t->rot = 0;
    tetris_step(t, 0, BTN_HARD_DROP);
    for (int f = 0; f < CLEAR_FRAMES; f++)
        tetris_step(t, 0, 0);

    /* Quelques pièces, dont une en réserve. On ne garde que les 4 lignes
     * du bas pour ne jamais perdre. */
    for (int i = 0; i < 8; i++) {
        if (i % 3 == 0)
            tetris_step(t, 0, BTN_HOLD);
        tetris_step(t, BTN_LEFT, BTN_LEFT);
        tetris_step(t, 0, BTN_HARD_DROP);
        for (int f = 0; f < CLEAR_FRAMES; f++)
            tetris_step(t, 0, 0);
        memset(t->cells, 0, (BOARD_H - 4) * BOARD_W);
    }
    for (int f = 0; f < 20; f++)
        tetris_step(t, BTN_SOFT_DROP, 0);
}

/* Deux parties sont-elles identiques, champ par champ ? La répétition des
 * déplacements n'est pas sauvegardée et n'est pas comparée. */
static bool same_game(tetris_t const *a, tetris_t const *b)
{
    tetris_t x = *a, y = *b;
    x.das_dir = y.das_dir = 0;
    x.das_timer = y.das_timer = 0;
    return memcmp(x.cells, y.cells, sizeof x.cells) == 0
        && x.type == y.type && x.rot == y.rot && x.x == y.x && x.y == y.y
        && x.hold == y.hold && x.hold_used == y.hold_used
        && memcmp(x.next, y.next, sizeof x.next) == 0
        && memcmp(x.bag, y.bag, sizeof x.bag) == 0
        && x.bag_left == y.bag_left && x.rng.state == y.rng.state
        && x.score == y.score && x.lines == y.lines && x.level == y.level
        && x.start_level == y.start_level && x.state == y.state
        && x.gravity == y.gravity && x.lock_timer == y.lock_timer
        && x.lock_resets == y.lock_resets && x.lowest_y == y.lowest_y
        && x.clear_timer == y.clear_timer && x.clear_rows == y.clear_rows
        && x.last_clear == y.last_clear && x.frames == y.frames;
}

static void check_roundtrip(void)
{
    save_data_t d, loaded;
    uint8_t buffer[SAVE_MAX_SIZE];

    /* Premier lancement : pas de partie en cours. */
    save_defaults(&d);
    CHECK(!tetris_in_progress(&d.game));
    size_t size = save_serialize(&d, buffer, sizeof buffer);
    CHECK(size > 0);
    CHECK(save_deserialize(&loaded, buffer, size));
    CHECK(!tetris_in_progress(&loaded.game));
    CHECK(loaded.start_level == 1);

    /* Scores et partie en cours. */
    save_defaults(&d);
    scores_record(&d.scores, 12340, 45, 5);
    scores_record(&d.scores, 800, 6, 1);
    d.start_level = 7;
    play_some(&d.game, 77);
    CHECK(tetris_in_progress(&d.game));
    CHECK(d.game.lines > 0 && d.game.hold != PIECE_NONE);

    size = save_serialize(&d, buffer, sizeof buffer);
    CHECK(size > 0 && size <= SAVE_MAX_SIZE);
    CHECK(save_deserialize(&loaded, buffer, size));
    CHECK(loaded.start_level == 7);
    CHECK(memcmp(&loaded.scores, &d.scores, sizeof d.scores) == 0);
    CHECK(same_game(&loaded.game, &d.game));

    /* La partie rechargée continue exactement comme l'originale. */
    for (int f = 0; f < 600; f++) {
        unsigned b = (f % 50 == 0) ? BTN_HARD_DROP : (f % 7 == 0)
            ? BTN_ROTATE_CW : 0;
        tetris_step(&d.game, 0, b);
        tetris_step(&loaded.game, 0, b);
    }
    CHECK(same_game(&loaded.game, &d.game));

    /* Pendant le clignotement des lignes aussi. */
    tetris_t *t = &d.game;
    tetris_new(t, 1, 5);
    for (int x = 0; x < BOARD_W; x++)
        t->cells[BOARD_H - 1][x] = (x >= 3 && x <= 6) ? 0 : 1;
    t->type = PIECE_I;
    t->rot = 0;
    tetris_step(t, 0, BTN_HARD_DROP);
    CHECK(t->state == TETRIS_CLEARING);
    size = save_serialize(&d, buffer, sizeof buffer);
    CHECK(save_deserialize(&loaded, buffer, size));
    CHECK(same_game(&loaded.game, t));

    /* Une partie terminée n'est pas sauvegardée. */
    t->state = TETRIS_OVER;
    size = save_serialize(&d, buffer, sizeof buffer);
    CHECK(save_deserialize(&loaded, buffer, size));
    CHECK(!tetris_in_progress(&loaded.game));
}

static void check_corruption(void)
{
    save_data_t d, loaded;
    uint8_t buffer[SAVE_MAX_SIZE], copy[SAVE_MAX_SIZE];
    save_defaults(&d);
    scores_record(&d.scores, 5000, 20, 3);
    play_some(&d.game, 3);
    size_t size = save_serialize(&d, buffer, sizeof buffer);

    /* Chaque octet modifié est détecté par la somme de contrôle. */
    for (size_t i = 0; i < size; i++) {
        memcpy(copy, buffer, size);
        copy[i] ^= 0x10;
        CHECK(!save_deserialize(&loaded, copy, size));
        CHECK(loaded.scores.best[0].score == 0);
        CHECK(!tetris_in_progress(&loaded.game));
    }

    /* Fichier tronqué, vide ou d'un autre jeu. */
    for (size_t n = 0; n < size; n += 7)
        CHECK(!save_deserialize(&loaded, buffer, n));
    CHECK(!save_deserialize(&loaded, (uint8_t const *)"MSWP\1\3", 6));

    /* Tampon trop petit pour écrire. */
    CHECK(save_serialize(&d, buffer, 20) == 0);
}

/* Recalcule la somme de contrôle après une modification volontaire. */
static void fix_checksum(uint8_t *buffer, size_t size)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < size - 4; i++) {
        h ^= buffer[i];
        h *= 16777619u;
    }
    for (int i = 0; i < 4; i++)
        buffer[size - 4 + i] = h >> (8 * i);
}

static void check_invalid_game(void)
{
    save_data_t d, loaded;
    uint8_t buffer[SAVE_MAX_SIZE];
    save_defaults(&d);
    scores_record(&d.scores, 4321, 10, 2);
    tetris_new(&d.game, 1, 9);

    /* Position des données de la partie dans le fichier. */
    size_t game = 4 + 1 + 1 + 7 * SCORES_COUNT + 2 + 4 + 1 + 1;
    size_t piece = game + BOARD_H * BOARD_W;

    /* Pièce qui chevauche une case du plateau : la partie est ignorée,
     * mais les scores sont gardés. */
    size_t size = save_serialize(&d, buffer, sizeof buffer);
    buffer[game + (d.game.y + 1) * BOARD_W + 4] = 1;
    fix_checksum(buffer, size);
    CHECK(save_deserialize(&loaded, buffer, size));
    CHECK(!tetris_in_progress(&loaded.game));
    CHECK(loaded.scores.best[0].score == 4321);

    /* Forme de pièce inconnue. */
    size = save_serialize(&d, buffer, sizeof buffer);
    buffer[piece] = PIECE_COUNT;
    fix_checksum(buffer, size);
    CHECK(save_deserialize(&loaded, buffer, size));
    CHECK(!tetris_in_progress(&loaded.game));

    /* Case du plateau invalide. */
    size = save_serialize(&d, buffer, sizeof buffer);
    buffer[game + 5] = 200;
    fix_checksum(buffer, size);
    CHECK(save_deserialize(&loaded, buffer, size));
    CHECK(!tetris_in_progress(&loaded.game));

    /* Témoin : sans modification, la partie est bien relue. */
    size = save_serialize(&d, buffer, sizeof buffer);
    fix_checksum(buffer, size);
    CHECK(save_deserialize(&loaded, buffer, size));
    CHECK(tetris_in_progress(&loaded.game));
}

void test_save(void)
{
    check_roundtrip();
    check_corruption();
    check_invalid_game();
}
