# Guide d'installation et d'utilisation
# Simulateur d'Ordonnancement de Processus

> Destiné à un programmeur système souhaitant compiler, installer et exploiter le simulateur en environnement UNIX/Linux ou macOS.

---

## Table des matières

1. [Prérequis](#1-prérequis)
2. [Compilation](#2-compilation)
3. [Installation](#3-installation)
4. [Utilisation rapide](#4-utilisation-rapide)
5. [Options de la ligne de commande](#5-options-de-la-ligne-de-commande)
6. [Format du fichier `.sim`](#6-format-du-fichier-sim)
7. [Algorithmes disponibles](#7-algorithmes-disponibles)
8. [Sorties produites](#8-sorties-produites)
9. [Génération des graphiques](#9-génération-des-graphiques)
10. [Documentation Doxygen](#10-documentation-doxygen)
11. [Ajouter un algorithme](#11-ajouter-un-algorithme)
12. [Nettoyage](#12-nettoyage)

---

## 1. Prérequis

| Composant | Rôle | Obligatoire |
|---|---|---|
| `gcc` ≥ 9 | Compilation C11 | Oui |
| `make` | Système de build | Oui |
| `libncurses-dev` | IHM interactive (`-i`) | Non |
| `doxygen` | Génération de la doc | Non |
| `python3` + `matplotlib` | Graphiques PNG (`-P`) | Non |

**Debian / Ubuntu :**
```bash
sudo apt install gcc make libncurses-dev doxygen python3-matplotlib
```

**macOS (Homebrew) :**
```bash
brew install gcc make ncurses doxygen python3
pip3 install matplotlib
```

---

## 2. Compilation

### Build de production (optimisé)

```bash
cd file-OS/
make
```

L'exécutable `scheduler` est produit à la racine du projet.

Le Makefile détecte automatiquement la présence de ncurses et l'active si disponible :

```
[ncurses] détecté — option -i activée
Exécutable : scheduler
```

### Build de débogage

Active les symboles GDB, AddressSanitizer et UndefinedBehaviorSanitizer.

```bash
make debug
```

---

## 3. Installation

```bash
make install
```

Le Makefile choisit automatiquement la destination :

- Si `/usr/local/bin` est accessible en écriture → installe dans `/usr/local/bin/scheduler`
- Sinon → installe dans `$HOME/.local/bin/scheduler`

> **Note :** Si l'installation atterrit dans `$HOME/.local/bin`, vérifiez que ce répertoire est dans votre `PATH` :
> ```bash
> echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
> source ~/.bashrc
> ```

Vérification :
```bash
scheduler -l
```

---

## 4. Utilisation rapide

### Cas simple : processus en ligne de commande

Chaque argument positionnel représente un processus. Le format d'un argument est :

```
"<cpu_ms>,<io_ms>,<cpu_ms>,..."
```

Exemples :

```bash
# 3 processus purement CPU, algorithme FIFO
./scheduler -a fifo "200" "300" "150"

# 3 processus avec E/S, algorithme SRJF, Gantt ASCII
./scheduler -a srjf -g "200,50,100" "300" "150,30,50"

# Round Robin, quantum 100 ms
./scheduler -a rr -q 100 "200,50,100" "300"
```

### Cas complet : fichier `.sim`

```bash
# Jeu de test fourni (3 processus sans E/S)
./scheduler -a fifo -g -f tests/sample_inputs/basic.sim

# Jeu de test avec E/S, Gantt + graphiques PNG
./scheduler -a rr -g -P -f tests/sample_inputs/io_heavy.sim

# IHM ncurses interactive (si compilé avec ncurses)
./scheduler -a rr -i -f tests/sample_inputs/io_heavy.sim
```

---

## 5. Options de la ligne de commande

```
scheduler -a <algo> [options] [<burst_spec>...]
scheduler -a <algo> [options] -f <fichier.sim>
```

| Option | Argument | Description |
|---|---|---|
| `-a <algo>` | `fifo` `sjf` `srjf` `rr` | **Obligatoire.** Algorithme d'ordonnancement. |
| `-f <fichier>` | chemin `.sim` | Charger les processus depuis un fichier. |
| `-q <quantum>` | entier (ms) | Quantum pour Round Robin (défaut : `50`). |
| `-o <fichier>` | chemin `.csv` | Nom du fichier CSV de sortie (défaut : auto). |
| `-g` | — | Afficher le diagramme de Gantt ASCII. |
| `-i` | — | IHM ncurses interactive (nécessite ncurses). |
| `-P` | — | Générer les graphiques PNG (nécessite Python + matplotlib). |
| `-S` | — | Mode E/S séquentielles (non parallélisables). |
| `-v` | — | Mode verbeux (liste les processus chargés). |
| `-l` | — | Lister les algorithmes disponibles et quitter. |
| `-h` | — | Afficher l'aide et quitter. |

> Le quantum spécifié dans un fichier `.sim` (directive `quantum`) prend la priorité sur `-q` si `-q` n'est pas explicitement passé.

---

## 6. Format du fichier `.sim`

Les fichiers `.sim` sont des fichiers texte décrivant un jeu de processus. Ils se placent typiquement dans `tests/sample_inputs/`.

### Syntaxe complète

```
# Commentaire (ligne commençant par '#')

quantum <valeur_ms>          # Optionnel — défaut 50

process <pid> "<nom>" arrival=<ms> priority=<n>
    burst cpu=<ms> io=<ms>   # Burst CPU suivi d'une E/S
    burst cpu=<ms> io=<ms>   # Autant de bursts que nécessaire
    burst cpu=<ms>           # Dernier burst : pas d'io=
end
```

### Champs et valeurs par défaut

| Champ | Obligatoire | Défaut |
|---|---|---|
| `quantum` | Non | `50` ms |
| `arrival=` | Non | `0` ms |
| `priority=` | Non | `0` |
| `io=` sur un burst | Non | `0` (pas d'E/S) |

### Exemple : `basic.sim`

```
# 3 processus sans E/S — validation FIFO/SJF/RR
quantum 50

process 1 "P1_Court" arrival=0 priority=1
    burst cpu=100
end

process 2 "P2_Moyen" arrival=0 priority=2
    burst cpu=300
end

process 3 "P3_Long" arrival=0 priority=3
    burst cpu=150
end
```

### Exemple : `io_heavy.sim`

```
# 4 processus avec cycles E/S parallélisées
quantum 50

process 1 "WebServer" arrival=0 priority=2
    burst cpu=200 io=100
    burst cpu=150 io=50
    burst cpu=100
end

process 2 "Compiler" arrival=10 priority=1
    burst cpu=300 io=200
    burst cpu=200
end

process 3 "Shell" arrival=0 priority=3
    burst cpu=50 io=80
    burst cpu=50 io=80
    burst cpu=50
end

process 4 "Daemon" arrival=5 priority=1
    burst cpu=80 io=150
    burst cpu=80
end
```

---

## 7. Algorithmes disponibles

| Clé CLI | Nom complet | Préemptif | Quantum |
|---|---|---|---|
| `fifo` | First In First Out | Non | — |
| `sjf` | Shortest Job First | Non | — |
| `srjf` | Shortest Remaining Job First | **Oui** | — |
| `rr` | Round Robin | **Oui** | Oui (`-q`) |

Lister les algorithmes à l'exécution :

```bash
./scheduler -l
```

---

## 8. Sorties produites

### Table des métriques (terminal)

Affichée automatiquement à la fin de chaque simulation. Format aligné en colonnes, compatible copier-coller dans un tableur.

```
Algorithme : Round Robin  |  Quantum : 50 ms  |  Processus : 4

PID  Nom         Arrivée  Fin   Restitution  Attente  Réponse
---  ----------  -------  ----  -----------  -------  -------
1    WebServer        0   850         850      400      0
2    Compiler        10   950         940      340     40
...
---  ----------  -------  ----  -----------  -------  -------
MOY                                   ...      ...     ...

Utilisation CPU : 87.3 %
```

### Diagramme de Gantt ASCII (`-g`)

```
./scheduler -a rr -q 50 -g -f tests/sample_inputs/io_heavy.sim
```

Produit une ligne de timeline par processus dans le terminal, avec les états CPU, IDLE et BLOCKED visibles.

### Fichier CSV (automatique)

Un fichier CSV est **toujours généré** après une simulation réussie. Le nom par défaut est `<ALGO>_<AAAAMMJJ>_<HHMMSS>.csv` (ex: `RR_20260402_141617.csv`).

Pour spécifier un nom explicite :

```bash
./scheduler -a rr -f tests/sample_inputs/io_heavy.sim -o resultats_rr.csv
```

**Structure du CSV :**

```
algo,quantum_ms,n_processes,cpu_util_pct
RR,50,4,87.3

pid,name,arrival_ms,finish_ms,turnaround_ms,wait_ms,response_ms
1,WebServer,0,850,850,400,0
...
MOY,,,,350,245,18
```

### IHM ncurses interactive (`-i`)

Requiert la compilation avec ncurses. Affiche un Gantt coloré et le tableau de métriques. Navigation :

- `q` : quitter
- Flèches directionnelles : faire défiler si la timeline dépasse la largeur du terminal

---

## 9. Génération des graphiques

Deux modes sont disponibles.

### Mode automatique (`-P`)

Génère les PNG immédiatement après la simulation :

```bash
./scheduler -a rr -q 50 -g -P -f tests/sample_inputs/io_heavy.sim
```

Produit dans le répertoire courant :
- `RR_<timestamp>_gantt.png` — diagramme de Gantt horizontal
- `RR_<timestamp>_metrics.png` — barres groupées (turnaround / attente / réponse)

### Mode différé (`make plot`)

Sur un CSV existant :

```bash
make plot CSV=RR_20260402_141617.csv
```

### Dépendances Python

```bash
pip3 install matplotlib
```

---

## 10. Documentation Doxygen

```bash
make doc
```

La documentation HTML est générée dans `docs/`. Ouvrez `docs/html/index.html` dans un navigateur.

> Requiert `doxygen` installé sur le système.

---

## 11. Ajouter un algorithme

L'architecture repose sur une **vtable de pointeurs de fonctions** (`struct Scheduler`). Ajouter un algorithme ne nécessite de modifier qu'**un seul fichier existant** après la création du nouveau module.

### Étape 1 — Créer `src/algorithms/mon_algo.c`

Implémenter la fonction d'initialisation qui remplit la vtable :

```c
#include "scheduler.h"
#include <stdlib.h>

static void mon_algo_admit(Scheduler *s, PCB *pcb, uint32_t now) { /* ... */ }
static PCB *mon_algo_pick_next(Scheduler *s, uint32_t now)        { /* ... */ }
static void mon_algo_on_yield(Scheduler *s, PCB *pcb, uint32_t now, int reason) { /* ... */ }
static void mon_algo_destroy(Scheduler *s) { free(s->private_data); }

void mon_algo_init(Scheduler *s, uint32_t quantum_ms)
{
    (void)quantum_ms;
    s->name         = "Mon Algorithme";
    s->short_name   = "MON";
    s->is_preemptive = 0;
    s->quantum_ms   = 0;

    s->admit          = mon_algo_admit;
    s->pick_next      = mon_algo_pick_next;
    s->on_yield       = mon_algo_on_yield;
    s->on_io_done     = mon_algo_admit;  /* Réadmettre après E/S */
    s->should_preempt = NULL;
    s->destroy        = mon_algo_destroy;

    s->private_data = /* allouer l'état interne */;
}
```

### Étape 2 — Enregistrer dans `src/scheduler.c`

Ajouter une entrée dans le tableau `scheduler_registry[]` :

```c
extern void mon_algo_init(Scheduler *s, uint32_t quantum_ms);

static const SchedulerRegistryEntry scheduler_registry[] = {
    { "fifo", fifo_init      },
    { "sjf",  sjf_init       },
    { "srjf", srjf_init      },
    { "rr",   round_robin_init },
    { "mon",  mon_algo_init  },  /* <-- ajouter ici */
    { NULL, NULL }
};
```

### Étape 3 — Ajouter la source au Makefile

Dans `Makefile`, section `SRCS` :

```makefile
SRCS = ...
       $(SRC_DIR)/algorithms/mon_algo.c
```

### Étape 4 — Compiler et tester

```bash
make clean && make
./scheduler -a mon -f tests/sample_inputs/basic.sim -g
```

---

## 12. Nettoyage

| Commande | Effet |
|---|---|
| `make clean` | Supprime `build/` et l'exécutable `scheduler` |
| `make distclean` | `clean` + supprime `docs/` |

---

*Généré le 2 avril 2026 — Projet L3 Informatique, Simulation d'Ordonnancement de Processus.*
