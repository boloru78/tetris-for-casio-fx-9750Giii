/* screens.c — Menu principal, choix du niveau et meilleurs scores. */
#include "app.h"
#include "gfx.h"
#include "platform.h"
#include "render.h"
#include "ui.h"
#include <stdio.h>

//---
// Menu principal
//---

screen_t screen_main(save_data_t *d)
{
    enum { RESUME, NEW_GAME, SCORES, HELP, QUIT, ACTION_COUNT };
    static char const *const labels[ACTION_COUNT] = {
        "Reprendre la partie", "Nouvelle partie", "Meilleurs scores", "Aide",
        "Quitter",
    };

    /* « Reprendre » n'apparaît que s'il y a une partie en cours. */
    char const *items[ACTION_COUNT];
    int actions[ACTION_COUNT];
    int count = 0;
    for (int a = tetris_in_progress(&d->game) ? RESUME : NEW_GAME;
            a < ACTION_COUNT; a++) {
        items[count] = labels[a];
        actions[count++] = a;
    }

    int selected = 0;
    for (;;) {
        gfx_clear();
        gfx_sprite(2, 3, &SPR_LOGO, GFX_BLACK);
        gfx_text_bold_at(17, 4, "Tetris", GFX_BLACK, ALIGN_LEFT);
        gfx_text_at(GFX_WIDTH - 2, 4, "v" APP_VERSION, GFX_BLACK, ALIGN_RIGHT);
        gfx_rect(0, 15, GFX_WIDTH - 1, 15, GFX_BLACK);
        /* Liste centrée dans la place restante sous le titre. */
        int y = 19 + (ACTION_COUNT - count) * UI_ITEM_HEIGHT / 2;
        ui_items(2, GFX_WIDTH - 3, y, items, count, selected);
        gfx_present();

        if (ui_list_input(pf_getkey(false), &selected, count) != UI_CHOOSE)
            continue;
        switch (actions[selected]) {
        case RESUME:   return SCREEN_PLAY;
        case NEW_GAME: return SCREEN_NEW_GAME;
        case SCORES:   return SCREEN_SCORES;
        case HELP:     return SCREEN_HELP;
        default:       return SCREEN_QUIT;
        }
    }
}

//---
// Choix du niveau de départ
//---

screen_t screen_new_game(save_data_t *d)
{
    int level = d->start_level;
    char text[40];

    for (;;) {
        gfx_clear();
        ui_title_bar("Nouvelle partie");
        gfx_text_at(GFX_WIDTH / 2, 13, "Niveau de départ", GFX_BLACK,
            ALIGN_CENTER);

        /* Le niveau, entre deux flèches qui disparaissent aux extrémités. */
        snprintf(text, sizeof text, "%d", level);
        gfx_text_bold_at(GFX_WIDTH / 2, 24, text, GFX_BLACK, ALIGN_CENTER);
        if (level > 1)
            gfx_text_at(GFX_WIDTH / 2 - 16, 24, "◀", GFX_BLACK, ALIGN_CENTER);
        if (level < LEVEL_MAX)
            gfx_text_at(GFX_WIDTH / 2 + 17, 24, "▶", GFX_BLACK,
                ALIGN_CENTER);

        /* Vitesse de chute du niveau, en millisecondes par ligne. */
        unsigned long ms = 65536ul * 1000 / (PF_FPS * tetris_gravity(level));
        snprintf(text, sizeof text, "Une ligne en %lu ms", ms);
        gfx_text_at(GFX_WIDTH / 2, 35, text, GFX_BLACK, ALIGN_CENTER);

        gfx_rect(0, 45, GFX_WIDTH - 1, 45, GFX_BLACK);
        gfx_text_at(GFX_WIDTH / 2, 48, "EXE : commencer", GFX_BLACK,
            ALIGN_CENTER);
        gfx_text_at(GFX_WIDTH / 2, 57, "EXIT : retour", GFX_BLACK,
            ALIGN_CENTER);
        gfx_present();

        switch (pf_getkey(false)) {
        case IN_LEFT:
        case IN_DOWN:
            if (level > 1)
                level--;
            break;
        case IN_RIGHT:
        case IN_UP:
            if (level < LEVEL_MAX)
                level++;
            break;
        case IN_EXE:
        case IN_SHIFT:
            /* Une partie en cours doit d'abord être abandonnée. */
            if (!abandon_current_game(d))
                break;
            d->start_level = level;
            start_new_game(d);
            return SCREEN_PLAY;
        case IN_EXIT:
            return SCREEN_MAIN;
        default:
            break;
        }
    }
}

//---
// Meilleurs scores
//---

screen_t screen_scores(save_data_t *d)
{
    char text[40];

    for (;;) {
        gfx_clear();
        ui_title_bar("Meilleurs scores");

        for (int i = 0; i < SCORES_COUNT; i++) {
            score_entry_t const *e = &d->scores.best[i];
            int y = 11 + i * 8;
            snprintf(text, sizeof text, "%d.", i + 1);
            gfx_text(2, y, text, GFX_BLACK);
            if (e->score == 0) {
                gfx_text_at(62, y, "-", GFX_BLACK, ALIGN_RIGHT);
                continue;
            }
            format_number(text, e->score);
            gfx_text_at(62, y, text, GFX_BLACK, ALIGN_RIGHT);
            snprintf(text, sizeof text, "%u ligne%s", (unsigned)e->lines,
                e->lines > 1 ? "s" : "");
            gfx_text_at(GFX_WIDTH - 3, y, text, GFX_BLACK, ALIGN_RIGHT);
        }

        gfx_rect(0, 52, GFX_WIDTH - 1, 52, GFX_BLACK);
        snprintf(text, sizeof text, "Parties : %u",
            (unsigned)d->scores.played);
        gfx_text(2, 55, text, GFX_BLACK);
        gfx_text_at(GFX_WIDTH - 3, 55, "F6 : effacer", GFX_BLACK,
            ALIGN_RIGHT);
        gfx_present();

        switch (pf_getkey(false)) {
        case IN_F6:
        case IN_DEL:
            if (ui_confirm("Tout effacer ?", "Les meilleurs scores",
                    "et les statistiques.", "Oui, effacer", "Non", NULL,
                    NULL)) {
                scores_reset(&d->scores);
                save_store(d);
            }
            break;
        case IN_EXIT:
        case IN_EXE:
        case IN_SHIFT:
            return SCREEN_MAIN;
        default:
            break;
        }
    }
}
