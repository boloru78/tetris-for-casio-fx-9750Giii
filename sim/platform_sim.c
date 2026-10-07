/* platform_sim.c — Implémentation de platform.h pour le simulateur PC.
 *
 * Le simulateur n'a pas de fenêtre : il lit une suite de touches dans un
 * script et enregistre les écrans demandés en images PBM (voir sim/README.md
 * pour la syntaxe des scripts). Le temps est simulé et compté en images de
 * 1/60 s, ce qui rend les captures parfaitement reproductibles. */
#include "platform.h"
#include "sim.h"
#include "gfx.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Image affichée en dernier. */
static uint32_t screen[GFX_HEIGHT * 4];

static sim_options_t opt;
static char *script;          /* contenu du script */
static char *cursor;          /* position de lecture */
static uint32_t now_frames;   /* horloge simulée, en images */
static uint32_t key_delay = 15; /* durée d'un appui dans les menus, en images */
static int wait_frames;       /* images d'attente restantes (WAIT:n) */
static uint32_t held;         /* touches maintenues (+TOUCHE / -TOUCHE) */
static void (*save_hook)(void);

/* Enregistrement d'une animation (REC:n / REC:off). */
static int rec_every;         /* 0 : pas d'enregistrement */
static int rec_skip;
static int frame_count;
static FILE *frame_index;

static void write_pbm(char const *path, uint32_t const *vram)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        fprintf(stderr, "sim : impossible d'écrire %s\n", path);
        exit(1);
    }
    fprintf(fp, "P4\n%d %d\n", GFX_WIDTH, GFX_HEIGHT);
    for (int i = 0; i < GFX_HEIGHT * 4; i++) {
        uint8_t bytes[4] = { vram[i] >> 24, vram[i] >> 16, vram[i] >> 8,
            vram[i] };
        fwrite(bytes, 1, 4, fp);
    }
    fclose(fp);
}

void sim_start(sim_options_t const *options)
{
    opt = *options;

    FILE *fp = fopen(opt.script_path, "rb");
    if (!fp) {
        fprintf(stderr, "sim : script introuvable : %s\n", opt.script_path);
        exit(1);
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    script = calloc(size + 1, 1);
    if (!script || fread(script, 1, size, fp) != (size_t)size) {
        fprintf(stderr, "sim : lecture impossible : %s\n", opt.script_path);
        exit(1);
    }
    fclose(fp);
    cursor = script;
}

int sim_finish(void)
{
    if (frame_index)
        fclose(frame_index);
    frame_index = NULL;
    free(script);
    script = cursor = NULL;

    /* Un texte trop long pour l'écran est une erreur. */
    if (gfx_text_clipped > 0) {
        fprintf(stderr, "sim : %d pixels de texte hors de l'écran\n",
            gfx_text_clipped);
        return 2;
    }
    return 0;
}

void pf_init(void) {}
void pf_quit(void) {}

static void record_frame(void)
{
    char path[512];
    snprintf(path, sizeof path, "%s/frame-%05d.pbm", opt.out_dir,
        frame_count);
    write_pbm(path, screen);
    fprintf(frame_index, "frame-%05d.pbm %u\n", frame_count,
        (unsigned)pf_time_ms());
    frame_count++;
}

void pf_present(uint32_t const *vram)
{
    memcpy(screen, vram, sizeof screen);
    /* On garde une image sur [rec_every] pour que l'animation reste
     * légère. */
    if (rec_every && rec_skip-- <= 0) {
        record_frame();
        rec_skip = rec_every - 1;
    }
}

uint32_t pf_time_ms(void)
{
    return (uint32_t)((uint64_t)now_frames * 1000 / PF_FPS);
}

uint32_t pf_entropy(void)
{
    return opt.seed;
}

void pf_set_save_hook(void (*hook)(void))
{
    save_hook = hook;
}

/* Lit le prochain mot du script (en sautant blancs et commentaires). */
static bool next_token(char *token, size_t size)
{
    for (;;) {
        while (isspace((unsigned char)*cursor))
            cursor++;
        if (*cursor == '#') {
            while (*cursor && *cursor != '\n')
                cursor++;
            continue;
        }
        break;
    }
    if (!*cursor)
        return false;

    size_t n = 0;
    while (*cursor && !isspace((unsigned char)*cursor)) {
        if (n + 1 < size)
            token[n++] = *cursor;
        cursor++;
    }
    token[n] = 0;
    return true;
}

static struct { char const *name; input_t key; } const KEYS[] = {
    { "UP", IN_UP }, { "DOWN", IN_DOWN }, { "LEFT", IN_LEFT },
    { "RIGHT", IN_RIGHT }, { "EXE", IN_EXE }, { "SHIFT", IN_SHIFT },
    { "ALPHA", IN_ALPHA }, { "OPTN", IN_OPTN }, { "F1", IN_F1 },
    { "F2", IN_F2 }, { "F3", IN_F3 }, { "F4", IN_F4 }, { "F5", IN_F5 },
    { "F6", IN_F6 }, { "EXIT", IN_EXIT }, { "DEL", IN_DEL },
    { "OTHER", IN_OTHER },
};

static input_t find_key(char const *name)
{
    for (size_t i = 0; i < sizeof KEYS / sizeof *KEYS; i++) {
        if (!strcmp(name, KEYS[i].name))
            return KEYS[i].key;
    }
    return IN_NONE;
}

/* Instruction du script : ce qu'elle demande au jeu. */
typedef enum {
    STEP_KEY,    /* appui sur une touche (step.key) */
    STEP_HOLD,   /* +TOUCHE : touche enfoncée et maintenue */
    STEP_WAIT,   /* WAIT:n : n images sans nouvel appui */
    STEP_MENU,   /* MENU : sauvegarde puis retour immédiat dans le jeu */
} step_kind_t;

typedef struct {
    step_kind_t kind;
    input_t key;
} step_t;

/* Lit les instructions jusqu'à la prochaine qui concerne le jeu ; exécute
 * au passage celles qui ne concernent que le simulateur. */
static step_t next_step(void)
{
    char token[256];

    for (;;) {
        /* Fin du script : le simulateur s'arrête là. */
        if (!next_token(token, sizeof token))
            exit(sim_finish());

        if (!strncmp(token, "WAIT:", 5)) {
            wait_frames = atoi(token + 5);
            return (step_t){ STEP_WAIT, IN_NONE };
        }
        if (!strncmp(token, "SHOT:", 5)) {
            char path[512];
            snprintf(path, sizeof path, "%s/%s.pbm", opt.out_dir, token + 5);
            write_pbm(path, screen);
            continue;
        }
        if (!strncmp(token, "SEED:", 5)) {
            opt.seed = strtoul(token + 5, NULL, 0);
            continue;
        }
        if (!strncmp(token, "DELAY:", 6)) {
            key_delay = atoi(token + 6);
            continue;
        }
        if (!strncmp(token, "REC:", 4)) {
            if (!strcmp(token + 4, "off")) {
                /* Dernière image, pour que sa durée soit connue. */
                if (rec_every)
                    record_frame();
                rec_every = 0;
                continue;
            }
            char path[512];
            snprintf(path, sizeof path, "%s/frames.txt", opt.out_dir);
            if (!frame_index && !(frame_index = fopen(path, "w"))) {
                fprintf(stderr, "sim : impossible d'écrire %s\n", path);
                exit(1);
            }
            rec_every = atoi(token + 4) > 0 ? atoi(token + 4) : 1;
            rec_skip = 0;
            continue;
        }
        if (!strcmp(token, "MENU"))
            return (step_t){ STEP_MENU, IN_NONE };

        if (token[0] == '-' && find_key(token + 1) != IN_NONE) {
            held &= ~PF_BIT(find_key(token + 1));
            continue;
        }
        if (token[0] == '+' && find_key(token + 1) != IN_NONE)
            return (step_t){ STEP_HOLD, find_key(token + 1) };
        if (find_key(token) != IN_NONE)
            return (step_t){ STEP_KEY, find_key(token) };

        fprintf(stderr, "sim : instruction inconnue : %s\n", token);
        exit(1);
    }
}

input_t pf_getkey(bool timeout)
{
    for (;;) {
        if (wait_frames > 0) {
            wait_frames--;
            now_frames++;
            if (timeout)
                return IN_NONE;
            continue;
        }

        step_t step = next_step();
        switch (step.kind) {
        case STEP_WAIT:
            continue;
        case STEP_MENU:
            if (save_hook)
                save_hook();
            now_frames += key_delay;
            return IN_RESUME;
        default:
            /* Dans les menus, une touche maintenue compte comme un appui. */
            now_frames += key_delay;
            return step.key;
        }
    }
}

void pf_frame(pf_keys_t *keys)
{
    keys->pressed = 0;
    keys->resumed = false;
    now_frames++;

    if (wait_frames > 0) {
        wait_frames--;
        keys->held = held;
        return;
    }

    /* Les touches maintenues sont appliquées à l'image suivante, avec
     * l'instruction qui la remplit. */
    for (;;) {
        step_t step = next_step();
        switch (step.kind) {
        case STEP_HOLD:
            held |= PF_BIT(step.key);
            keys->pressed |= PF_BIT(step.key);
            continue;
        case STEP_WAIT:
            if (wait_frames > 0)
                wait_frames--;
            break;
        case STEP_MENU:
            if (save_hook)
                save_hook();
            keys->resumed = true;
            break;
        default:
            /* Appui bref : la touche est relâchée avant l'image suivante. */
            keys->pressed |= PF_BIT(step.key);
            break;
        }
        break;
    }
    keys->held = held;
}

int pf_save_read(void *buffer, size_t max_size)
{
    FILE *fp = fopen(opt.save_path, "rb");
    if (!fp)
        return -1;
    size_t size = fread(buffer, 1, max_size, fp);
    fclose(fp);
    return (int)size;
}

bool pf_save_write(void const *data, size_t size)
{
    FILE *fp = fopen(opt.save_path, "wb");
    if (!fp)
        return false;
    size_t written = fwrite(data, 1, size, fp);
    return (fclose(fp) == 0) && written == size;
}
