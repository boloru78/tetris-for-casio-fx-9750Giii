/* save.c — Sauvegarde des scores et de la partie en cours.
 *
 * Format du fichier (version 1), entiers en petit-boutiste :
 *
 *   octets  contenu
 *   4       signature "TTRS"
 *   1       version du format (1)
 *   1       nombre de meilleurs scores N
 *   7 × N   meilleurs scores : u32 score, u16 lignes, u8 niveau
 *   2       parties terminées
 *   4       lignes effacées en tout
 *   1       dernier niveau de départ choisi
 *   1       1 si une partie en cours suit, 0 sinon
 *   [partie en cours]
 *           22 × 10 octets : cases du plateau, ligne par ligne (0 à 7)
 *           u8 pièce, u8 orientation, i8 x, i8 y
 *           u8 réserve (255 = vide), u8 réserve déjà utilisée
 *           3 × u8 pièces suivantes, 7 × u8 sac, u8 pièces restantes
 *           u32 état du générateur aléatoire
 *           u32 score, u16 lignes, u8 niveau, u8 niveau de départ
 *           u8 état, u32 gravité, u8 temps au sol, u8 déplacements au sol,
 *           i8 ligne la plus basse, u8 clignotement restant,
 *           u32 lignes à effacer, u8 lignes du dernier effacement,
 *           u32 durée en images
 *   4       somme de contrôle FNV-1a de tous les octets précédents
 *
 * La répétition des déplacements (DAS) n'est pas stockée : elle repart de
 * zéro à la reprise, quand plus aucune touche n'est enfoncée. */
#include "save.h"
#include "platform.h"
#include <string.h>

#define SAVE_VERSION 1

static uint32_t fnv1a(uint8_t const *data, size_t size)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < size; i++) {
        h ^= data[i];
        h *= 16777619u;
    }
    return h;
}

void save_defaults(save_data_t *d)
{
    memset(d, 0, sizeof *d);
    scores_reset(&d->scores);
    d->start_level = 1;
    d->game.state = TETRIS_OVER;
}

//---
// Écriture
//---

typedef struct {
    uint8_t *data;
    size_t size, max;
} writer_t;

static void put8(writer_t *w, uint32_t v)
{
    if (w->size < w->max)
        w->data[w->size] = v;
    w->size++;
}
static void put16(writer_t *w, uint32_t v)
{
    put8(w, v);
    put8(w, v >> 8);
}
static void put32(writer_t *w, uint32_t v)
{
    put16(w, v);
    put16(w, v >> 16);
}

static void write_game(writer_t *w, tetris_t const *t)
{
    for (int y = 0; y < BOARD_H; y++)
        for (int x = 0; x < BOARD_W; x++)
            put8(w, t->cells[y][x]);
    put8(w, t->type);
    put8(w, t->rot);
    put8(w, (uint8_t)t->x);
    put8(w, (uint8_t)t->y);
    put8(w, t->hold);
    put8(w, t->hold_used);
    for (int i = 0; i < NEXT_COUNT; i++)
        put8(w, t->next[i]);
    for (int i = 0; i < PIECE_COUNT; i++)
        put8(w, t->bag[i]);
    put8(w, t->bag_left);
    put32(w, t->rng.state);
    put32(w, t->score);
    put16(w, t->lines);
    put8(w, t->level);
    put8(w, t->start_level);
    put8(w, t->state);
    put32(w, t->gravity);
    put8(w, t->lock_timer);
    put8(w, t->lock_resets);
    put8(w, (uint8_t)t->lowest_y);
    put8(w, t->clear_timer);
    put32(w, t->clear_rows);
    put8(w, t->last_clear);
    put32(w, t->frames);
}

size_t save_serialize(save_data_t const *d, uint8_t *buffer, size_t max)
{
    writer_t w = { buffer, 0, max };

    put8(&w, 'T'); put8(&w, 'T'); put8(&w, 'R'); put8(&w, 'S');
    put8(&w, SAVE_VERSION);
    put8(&w, SCORES_COUNT);
    for (int i = 0; i < SCORES_COUNT; i++) {
        score_entry_t const *e = &d->scores.best[i];
        put32(&w, e->score);
        put16(&w, e->lines);
        put8(&w, e->level);
    }
    put16(&w, d->scores.played);
    put32(&w, d->scores.total_lines);
    put8(&w, d->start_level);

    bool has_game = tetris_in_progress(&d->game);
    put8(&w, has_game);
    if (has_game)
        write_game(&w, &d->game);

    if (w.size + 4 > max)
        return 0;
    put32(&w, fnv1a(buffer, w.size));
    return w.size;
}

//---
// Lecture
//---

typedef struct {
    uint8_t const *data;
    size_t size, pos;
    bool error;
} reader_t;

static uint32_t get8(reader_t *r)
{
    if (r->pos >= r->size) {
        r->error = true;
        return 0;
    }
    return r->data[r->pos++];
}
static uint32_t get16(reader_t *r)
{
    uint32_t lo = get8(r);
    return lo | (get8(r) << 8);
}
static uint32_t get32(reader_t *r)
{
    uint32_t lo = get16(r);
    return lo | (get16(r) << 16);
}

/* Vrai si toutes les cases de la ligne [y] sont pleines. */
static bool row_full(tetris_t const *t, int y)
{
    for (int x = 0; x < BOARD_W; x++)
        if (!t->cells[y][x])
            return false;
    return true;
}

/* Lit et vérifie une partie en cours. Renvoie faux si elle est incohérente :
 * un fichier abîmé ne doit jamais faire planter le jeu. */
static bool read_game(reader_t *r, tetris_t *t)
{
    memset(t, 0, sizeof *t);
    for (int y = 0; y < BOARD_H; y++) {
        for (int x = 0; x < BOARD_W; x++) {
            t->cells[y][x] = get8(r);
            if (t->cells[y][x] > PIECE_COUNT)
                return false;
        }
    }
    t->type = get8(r);
    t->rot = get8(r);
    t->x = (int8_t)get8(r);
    t->y = (int8_t)get8(r);
    t->hold = get8(r);
    t->hold_used = get8(r) != 0;
    for (int i = 0; i < NEXT_COUNT; i++)
        t->next[i] = get8(r);
    for (int i = 0; i < PIECE_COUNT; i++)
        t->bag[i] = get8(r);
    t->bag_left = get8(r);
    t->rng.state = get32(r);
    t->score = get32(r);
    t->lines = get16(r);
    t->level = get8(r);
    t->start_level = get8(r);
    t->state = get8(r);
    t->gravity = get32(r);
    t->lock_timer = get8(r);
    t->lock_resets = get8(r);
    t->lowest_y = (int8_t)get8(r);
    t->clear_timer = get8(r);
    t->clear_rows = get32(r);
    t->last_clear = get8(r);
    t->frames = get32(r);
    if (r->error)
        return false;

    if (t->type >= PIECE_COUNT || t->rot >= ROT_COUNT)
        return false;
    if (t->hold != PIECE_NONE && t->hold >= PIECE_COUNT)
        return false;
    for (int i = 0; i < NEXT_COUNT; i++)
        if (t->next[i] >= PIECE_COUNT)
            return false;
    for (int i = 0; i < PIECE_COUNT; i++)
        if (t->bag[i] >= PIECE_COUNT)
            return false;
    if (t->bag_left > PIECE_COUNT || t->rng.state == 0)
        return false;
    if (t->level < 1 || t->level > LEVEL_MAX || t->start_level < 1
            || t->start_level > t->level)
        return false;
    if (t->gravity >= 65536 || t->lock_timer > LOCK_FRAMES
            || t->lock_resets > LOCK_RESETS_MAX)
        return false;
    if (t->frames > TETRIS_MAX_FRAMES)
        return false;

    if (t->state == TETRIS_FALLING) {
        /* La pièce doit tenir à sa place. */
        if (!tetris_fits(t, t->type, t->rot, t->x, t->y))
            return false;
    } else if (t->state == TETRIS_CLEARING) {
        /* Les lignes à effacer doivent être pleines. */
        if (t->clear_rows == 0 || (t->clear_rows >> BOARD_H) != 0)
            return false;
        if (t->clear_timer < 1 || t->clear_timer > CLEAR_FRAMES)
            return false;
        for (int y = 0; y < BOARD_H; y++)
            if ((t->clear_rows & (1u << y)) && !row_full(t, y))
                return false;
    } else {
        return false;
    }
    if (t->last_clear > 4)
        return false;
    return true;
}

bool save_deserialize(save_data_t *d, uint8_t const *buffer, size_t size)
{
    save_defaults(d);

    if (size < 10 || memcmp(buffer, "TTRS", 4) != 0)
        return false;

    /* Somme de contrôle sur tout sauf ses propres 4 octets. */
    reader_t check = { buffer, size, size - 4, false };
    if (get32(&check) != fnv1a(buffer, size - 4))
        return false;

    reader_t r = { buffer, size - 4, 4, false };
    if (get8(&r) != SAVE_VERSION || get8(&r) != SCORES_COUNT)
        return false;

    save_data_t loaded;
    save_defaults(&loaded);
    for (int i = 0; i < SCORES_COUNT; i++) {
        score_entry_t *e = &loaded.scores.best[i];
        e->score = get32(&r);
        e->lines = get16(&r);
        e->level = get8(&r);
    }
    loaded.scores.played = get16(&r);
    loaded.scores.total_lines = get32(&r);
    loaded.start_level = get8(&r);
    if (loaded.start_level < 1 || loaded.start_level > LEVEL_MAX)
        loaded.start_level = 1;

    bool has_game = get8(&r);
    if (r.error)
        return false;

    /* Une partie illisible est ignorée, mais les scores sont gardés. */
    if (has_game && !read_game(&r, &loaded.game)) {
        memset(&loaded.game, 0, sizeof loaded.game);
        loaded.game.state = TETRIS_OVER;
    }

    *d = loaded;
    return true;
}

//---
// Fichier
//---

bool save_load(save_data_t *d)
{
    uint8_t buffer[SAVE_MAX_SIZE];
    int size = pf_save_read(buffer, sizeof buffer);
    if (size <= 0) {
        save_defaults(d);
        return false;
    }
    return save_deserialize(d, buffer, size);
}

bool save_store(save_data_t const *d)
{
    uint8_t buffer[SAVE_MAX_SIZE];
    size_t size = save_serialize(d, buffer, sizeof buffer);
    return size > 0 && pf_save_write(buffer, size);
}
