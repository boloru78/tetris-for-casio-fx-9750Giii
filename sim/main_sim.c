/* main_sim.c — Point d'entrée du simulateur PC.
 *
 * Usage : tetris-sim [-s graine] [-o dossier] [-k] script.txt
 *   -s  graine du générateur de pièces (1 par défaut)
 *   -o  dossier où écrire les captures (. par défaut)
 *   -k  garder la sauvegarde d'une exécution précédente
 *       (par défaut, chaque exécution part de zéro) */
#include "app.h"
#include "platform.h"
#include "sim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    sim_options_t opt = { .out_dir = ".", .seed = 1 };
    bool keep_save = false;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-s") && i + 1 < argc)
            opt.seed = strtoul(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "-o") && i + 1 < argc)
            opt.out_dir = argv[++i];
        else if (!strcmp(argv[i], "-k"))
            keep_save = true;
        else if (argv[i][0] != '-' && !opt.script_path)
            opt.script_path = argv[i];
        else {
            fprintf(stderr, "usage : %s [-s graine] [-o dossier] [-k] "
                "script.txt\n", argv[0]);
            return 1;
        }
    }
    if (!opt.script_path) {
        fprintf(stderr, "sim : aucun script indiqué\n");
        return 1;
    }

    static char save_path[512];
    snprintf(save_path, sizeof save_path, "%s/TETRIS.sav", opt.out_dir);
    opt.save_path = save_path;
    if (!keep_save)
        remove(save_path);

    sim_start(&opt);
    pf_init();
    app_run();
    pf_quit();
    return sim_finish();
}
