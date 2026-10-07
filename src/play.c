/* play.c — Écran de jeu : la boucle en temps réel d'une partie. */
#include "app.h"
#include "gfx.h"
#include "help.h"
#include "platform.h"
#include "render.h"
#include "ui.h"
#include <stdio.h>

/* Durée de chaque chiffre du compte à rebours, en images. */
#define COUNTDOWN_FRAMES 30

/* Fin de partie : le plateau se remplit ligne par ligne (2 images par
 * ligne), puis reste affiché un instant avant le bilan. */
#define CURTAIN_FRAMES_PER_ROW 2
#define CURTAIN_PAUSE_FRAMES 30

#define VISIBLE_ROWS (BOARD_H - BOARD_HIDDEN)

/* Touches de la calculatrice -> boutons du jeu. */
static unsigned buttons(uint32_t keys)
{
    unsigned b = 0;
    if (keys & PF_BIT(IN_LEFT))
        b |= BTN_LEFT;
    if (keys & PF_BIT(IN_RIGHT))
        b |= BTN_RIGHT;
    if (keys & PF_BIT(IN_DOWN))
        b |= BTN_SOFT_DROP;
    if (keys & (PF_BIT(IN_UP) | PF_BIT(IN_EXE)))
        b |= BTN_HARD_DROP;
    if (keys & PF_BIT(IN_SHIFT))
        b |= BTN_ROTATE_CW;
    if (keys & PF_BIT(IN_ALPHA))
        b |= BTN_ROTATE_CCW;
    if (keys & (PF_BIT(IN_OPTN) | PF_BIT(IN_F1)))
        b |= BTN_HOLD;
    return b;
}

/* Vrai si le joueur demande la pause : EXIT, ou retour du menu Casio. */
static bool wants_pause(pf_keys_t const *k)
{
    return k->resumed || (k->pressed & PF_BIT(IN_EXIT));
}

/* Fond de la pause : le score seul. Le plateau et les pièces suivantes
 * sont cachés pour qu'on ne puisse pas réfléchir en pause. */
static void pause_background(void const *context)
{
    render_stats(context);
    render_well();
}

/* Les [rows] lignes du bas du plateau, grisées (une case sur deux). */
static void draw_curtain(int rows)
{
    for (int r = 0; r < rows && r < VISIBLE_ROWS; r++) {
        int y0 = BOARD_Y + (VISIBLE_ROWS - 1 - r) * CELL_SIZE;
        for (int y = y0; y < y0 + CELL_SIZE; y++)
            for (int x = BOARD_X; x < BOARD_X + BOARD_W * CELL_SIZE; x++)
                gfx_pixel(x, y, ((x + y) & 1) ? GFX_WHITE : GFX_BLACK);
    }
}

/* Fond du bilan de fin de partie : le plateau entièrement grisé. */
static void game_over_background(void const *context)
{
    render_stats(context);
    render_queue(context);
    render_well();
    draw_curtain(VISIBLE_ROWS);
}

/* Compte à rebours 3, 2, 1 sur le plateau, avant de commencer ou de
 * reprendre. Renvoie faux si le joueur demande la pause entre-temps. */
static bool countdown(tetris_t const *t)
{
    for (int n = 3; n >= 1; n--) {
        char text[2] = { (char)('0' + n), 0 };
        for (int f = 0; f < COUNTDOWN_FRAMES; f++) {
            render_game(t);
            render_board_message(text);
            gfx_present();
            pf_keys_t k;
            pf_frame(&k);
            if (wants_pause(&k))
                return false;
        }
    }
    return true;
}

/* Le plateau se remplit de bas en haut. */
static void game_over_animation(tetris_t const *t)
{
    int total = VISIBLE_ROWS * CURTAIN_FRAMES_PER_ROW + CURTAIN_PAUSE_FRAMES;
    for (int f = 0; f < total; f++) {
        render_game(t);
        draw_curtain(f / CURTAIN_FRAMES_PER_ROW);
        gfx_present();
        pf_keys_t k;
        pf_frame(&k);
    }
}

/* Fin de partie : scores, sauvegarde puis bilan. */
static screen_t end_of_game(save_data_t *d)
{
    tetris_t *t = &d->game;
    int rank = scores_record(&d->scores, t->score, t->lines, t->level);
    save_store(d);
    game_over_animation(t);

    char number[FORMAT_NUMBER_SIZE], line1[32], line2[32], line3[32];
    format_number(number, t->score);
    snprintf(line1, sizeof line1, "Score : %s", number);
    snprintf(line2, sizeof line2, "Lignes : %u", (unsigned)t->lines);
    if (rank == 0) {
        snprintf(line3, sizeof line3, "Nouveau record !");
    } else if (rank > 0) {
        snprintf(line3, sizeof line3, "%de meilleur score", rank + 1);
    } else {
        format_number(number, d->scores.best[0].score);
        snprintf(line3, sizeof line3, "Record : %s", number);
    }

    char const *lines[] = { line1, line2, line3 };
    char const *items[] = { "Rejouer", "Menu principal" };
    int choice = ui_dialog("Partie terminée", lines, 3, items, 2, 0,
        game_over_background, t);
    if (choice == 0) {
        start_new_game(d);
        return SCREEN_PLAY;
    }
    return SCREEN_MAIN;
}

/* Menu de pause (touche EXIT, ou retour du menu Casio). Renvoie
 * SCREEN_PLAY pour continuer à jouer, peut-être une nouvelle partie. */
static screen_t pause_menu(save_data_t *d)
{
    tetris_t *t = &d->game;
    char const *items[] = { "Reprendre", "Recommencer", "Aide",
        "Menu principal" };
    int selected = 0;

    for (;;) {
        switch (ui_dialog("Pause", NULL, 0, items, 4, selected,
                pause_background, t)) {
        case 1:
            if (abandon_current_game(d)) {
                start_new_game(d);
                return SCREEN_PLAY;
            }
            selected = 1;
            break;
        case 2:
            help_show();
            selected = 2;
            break;
        case 3:
            /* La partie reste en mémoire : « Reprendre la partie ». */
            save_store(d);
            return SCREEN_MAIN;
        default:
            return SCREEN_PLAY;
        }
    }
}

screen_t screen_play(save_data_t *d)
{
    tetris_t *t = &d->game;
    bool running = countdown(t);

    for (;;) {
        if (!running) {
            screen_t next = pause_menu(d);
            if (next != SCREEN_PLAY)
                return next;
            running = countdown(t);
            continue;
        }

        render_game(t);
        gfx_present();

        pf_keys_t k;
        pf_frame(&k);
        if (wants_pause(&k)) {
            running = false;
            continue;
        }
        tetris_step(t, buttons(k.held), buttons(k.pressed));
        if (!tetris_in_progress(t))
            return end_of_game(d);
    }
}
