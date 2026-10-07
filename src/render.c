/* render.c — Dessin de l'écran de jeu. */
#include "render.h"
#include "gfx.h"
#include <stdio.h>

/* Bords des panneaux et des murs (voir le schéma de render.h). */
#define LEFT_PANEL_RIGHT 44
#define WALL_LEFT 47
#define WALL_RIGHT 80
#define WELL_BOTTOM 63
#define RIGHT_PANEL_CENTER 105

void format_number(char *buffer, uint32_t value)
{
    char digits[12];
    int n = snprintf(digits, sizeof digits, "%lu", (unsigned long)value);
    int j = 0;
    for (int i = 0; i < n; i++) {
        if (i > 0 && (n - i) % 3 == 0)
            buffer[j++] = ' ';
        buffer[j++] = digits[i];
    }
    buffer[j] = 0;
}

void format_duration(char *buffer, uint32_t frames)
{
    unsigned long s = frames / 60;
    if (s >= 3600)
        snprintf(buffer, FORMAT_NUMBER_SIZE, "%lu:%02lu:%02lu", s / 3600,
            s / 60 % 60, s % 60);
    else
        snprintf(buffer, FORMAT_NUMBER_SIZE, "%lu:%02lu", s / 60, s % 60);
}

//---
// Panneaux
//---

/* Un intitulé, et sa valeur alignée à droite sur la ligne du dessous. */
static void stat(int y, char const *label, char const *value)
{
    gfx_text(1, y, label, GFX_BLACK);
    gfx_text_at(LEFT_PANEL_RIGHT, y + 9, value, GFX_BLACK, ALIGN_RIGHT);
}

void render_stats(tetris_t const *t)
{
    char text[FORMAT_NUMBER_SIZE];

    /* Les très grands scores perdent leurs espaces pour tenir. */
    format_number(text, t->score);
    if (gfx_text_width(text) > LEFT_PANEL_RIGHT)
        snprintf(text, sizeof text, "%lu", (unsigned long)t->score);
    stat(1, "Score", text);

    snprintf(text, sizeof text, "%u", (unsigned)t->lines);
    stat(22, "Lignes", text);

    snprintf(text, sizeof text, "%u", (unsigned)t->level);
    stat(43, "Niveau", text);
}

/* Pièce en miniature (orientation d'apparition), centrée en [cx] dans une
 * case de 2 lignes de haut commençant en [y]. Une pièce [hollow] est
 * dessinée en creux : la réserve déjà utilisée. */
static void draw_mini(int type, int cx, int y, bool hollow)
{
    cell_t c[4];
    piece_cells(type, 0, c);
    int min_x = 3, max_x = 0, min_y = 3, max_y = 0;
    for (int i = 0; i < 4; i++) {
        if (c[i].x < min_x) min_x = c[i].x;
        if (c[i].x > max_x) max_x = c[i].x;
        if (c[i].y < min_y) min_y = c[i].y;
        if (c[i].y > max_y) max_y = c[i].y;
    }
    int w = (max_x - min_x + 1) * CELL_SIZE;
    int h = (max_y - min_y + 1) * CELL_SIZE;
    int x0 = cx - w / 2, y0 = y + (2 * CELL_SIZE - h) / 2;

    for (int i = 0; i < 4; i++) {
        int px = x0 + (c[i].x - min_x) * CELL_SIZE;
        int py = y0 + (c[i].y - min_y) * CELL_SIZE;
        if (hollow)
            gfx_pixel(px + 1, py + 1, GFX_BLACK);
        else
            gfx_rect(px, py, px + CELL_SIZE - 1, py + CELL_SIZE - 1,
                GFX_BLACK);
    }
}

void render_queue(tetris_t const *t)
{
    gfx_text_at(RIGHT_PANEL_CENTER, 1, "Suite", GFX_BLACK, ALIGN_CENTER);
    for (int i = 0; i < NEXT_COUNT; i++)
        draw_mini(t->next[i], RIGHT_PANEL_CENTER, 10 + i * 9, false);

    gfx_text_at(RIGHT_PANEL_CENTER, 43, "Réserve", GFX_BLACK, ALIGN_CENTER);
    if (t->hold != PIECE_NONE)
        draw_mini(t->hold, RIGHT_PANEL_CENTER, 53, t->hold_used);
}

//---
// Plateau
//---

void render_well(void)
{
    gfx_rect(WALL_LEFT, 0, WALL_LEFT, WELL_BOTTOM, GFX_BLACK);
    gfx_rect(WALL_RIGHT, 0, WALL_RIGHT, WELL_BOTTOM, GFX_BLACK);
    gfx_rect(WALL_LEFT, WELL_BOTTOM, WALL_RIGHT, WELL_BOTTOM, GFX_BLACK);
}

/* Coin haut-gauche de la case (x, y) du plateau, à l'écran. */
static int cell_px(int x) { return BOARD_X + x * CELL_SIZE; }
static int cell_py(int y) { return BOARD_Y + (y - BOARD_HIDDEN) * CELL_SIZE; }

static void draw_cell(int x, int y)
{
    if (y < BOARD_HIDDEN)
        return;
    int px = cell_px(x), py = cell_py(y);
    gfx_rect(px, py, px + CELL_SIZE - 1, py + CELL_SIZE - 1, GFX_BLACK);
}

/* Pièce fantôme : seulement le contour de la pièce, là où elle tomberait. */
static void draw_ghost(tetris_t const *t, int ghost_y)
{
    cell_t c[4];
    piece_cells(t->type, t->rot, c);

    for (int i = 0; i < 4; i++) {
        int x = t->x + c[i].x, y = ghost_y + c[i].y;
        if (y < BOARD_HIDDEN)
            continue;
        /* Un côté n'est dessiné que s'il ne touche pas une autre case de
         * la pièce. */
        bool up = true, down = true, left = true, right = true;
        for (int j = 0; j < 4; j++) {
            int dx = c[j].x - c[i].x, dy = c[j].y - c[i].y;
            if (dx == 0 && dy == -1) up = false;
            if (dx == 0 && dy == 1) down = false;
            if (dx == -1 && dy == 0) left = false;
            if (dx == 1 && dy == 0) right = false;
        }
        int x1 = cell_px(x), y1 = cell_py(y);
        int x2 = x1 + CELL_SIZE - 1, y2 = y1 + CELL_SIZE - 1;
        if (up) gfx_rect(x1, y1, x2, y1, GFX_BLACK);
        if (down) gfx_rect(x1, y2, x2, y2, GFX_BLACK);
        if (left) gfx_rect(x1, y1, x1, y2, GFX_BLACK);
        if (right) gfx_rect(x2, y1, x2, y2, GFX_BLACK);
    }
}

void render_board(tetris_t const *t)
{
    /* Pendant l'effacement, les lignes complètes clignotent. */
    bool blink_off = t->state == TETRIS_CLEARING
        && (t->clear_timer / 4) % 2 == 1;

    for (int y = BOARD_HIDDEN; y < BOARD_H; y++) {
        if (blink_off && (t->clear_rows & (1u << y)))
            continue;
        for (int x = 0; x < BOARD_W; x++) {
            if (t->cells[y][x])
                draw_cell(x, y);
        }
    }

    if (t->state == TETRIS_FALLING) {
        int ghost_y = tetris_ghost_y(t);
        if (ghost_y != t->y)
            draw_ghost(t, ghost_y);

        cell_t c[4];
        piece_cells(t->type, t->rot, c);
        for (int i = 0; i < 4; i++)
            draw_cell(t->x + c[i].x, t->y + c[i].y);
    }

    if (t->state == TETRIS_CLEARING && t->last_clear == 4)
        render_board_message("Tetris");
}

void render_board_message(char const *text)
{
    int cx = cell_px(BOARD_W / 2), cy = cell_py(BOARD_HIDDEN + 10);
    int w = gfx_text_width(text) + 6;
    int x1 = cx - w / 2, x2 = x1 + w - 1;
    int y1 = cy - FONT_HEIGHT / 2 - 3, y2 = y1 + FONT_HEIGHT + 5;
    gfx_rect(x1, y1, x2, y2, GFX_WHITE);
    gfx_frame(x1, y1, x2, y2, GFX_BLACK);
    gfx_text_at(cx, y1 + 3, text, GFX_BLACK, ALIGN_CENTER);
}

void render_game(tetris_t const *t)
{
    gfx_clear();
    render_stats(t);
    render_queue(t);
    render_well();
    render_board(t);
}
