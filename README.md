# Simulateur d'Ordonnancement de Processus

Projet universitaire de L3 Informatique — Simulation en mode console d'algorithmes d'ordonnancement de processus, écrit en C.

---

## Présentation

Le simulateur reproduit le comportement d'un noyau de système d'exploitation vis-à-vis de l'ordonnancement. Il prend en entrée un ensemble de processus (avec des phases CPU et des phases d'E/S), les fait s'exécuter selon un algorithme choisi, et produit des métriques de performance ainsi que des graphiques.

La simulation avance **milliseconde par milliseconde**. À chaque tick, le moteur gère les arrivées, les E/S, les préemptions, le choix du prochain processus et l'enregistrement du diagramme de Gantt.

---

## Algorithmes disponibles

| Clé | Nom complet | Préemptif |
|---|---|---|
| `fifo` | First In First Out | Non |
| `sjf` | Shortest Job First | Non |
| `srjf` | Shortest Remaining Job First | Oui |
| `rr` | Round Robin | Oui |

L'architecture repose sur une **vtable de pointeurs de fonctions** : ajouter un nouvel algorithme se fait en créant un seul fichier `.c`, sans modifier le reste du code.

---

## Compilation

**Prérequis :** `gcc` >= 9, `make`. Optionnels : `gtk4` (IHM desktop), `doxygen` (documentation), `python3` + `matplotlib` (graphiques PNG).

```bash
make          # build de production
make debug    # build avec AddressSanitizer et symboles GDB
make install  # installe dans /usr/local/bin ou $HOME/.local/bin
```

---

## Utilisation rapide

### Processus en ligne de commande

Chaque argument positionnel est un processus, au format `"cpu_ms,io_ms,cpu_ms,..."` :

```bash
# FIFO, 3 processus purement CPU
./scheduler -a fifo "200" "300" "150"

# SRJF avec E/S, diagramme de Gantt ASCII
./scheduler -a srjf -g "200,50,100" "300" "150,30,50"

# Round Robin, quantum 100 ms
./scheduler -a rr -q 100 "200,50,100" "300"
```

### Depuis un fichier `.sim`

```bash
./scheduler -a fifo -g -f tests/sample_inputs/basic.sim
./scheduler -a rr -q 50 -g -P -f tests/sample_inputs/io_heavy.sim
```

---

## Options principales

| Option | Description |
|---|---|
| `-a <algo>` | Algorithme (`fifo`, `sjf`, `srjf`, `rr`) — **obligatoire** |
| `-f <fichier>` | Charger les processus depuis un fichier `.sim` |
| `-q <ms>` | Quantum pour Round Robin (défaut : 50 ms) |
| `-g` | Afficher le diagramme de Gantt ASCII |
| `-G` | IHM desktop GTK4 (nécessite GTK4) |
| `-P` | Générer les graphiques PNG (nécessite Python + matplotlib) |
| `-S` | E/S séquentielles (non parallélisables) |
| `-o <fichier>` | Nom du fichier CSV de sortie |
| `-l` | Lister les algorithmes disponibles |
| `-v` | Mode verbeux |

---

## Sorties produites

- **Table de métriques** dans le terminal (temps d'attente, turnaround, temps de réponse, taux CPU)
- **Fichier CSV** généré automatiquement après chaque simulation (`<ALGO>_<timestamp>.csv`)
- **Diagramme de Gantt ASCII** avec `-g`
- **Graphiques PNG** avec `-P` (diagramme de Gantt + barres de métriques)
- **IHM desktop GTK4** avec `-G` (fenêtre native : Gantt coloré Cairo + tableau métriques)

---

## Structure du projet

```
file-OS/
├── src/
│   ├── main.c              # Point d'entrée
│   ├── simulation.c        # Moteur de simulation (boucle tick)
│   ├── scheduler.c         # Registre et création des algorithmes
│   ├── algorithms/
│   │   ├── fifo.c
│   │   ├── sjf.c
│   │   ├── srjf.c
│   │   └── round_robin.c
│   ├── process.c           # PCB et transitions d'état
│   ├── queue.c             # File FIFO et file de priorité
│   ├── metrics.c           # Calcul des indicateurs de performance
│   ├── input.c             # Lecture CLI et fichiers .sim
│   ├── output.c            # Affichage terminal et export CSV
│   └── output_gtk.c        # IHM desktop GTK4 (optionnel)
├── include/                # Headers (.h) de chaque module
├── tests/sample_inputs/    # Fichiers .sim d'exemple
├── tools/plot_results.py   # Génération de graphiques Python
├── Makefile
├── Doxyfile
└── GUIDE.md                # Documentation technique complète
```

---

## Format du fichier `.sim`

```
# Commentaire
quantum 50

process 1 "P1" arrival=0 priority=1
    burst cpu=200 io=100
    burst cpu=150
end
```

---

## Générer la documentation

```bash
make doc
# Ouvrir docs/html/index.html dans un navigateur
```

---

## Nettoyage

```bash
make clean       # supprime build/ et l'exécutable
make distclean   # clean + supprime docs/
```

---

Pour l'installation, le format complet des fichiers `.sim`, et le guide pour ajouter un algorithme, voir [GUIDE.md](GUIDE.md).
