/* platform_gint.c — Implémentation de platform.h pour la calculatrice.
 *
 * C'est le seul fichier (avec main.c) qui utilise gint directement. */
#include "platform.h"
#include <gint/display.h>
#include <gint/drivers/keydev.h>
#include <gint/gint.h>
#include <gint/keyboard.h>
#include <gint/rtc.h>
#include <gint/timer.h>
#include <stdio.h>
#include <string.h>

/* Fichier de sauvegarde, à la racine de la mémoire de stockage. */
#define SAVE_PATH "/TETRIS.sav"

/* Extinction automatique après 10 minutes sans toucher au clavier, comme le
 * système de la calculatrice (gint ne le fait pas tout seul). */
#define AUTO_POWEROFF_MS (10 * 60 * 1000)

/* Répétition des flèches maintenues dans les menus : délai initial, puis
 * intervalle. Pendant la partie, le jeu gère lui-même la répétition. */
#define REPEAT_FIRST_US (350 * 1000)
#define REPEAT_NEXT_US (90 * 1000)

static volatile uint32_t frames;  /* images écoulées depuis pf_init() */
static volatile int frame_flag;   /* passe à 1 à chaque image */
static uint32_t last_activity;    /* image de la dernière touche pressée */
static int timer = -1;
static void (*save_hook)(void);
static input_t pending = IN_NONE; /* touche mise de côté (voir SHIFT) */

/* Appelée par la minuterie à chaque image, sous interruption. */
static int on_frame(void)
{
    frames++;
    frame_flag = 1;
    return TIMER_CONTINUE;
}

void pf_init(void)
{
    timer = timer_configure(TIMER_ANY, 1000000 / PF_FPS, GINT_CALL(on_frame));
    if (timer >= 0)
        timer_start(timer);
    keydev_set_standard_repeats(keydev_std(), REPEAT_FIRST_US,
        REPEAT_NEXT_US);
}

void pf_quit(void)
{
    if (timer >= 0)
        timer_stop(timer);
    timer = -1;
}

void pf_present(uint32_t const *vram)
{
    /* Notre image a exactement le format de la VRAM de gint. */
    memcpy(gint_vram, vram, 128 * 64 / 8);
    dupdate();
}

uint32_t pf_time_ms(void)
{
    return (uint32_t)((uint64_t)frames * 1000 / PF_FPS);
}

uint32_t pf_entropy(void)
{
    /* Le moment exact où le joueur lance la partie est imprévisible :
     * l'horloge temps réel (128 Hz) et nos images suffisent à rendre chaque
     * suite de pièces différente. */
    rtc_time_t t;
    rtc_get_time(&t);
    return rtc_ticks() ^ (frames << 16) ^ ((uint32_t)t.month_day << 24)
        ^ ((uint32_t)t.year << 4);
}

void pf_set_save_hook(void (*hook)(void))
{
    save_hook = hook;
}

/* Sauvegarde puis éteint la calculatrice ; le jeu reprend au rallumage. */
static void power_off(void)
{
    if (save_hook)
        save_hook();
    gint_poweroff(true);
    last_activity = frames;
}

/* Sauvegarde puis ouvre le menu principal de la calculatrice : le système
 * peut fermer l'add-in si le joueur lance une autre application. */
static void os_menu(void)
{
    if (save_hook)
        save_hook();
    gint_osmenu();
    last_activity = frames;
}

static input_t translate(int key)
{
    switch (key) {
    case KEY_UP:    return IN_UP;
    case KEY_DOWN:  return IN_DOWN;
    case KEY_LEFT:  return IN_LEFT;
    case KEY_RIGHT: return IN_RIGHT;
    case KEY_EXE:   return IN_EXE;
    case KEY_SHIFT: return IN_SHIFT;
    case KEY_ALPHA: return IN_ALPHA;
    case KEY_OPTN:  return IN_OPTN;
    case KEY_F1:    return IN_F1;
    case KEY_F2:    return IN_F2;
    case KEY_F3:    return IN_F3;
    case KEY_F4:    return IN_F4;
    case KEY_F5:    return IN_F5;
    case KEY_F6:    return IN_F6;
    case KEY_EXIT:  return IN_EXIT;
    case KEY_DEL:   return IN_DEL;
    default:        return IN_OTHER;
    }
}

//---
// Menus : une touche à la fois
//---

/* Dans les menus, SHIFT valide, mais SHIFT puis AC/ON doit éteindre la
 * calculatrice comme d'habitude. On attend donc de savoir ce qui suit :
 *   - SHIFT relâchée seule : c'est bien un appui sur SHIFT ;
 *   - AC/ON pendant que SHIFT est enfoncée : extinction ;
 *   - autre touche : on rend SHIFT et on garde l'autre touche pour après. */
static input_t resolve_shift(void)
{
    for (;;) {
        frame_flag = 0;
        key_event_t e = waitevent(&frame_flag);
        if (e.type == KEYEV_UP && e.key == KEY_SHIFT)
            return IN_SHIFT;
        if (e.type != KEYEV_DOWN)
            continue;
        if (e.key == KEY_ACON) {
            power_off();
            return IN_RESUME;
        }
        pending = translate(e.key);
        return IN_SHIFT;
    }
}

input_t pf_getkey(bool timeout)
{
    if (pending != IN_NONE) {
        input_t key = pending;
        pending = IN_NONE;
        return key;
    }

    for (;;) {
        frame_flag = 0;
        key_event_t e = getkey_opt(GETKEY_REP_ARROWS, &frame_flag);

        if (e.type == KEYEV_NONE) {
            if ((uint64_t)(frames - last_activity) * 1000 / PF_FPS
                    >= AUTO_POWEROFF_MS) {
                power_off();
                return IN_RESUME;
            }
            if (timeout)
                return IN_NONE;
            continue;
        }

        last_activity = frames;
        switch (e.key) {
        case KEY_MENU:
            os_menu();
            return IN_RESUME;
        case KEY_SHIFT:
            return resolve_shift();
        default:
            return translate(e.key);
        }
    }
}

//---
// Partie : une image à la fois
//---

/* Touches dont on suit l'état enfoncé / relâché pendant la partie. */
static int const PLAY_KEYS[] = {
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_EXE, KEY_SHIFT, KEY_ALPHA,
    KEY_OPTN, KEY_EXIT,
};

void pf_frame(pf_keys_t *keys)
{
    keys->pressed = 0;
    keys->resumed = false;

    /* On traite les touches au fil de l'eau jusqu'à l'image suivante. Le
     * processeur dort entre deux événements. */
    for (;;) {
        key_event_t e = waitevent(&frame_flag);
        if (e.type == KEYEV_NONE)
            break;
        if (e.type != KEYEV_DOWN)
            continue;

        last_activity = frames;
        if (e.key == KEY_MENU) {
            os_menu();
            keys->resumed = true;
        } else if (e.key == KEY_ACON && keydown(KEY_SHIFT)) {
            /* SHIFT a déjà fait tourner la pièce : sans conséquence, la
             * partie reprend en pause au rallumage. */
            power_off();
            keys->resumed = true;
        } else {
            keys->pressed |= PF_BIT(translate(e.key));
        }
    }
    frame_flag = 0;

    /* Tous les événements ont été lus : keydown() est à jour. */
    keys->held = 0;
    for (size_t i = 0; i < sizeof PLAY_KEYS / sizeof *PLAY_KEYS; i++) {
        if (keydown(PLAY_KEYS[i]))
            keys->held |= PF_BIT(translate(PLAY_KEYS[i]));
    }
}

//---
// Fichier de sauvegarde
//
// Les fonctions de fichiers du système (BFile) ne peuvent pas être appelées
// pendant que gint contrôle la machine : gint_world_switch() rend
// temporairement la main au système pour exécuter read_file()/write_file().
//---

static int read_file(void *buffer, size_t max_size)
{
    FILE *fp = fopen(SAVE_PATH, "rb");
    if (!fp)
        return -1;
    size_t size = fread(buffer, 1, max_size, fp);
    fclose(fp);
    return (int)size;
}

static int write_file(void const *data, size_t size)
{
    FILE *fp = fopen(SAVE_PATH, "wb");
    if (!fp)
        return 0;
    size_t written = fwrite(data, 1, size, fp);
    int closed = (fclose(fp) == 0);
    return written == size && closed;
}

int pf_save_read(void *buffer, size_t max_size)
{
    return gint_world_switch(GINT_CALL(read_file, buffer, max_size));
}

bool pf_save_write(void const *data, size_t size)
{
    return gint_world_switch(GINT_CALL(write_file, data, size)) == 1;
}
