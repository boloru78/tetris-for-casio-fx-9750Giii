# Simulateur PC

Le simulateur exécute **exactement le même code de jeu** que la calculatrice
(tout le dossier `src/` sauf `main.c` et `platform_gint.c`), mais sur
ordinateur et sans fenêtre : il rejoue une suite de touches écrite dans un
script et enregistre les écrans demandés.

Il sert à trois choses :

- générer les captures d'écran et l'animation du README (`make screenshots`) ;
- vérifier automatiquement qu'aucun texte ne dépasse de l'écran
  (`make check-texts`, lancé par la CI) ;
- essayer une modification sans transférer l'add-in sur la calculatrice.

## Utilisation

```sh
make sim
./build/tetris-sim [-s graine] [-o dossier] [-k] script.txt
python3 tools/pbm_to_png.py dossier dossier-png   # captures en PNG
```

| Option | Rôle |
| --- | --- |
| `-s graine` | graine des suites de pièces (1 par défaut) ; même graine = mêmes pièces |
| `-o dossier` | où écrire les captures et la sauvegarde `TETRIS.sav` (`.` par défaut) |
| `-k` | garder la sauvegarde d'une exécution précédente (sinon on repart de zéro) |

Le programme renvoie `2` si un texte est sorti de l'écran pendant le script,
`1` en cas d'erreur dans le script, `0` sinon.

## Le temps

Le temps est simulé et compté en **images** de 1/60 s, comme la boucle de jeu
sur la calculatrice. Les captures sont donc identiques d'une exécution à
l'autre.

- **Dans les menus**, chaque touche du script est un appui, qui dure `DELAY`
  images.
- **Pendant la partie**, chaque instruction remplit une image : une touche
  est un appui bref (enfoncée puis relâchée pendant l'image), `WAIT:n` laisse
  passer `n` images. Pour maintenir une touche, on l'enfonce avec `+TOUCHE`
  et on la relâche plus tard avec `-TOUCHE`.

Au début de chaque partie, un compte à rebours dure 90 images : on commence
donc par `WAIT:90`.

## Syntaxe des scripts

Un script est une suite de mots séparés par des espaces ou des retours à la
ligne. Tout ce qui suit un `#` est un commentaire.

| Instruction | Effet |
| --- | --- |
| `UP` `DOWN` `LEFT` `RIGHT` | flèches |
| `EXE` `SHIFT` `ALPHA` `OPTN` `EXIT` `DEL` | touches du même nom |
| `F1` … `F6` | touches de fonction |
| `+TOUCHE` / `-TOUCHE` | enfonce / relâche une touche (par exemple `+DOWN WAIT:30 -DOWN`) |
| `MENU` | touche MENU : sauvegarde puis retour immédiat dans le jeu |
| `OTHER` | une touche sans effet |
| `WAIT:n` | laisse passer `n` images sans nouvel appui |
| `DELAY:n` | durée d'un appui dans les menus, en images (15 par défaut) |
| `SEED:n` | change la graine des parties suivantes |
| `SHOT:nom` | enregistre l'écran actuel dans `nom.pbm` |
| `REC:n` / `REC:off` | enregistre une image affichée sur `n` (pour `demo.gif`) |

Le simulateur s'arrête à la fin du script, ou plus tôt si le jeu se termine
(choix « Quitter »).

## Scripts fournis

- [`scripts/screenshots.txt`](scripts/screenshots.txt) : captures et
  animation du README. Les touches des parties y sont écrites par le joueur
  automatique [`tools/bot.c`](../tools/bot.c) (`make build/bot`).
- [`scripts/tour.txt`](scripts/tour.txt) : passe par tous les écrans et toutes
  les boîtes de dialogue (vérification des textes).
