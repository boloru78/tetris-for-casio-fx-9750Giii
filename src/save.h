/* save.h — Sauvegarde des scores et de la partie en cours.
 *
 * Le fichier TETRIS.sav est écrit dans la mémoire de stockage de la
 * calculatrice. Son format est décrit en détail dans save.c ; il est
 * indépendant du processeur (petit-boutiste, sans remplissage) pour que les
 * tests sur PC lisent exactement les mêmes octets que la calculatrice. */
#ifndef SAVE_H
#define SAVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "scores.h"
#include "tetris.h"

/* Taille maximale d'un fichier de sauvegarde, en octets. */
#define SAVE_MAX_SIZE 512

/* Tout ce qui survit à la fermeture du jeu. */
typedef struct {
    scores_t scores;
    uint8_t start_level; /* dernier niveau de départ choisi */
    tetris_t game;       /* conservée seulement si tetris_in_progress() */
} save_data_t;

/* Valeurs d'un premier lancement : aucun score, aucune partie. */
void save_defaults(save_data_t *d);

/* Conversion vers / depuis le format du fichier.
 * save_serialize() renvoie le nombre d'octets écrits (0 si [max] est trop
 * petit). save_deserialize() renvoie faux si les données sont invalides ;
 * [d] contient alors les valeurs par défaut. */
size_t save_serialize(save_data_t const *d, uint8_t *buffer, size_t max);
bool save_deserialize(save_data_t *d, uint8_t const *buffer, size_t size);

/* Lecture et écriture du fichier via la plateforme. */
bool save_load(save_data_t *d);
bool save_store(save_data_t const *d);

#endif /* SAVE_H */
