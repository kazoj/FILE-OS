# ============================================================
# Makefile — Simulateur d'Ordonnancement de Processus
# ============================================================
# Cibles disponibles :
#   make          → compile l'exécutable (release)
#   make debug    → compile avec symboles de débogage
#   make doc      → génère la documentation Doxygen
#   make install  → installe dans /usr/local/bin ou $HOME/.local/bin
#   make clean    → supprime les fichiers compilés
#   make distclean→ clean + supprime docs/
# ============================================================

CC       = gcc
CFLAGS   = -Wall -Wextra -Wpedantic -std=c11 -Iinclude
RELFLAGS = -O2 -DNDEBUG
DBGFLAGS = -g3 -DDEBUG -fsanitize=address,undefined
LDFLAGS  =

# ---- Détection GTK4 (optionnel — active la fenêtre desktop -G) ----
GTK4_CFLAGS := $(shell pkg-config --cflags gtk4 2>/dev/null || echo "")
GTK4_LIBS   := $(shell pkg-config --libs   gtk4 2>/dev/null || echo "")
GTK4_OK     := $(shell printf '\#include <gtk/gtk.h>\nint main(void){return 0;}' \
                 | $(CC) -x c - $(GTK4_CFLAGS) $(GTK4_LIBS) -o /dev/null \
                 2>/dev/null && echo 1 || echo 0)
ifeq ($(GTK4_OK),1)
    CFLAGS  += $(GTK4_CFLAGS) -DHAVE_GTK4
    LDFLAGS += $(GTK4_LIBS)
    $(info [gtk4]    détecté — option -G activée)
else
    $(info [gtk4]    non détecté — option -G désactivée (brew install gtk4))
endif

# Répertoires
SRC_DIR   = src
INC_DIR   = include
BUILD_DIR = build
DOC_DIR   = docs

# Sources
SRCS = $(SRC_DIR)/main.c             \
       $(SRC_DIR)/process.c          \
       $(SRC_DIR)/queue.c            \
       $(SRC_DIR)/scheduler.c        \
       $(SRC_DIR)/simulation.c       \
       $(SRC_DIR)/metrics.c          \
       $(SRC_DIR)/input.c            \
       $(SRC_DIR)/output.c           \
       $(SRC_DIR)/algorithms/fifo.c  \
       $(SRC_DIR)/algorithms/sjf.c   \
       $(SRC_DIR)/algorithms/srjf.c  \
       $(SRC_DIR)/algorithms/round_robin.c

# output_gtk.c compilé seulement si GTK4 est détecté
ifeq ($(GTK4_OK),1)
    SRCS += $(SRC_DIR)/output_gtk.c
endif

OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

TARGET = scheduler

# ---- Cible par défaut ----
.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(RELFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "Exécutable : $(TARGET)"

# ---- Règle de compilation (gère les sous-répertoires) ----
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(RELFLAGS) -c $< -o $@

# ---- Build debug ----
.PHONY: debug
debug: RELFLAGS = $(DBGFLAGS)
debug: $(TARGET)
	@echo "Build debug : $(TARGET)"

# ---- Documentation Doxygen ----
.PHONY: doc
doc:
	@which doxygen > /dev/null 2>&1 || \
	    (echo "Erreur : doxygen n'est pas installé." && exit 1)
	doxygen Doxyfile
	@echo "Documentation générée dans $(DOC_DIR)/"

# ---- Installation ----
.PHONY: install
install: $(TARGET)
	@if [ -w /usr/local/bin ]; then \
	    install -m 755 $(TARGET) /usr/local/bin/$(TARGET); \
	    echo "Installé dans /usr/local/bin/$(TARGET)"; \
	else \
	    mkdir -p $(HOME)/.local/bin; \
	    install -m 755 $(TARGET) $(HOME)/.local/bin/$(TARGET); \
	    echo "Installé dans $(HOME)/.local/bin/$(TARGET)"; \
	    echo "Assurez-vous que $(HOME)/.local/bin est dans votre PATH."; \
	fi

# ---- Graphiques (nécessite python3 + matplotlib) ----
.PHONY: plot
plot:
	@if [ -z "$(CSV)" ]; then \
	    echo "Usage : make plot CSV=<fichier.csv>"; \
	else \
	    python3 tools/plot_results.py "$(CSV)"; \
	fi

# ---- Nettoyage ----
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(TARGET)
	@echo "Nettoyage terminé."

.PHONY: distclean
distclean: clean
	rm -rf $(DOC_DIR)
	@echo "Nettoyage complet (build + docs)."

# ---- Affichage d'aide ----
.PHONY: help
help:
	@echo "Cibles disponibles :"
	@echo "  all       - Compiler l'exécutable (défaut)"
	@echo "  debug     - Compiler avec symboles de débogage + sanitizers"
	@echo "  doc       - Générer la documentation Doxygen"
	@echo "  install   - Installer l'exécutable"
	@echo "  plot      - Générer les graphiques PNG : make plot CSV=<fichier.csv>"
	@echo "  clean     - Supprimer les fichiers compilés"
	@echo "  distclean - Supprimer build + docs"
	@echo "  help      - Afficher cette aide"
