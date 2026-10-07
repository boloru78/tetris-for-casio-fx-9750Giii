# Notes pour Claude Code

Ce fichier est lu automatiquement par Claude Code au début de chaque session
ouverte dans ce dépôt. Il résume le projet, les choix déjà faits avec
l'auteur et ce qu'il reste à faire. **Mettez à jour la section « État actuel »
à la fin de chaque session.**

## Le projet

Un Tetris pour la calculatrice **Casio fx-9750GIII**, écrit en C avec gint et
le fxSDK, livré sous forme d'add-in `Tetris.g1a`. Il reprend la base du
démineur du même auteur
([minesweeper-for-casio-fx-9750Giii](https://github.com/boloru78/minesweeper-for-casio-fx-9750Giii)) :
police, interface, simulateur, outils et CI. Le README détaille les
fonctionnalités, l'installation et l'organisation du code.

## L'auteur et ses préférences

- Auteur : **boloru78**, sur **macOS** (Mac Intel), avec l'application Claude
  Desktop.
- But : jouer, et montrer le projet (portfolio, communauté Planète Casio et
  Cemetech). Le dépôt est **public**, sous **licence MIT**.
- **Tout est en français** : README, commentaires, textes du jeu, messages de
  commit, et les réponses de Claude. Les identifiants du code restent en
  anglais.
- Claude écrit le code ; l'auteur teste sur sa calculatrice et fait ses
  retours. Il débute avec Git et le Terminal : expliquer les commandes pas à
  pas.
- Seule la fx-9750GIII est visée.

## Choix déjà faits (ne pas les remettre en cause sans demander)

- Le nom « Tetris » a été choisi par l'auteur, en connaissance de cause :
  c'est une marque de The Tetris Company. Le README le précise. Si une
  demande de retrait arrive, il faudra renommer le jeu.
- Plateau vertical de 10 × 20 cases de 3×3 pixels, calculatrice tenue
  normalement. Score à gauche, 3 pièces suivantes et réserve à droite.
- Règles des versions récentes : rotation SRS avec décalages, sac de 7,
  pièce fantôme, réserve, chute immédiate et douce, délai au sol de 0,5 s
  relancé au plus 15 fois. 15 niveaux (formule de gravité officielle), un
  niveau toutes les 10 lignes, niveau de départ au choix. Points : 100, 300,
  500, 800 × niveau ; 1 par ligne en chute douce, 2 en chute immédiate.
  Volontairement absents pour l'instant : T-spins, combos, back-to-back,
  modes Sprint et Ultra.
- Commandes : ◀ ▶ déplacer (répétition après 200 ms, puis toutes les 50 ms) ;
  ▼ chute douce ; ▲ ou EXE chute immédiate ; SHIFT tourne à droite, ALPHA à
  gauche ; OPTN ou F1 réserve ; EXIT pause ; MENU menu Casio (avec
  sauvegarde).
- Boucle en temps réel à 60 images par seconde : `pf_frame()` dans
  `src/platform_gint.c` lit le clavier avec `waitevent()` jusqu'à l'image
  suivante, puis `keydown()`. Dans les menus, `pf_getkey()` garde le
  fonctionnement du démineur (SHIFT agit au relâchement pour SHIFT puis
  AC/ON). Pendant la partie, SHIFT tourne dès l'appui ; SHIFT puis AC/ON
  éteint quand même (la partie reprend en pause).
- Compte à rebours 3, 2, 1 au début et à chaque reprise. La pause cache le
  plateau et les pièces suivantes. Une partie abandonnée s'arrête et son
  score compte.
- Toute la logique (`tetris.c`) avance image par image et est déterministe :
  tests et captures en dépendent. Les captures du README sont jouées par
  `tools/bot.c` ; après un changement des règles, régénérer les touches
  (voir l'en-tête de `sim/scripts/screenshots.txt`).
- La police (avec accents), le logo et l'icône sont dessinés en texte dans
  `assets/`. `tools/gen_assets.py` produit `src/assets.c`, à ne jamais
  modifier à la main.
- Le nom affiché dans le menu Casio est « Tetris » (8 caractères au plus).

## Commandes utiles

```sh
make check          # tests + parcours des écrans + ressources à jour (comme la CI)
make test           # tests de la logique seulement
make screenshots    # régénère docs/images/ (après tout changement visuel)
make assets         # après une modification de assets/*.txt
```

Pillow est installé dans un environnement Python à côté du projet : ajouter
`PYTHON=../.venv/bin/python` aux commandes `make` qui en ont besoin
(`screenshots`, `assets`, `check`).

Docker n'est pas installé sur le Mac de l'auteur : l'add-in est compilé par
la CI. `gh` (GitHub CLI) est installé et connecté au compte boloru78 :
`gh run download` récupère l'artefact `Tetris-g1a`.

## Conventions

- C11, indentation de 4 espaces, commentaires en français.
- Compilation avec `-Wall -Wextra -Werror`, sur la calculatrice comme sur PC.
- Toute chaîne affichée doit tenir dans 128 pixels : `make check-texts` le
  vérifie. Pour un nouvel écran ou dialogue, l'ajouter à
  `sim/scripts/tour.txt`.
- Pour changer de version : `APP_VERSION` dans `src/app.h`, puis `project()`
  et `VERSION` (format `MM.mm.pppp`) dans `CMakeLists.txt`, puis un tag
  `vX.Y.Z`.

## Transférer le jeu sur la calculatrice (macOS)

La calculatrice se branche en USB et se met en mode **USB Flash** (F1). Elle
apparaît dans `/Volumes/NO NAME`. Copier avec `cp -X`, puis `dot_clean -m`,
supprimer `.fseventsd` juste avant d'éjecter avec `diskutil eject`.

**Attention :** le dossier `@MainMem` du disque USB est la **mémoire
principale** de la calculatrice (programmes Basic, listes, réglages). Ne
jamais y toucher.

## Historique

- **07/10/2026** : création du projet, dans la même session que les premiers
  essais du démineur sur la calculatrice. Logique, tests (3 340
  vérifications), simulateur, joueur automatique, captures, README et CI.
  Testé seulement dans le simulateur : la partie gint n'a pas encore été
  compilée.

## État actuel (07/10/2026)

- Le dépôt GitHub est créé et public, mais **sans le fichier de la CI**
  (`.github/workflows/build.yml`, gardé sur le Mac, non suivi par Git) : le
  jeton de `gh` n'a pas la permission `workflow`, et GitHub refuse alors
  l'envoi de ce fichier. L'add-in n'a donc pas encore été compilé.
- `platform_gint.c` a été vérifié avec les vrais en-têtes de gint 2.11
  (`clang -fsyntax-only`) : seule différence, `GINT_CALL()` avec un `size_t`,
  qui ne passe que sur un Mac 64 bits (même code que le démineur).
- Pas encore testé sur la calculatrice.

## Prochaines étapes

1. Quand l'auteur est devant son ordinateur : `gh auth refresh -h github.com
   -s workflow` (code à valider dans le navigateur, valable 15 minutes), puis
   `git add .github/workflows/build.yml`, commit et push. Vérifier que la CI
   compile `Tetris.g1a` (première compilation avec gint).
2. Copier `Tetris.g1a` sur la calculatrice et tester en priorité :
   - la fluidité et la réactivité des touches (régler `DAS_FRAMES` et
     `ARR_FRAMES` dans `src/tetris.h` si besoin) ;
   - tourner en se déplaçant (SHIFT et une flèche en même temps) ;
   - MENU pendant une partie, puis retour : la partie reprend en pause ;
   - SHIFT puis AC/ON pendant une partie ;
   - la lisibilité des cases de 3 pixels et de la pièce fantôme.
3. Mettre à jour la taille réelle de `Tetris.g1a` dans le README.
4. Créer le tag `v1.0.0`.
