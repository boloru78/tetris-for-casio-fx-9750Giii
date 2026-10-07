/* render.h — Dessin de l'écran de jeu : plateau, score, pièces suivantes.
 *
 * Disposition de l'écran (128×64 pixels) :
 *
 *   x : 0        45 47 48      79 80 82        127
 *       | score  |  |  | plateau  |  |  | suivantes |
 *       | lignes |  mur  10 × 3 px  mur  | réserve   |
 *       | niveau |                       |           |
 *
 * Le plateau montre ses 20 lignes visibles avec des cases de 3×3 pixels,
 * de y = 2 à y = 61 ; le fond du puits est en y = 63. */
#ifndef RENDER_H
#define RENDER_H

#include <stdbool.h>
#include <stdint.h>
#include "tetris.h"

/* Taille d'une case et position de la case (0, BOARD_HIDDEN). */
#define CELL_SIZE 3
#define BOARD_X 49
#define BOARD_Y 2

/* Panneau de gauche : score, lignes et niveau. */
void render_stats(tetris_t const *t);

/* Panneau de droite : pièces suivantes et réserve. */
void render_queue(tetris_t const *t);

/* Murs et fond du puits. */
void render_well(void);

/* Contenu du plateau : cases, pièce fantôme et pièce qui tombe. */
void render_board(tetris_t const *t);

/* Écran de jeu complet (efface l'écran, ne l'affiche pas). */
void render_game(tetris_t const *t);

/* Petit cadre blanc au milieu du plateau, avec un texte (compte à rebours,
 * annonce d'un Tetris…). */
void render_board_message(char const *text);

/* Écrit un nombre avec une espace entre les milliers (« 12 340 ») dans un
 * tampon d'au moins FORMAT_NUMBER_SIZE octets. */
#define FORMAT_NUMBER_SIZE 16
void format_number(char *buffer, uint32_t value);

/* Écrit une durée sous la forme « 12:34 » (ou « 1:02:03 » au-delà d'une
 * heure) dans un tampon d'au moins FORMAT_NUMBER_SIZE octets. */
void format_duration(char *buffer, uint32_t frames);

#endif /* RENDER_H */
