/* app.c — Enchaînement des écrans du jeu. */
#include "app.h"
#include "help.h"
#include "platform.h"
#include "ui.h"

/* Données sauvegardées, partagées par tous les écrans. */
static save_data_t data;

/* Sauvegarde d'urgence avant la touche MENU ou une extinction. */
static void save_hook(void)
{
    save_store(&data);
}

void start_new_game(save_data_t *d)
{
    /* La suite des pièces dépend du moment où le joueur lance la partie. */
    tetris_new(&d->game, d->start_level, pf_entropy());
}

bool abandon_current_game(save_data_t *d)
{
    tetris_t *t = &d->game;
    if (!tetris_in_progress(t))
        return true;
    if (!ui_confirm("Abandonner ?", "La partie s'arrête,", "son score compte.",
            "Oui, abandonner", "Non", NULL, NULL))
        return false;

    scores_record(&d->scores, t->score, t->lines, t->level);
    t->state = TETRIS_OVER;
    save_store(d);
    return true;
}

void app_run(void)
{
    save_load(&data);
    pf_set_save_hook(save_hook);

    screen_t screen = SCREEN_MAIN;
    while (screen != SCREEN_QUIT) {
        switch (screen) {
        case SCREEN_MAIN:     screen = screen_main(&data); break;
        case SCREEN_NEW_GAME: screen = screen_new_game(&data); break;
        case SCREEN_PLAY:     screen = screen_play(&data); break;
        case SCREEN_SCORES:   screen = screen_scores(&data); break;
        case SCREEN_HELP:     help_show(); screen = SCREEN_MAIN; break;
        default:              screen = SCREEN_QUIT; break;
        }
    }

    save_store(&data);
    pf_set_save_hook(NULL);
}
