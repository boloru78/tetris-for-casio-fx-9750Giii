# Outils de développement sur PC (Linux, macOS).
# La compilation pour la calculatrice se fait avec le fxSDK : fxsdk build-fx
#
#   make test         tests de la logique du jeu
#   make sim          simulateur PC (build/tetris-sim)
#   make screenshots  captures d'écran du README (docs/images/)
#   make check        tests + vérification des textes et des ressources
#   make assets       régénère src/assets.c et assets-fx/icon.png
#   make fx           compile l'add-in (raccourci pour fxsdk build-fx)

CC ?= cc
PYTHON ?= python3
CFLAGS ?= -O1 -g
CFLAGS += -std=c11 -Wall -Wextra -Werror -Isrc -Isim
SANITIZE ?= -fsanitize=address,undefined -fno-omit-frame-pointer

GAME_SRC := src/app.c src/assets.c src/gfx.c src/help.c src/piece.c \
            src/play.c src/render.c src/rng.c src/save.c src/scores.c \
            src/screens.c src/tetris.c src/ui.c
SIM_SRC := sim/main_sim.c sim/platform_sim.c
TEST_SRC := tests/test_main.c tests/test_piece.c tests/test_tetris.c \
            tests/test_save.c tests/test_gfx.c sim/platform_sim.c
HEADERS := $(wildcard src/*.h sim/*.h tests/*.h)
SCRIPTS := $(wildcard sim/scripts/*.txt)

.PHONY: all test sim screenshots check check-texts assets check-assets fx clean

all: test sim

build/tetris-sim: $(GAME_SRC) $(SIM_SRC) $(HEADERS)
	@mkdir -p build
	$(CC) $(CFLAGS) $(SANITIZE) -o $@ $(GAME_SRC) $(SIM_SRC)

build/tests: $(GAME_SRC) $(TEST_SRC) $(HEADERS)
	@mkdir -p build
	$(CC) $(CFLAGS) $(SANITIZE) -o $@ $(GAME_SRC) $(TEST_SRC)

sim: build/tetris-sim

# Joueur automatique qui écrit les touches de la démonstration du README.
build/bot: tools/bot.c src/tetris.c src/piece.c src/rng.c $(HEADERS)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ tools/bot.c src/tetris.c src/piece.c src/rng.c

test: build/tests
	./build/tests

# Rejoue chaque script du simulateur : échoue si un texte sort de l'écran.
check-texts: build/tetris-sim
	@mkdir -p build/sim
	@for s in $(SCRIPTS); do \
	    echo "sim $$s"; \
	    ./build/tetris-sim -o build/sim $$s || exit 1; \
	done

screenshots: build/tetris-sim
	@rm -rf build/shots && mkdir -p build/shots
	./build/tetris-sim -o build/shots sim/scripts/screenshots.txt
	$(PYTHON) tools/pbm_to_png.py build/shots docs/images

assets:
	$(PYTHON) tools/gen_assets.py

check-assets:
	$(PYTHON) tools/gen_assets.py --check

check: test check-texts check-assets

fx:
	fxsdk build-fx

clean:
	rm -rf build build-fx Tetris.g1a
