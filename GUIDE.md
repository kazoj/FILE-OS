# Guide d'installation et d'utilisation
# Simulateur d'Ordonnancement de Processus

Ce guide vous accompagne pas à pas : de l'installation jusqu'à l'analyse des résultats. 

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

Avant de compiler, assurez-vous d'avoir les outils suivants. Les deux premiers sont indispensables, les autres sont optionnels selon ce que vous souhaitez faire.

| Composant | À quoi ça sert ici | Obligatoire |
|---|---|---|
| `gcc` ≥ 9 | Compiler le code C du simulateur | Oui |
| `make` | Lancer les commandes de build | Oui |
| `gtk4` | Afficher l'interface graphique desktop (option `--gui`) | Non |
| `doxygen` | Générer la documentation HTML du code | Non |
| `python3` + `matplotlib` | Générer des graphiques PNG (option `--plot`) | Non |

**Debian / Ubuntu :**
```bash
sudo apt install gcc make libgtk-4-dev doxygen python3-matplotlib
```

**macOS (Homebrew) :**
```bash
brew install gcc make gtk4 doxygen python3
pip3 install matplotlib
```

---

## 2. Compilation

Placez-vous dans le dossier du projet et lancez `make`.

### Build normal (recommandé)

```bash
cd file-OS/
make
```

Cela produit un exécutable `scheduler` à la racine du projet. Le Makefile détecte automatiquement si GTK4 est disponible sur votre système et l'active si c'est le cas :

```
[gtk4]    détecté — option -G activée
Exécutable : scheduler
```

### Build de débogage

Utile si vous voulez traquer un bug : active les symboles GDB, AddressSanitizer et UndefinedBehaviorSanitizer, qui signalent les erreurs mémoire à l'exécution.

```bash
make debug
```

---

## 3. Installation

Pour pouvoir appeler `scheduler` depuis n'importe quel dossier (sans `./`), installez-le :

```bash
make install
```

Le Makefile choisit automatiquement où installer selon vos droits :

- Si vous avez accès à `/usr/local/bin` → installe dans `/usr/local/bin/scheduler` (disponible pour tous les utilisateurs)
- Sinon → installe dans `$HOME/.local/bin/scheduler` (disponible pour vous uniquement)

> **Si l'installation atterrit dans `$HOME/.local/bin`**, ce dossier n'est peut-être pas dans votre `PATH`. Pour l'ajouter une fois pour toutes :
> ```bash
> echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
> source ~/.bashrc
> ```

Pour vérifier que tout fonctionne :
```bash
scheduler list
```

---

## 4. Utilisation rapide

Le simulateur supporte deux syntaxes : une interface en **sous-commandes** (recommandée) et l'ancienne syntaxe à flags courts (toujours supportée).

### Sous-commandes disponibles

| Sous-commande | Description |
|---|---|
| `run <algo>` | Lancer une simulation |
| `list` | Lister les algorithmes disponibles |
| `help [cmd]` | Afficher l'aide d'une sous-commande |
| `interactive` | Mode guidé interactif |

### Option A : depuis un fichier `.sim` (recommandé pour les jeux de test)

```bash
# Simulation FIFO avec Gantt ASCII
./scheduler run fifo tests/sample_inputs/basic.sim --gantt

# Round Robin, quantum 50 ms, Gantt + graphiques PNG
./scheduler run rr tests/sample_inputs/io_heavy.sim --quantum 50 --gantt --plot

# IHM desktop GTK4
./scheduler run rr tests/sample_inputs/io_heavy.sim --gui
```

Le fichier `.sim` est détecté automatiquement à l'extension — pas besoin de `-f`.

### Option B : processus en ligne de commande (--process)

Définissez chaque processus avec `--process "Nom:cpu=<ms>[,io=<ms>,cpu=<ms>...]"` :

```bash
# 2 processus purement CPU
./scheduler run fifo --process "P1:cpu=200" --process "P2:cpu=300"

# Processus avec E/S alternées, SRJF, Gantt
./scheduler run srjf --gantt \
    --process "P1:cpu=200,io=50,cpu=100" \
    --process "P2:cpu=300" \
    --process "P3:cpu=150,io=30,cpu=50"

# Round Robin quantum 100 ms
./scheduler run rr --quantum 100 \
    --process "Client:cpu=200,io=50,cpu=100" \
    --process "Server:cpu=300"
```

### Option C : mode interactif guidé

Si vous ne savez pas encore quelle commande utiliser, lancez simplement :

```bash
./scheduler interactive
# ou sans argument :
./scheduler
```

L'assistant vous posera des questions (algorithme, quantum, fichier ou processus manuels, options de sortie) et lancera la simulation.

### Option D : ancienne syntaxe (rétrocompatibilité)

L'ancienne syntaxe à flags courts est toujours valide :

```bash
./scheduler -a fifo -g -f tests/sample_inputs/basic.sim
./scheduler -a rr -q 50 -g -P -f tests/sample_inputs/io_heavy.sim
./scheduler -a srjf -g "200,50,100" "300" "150,30,50"
```

---

## 5. Options de la ligne de commande

### Syntaxe principale (sous-commande `run`)

```
scheduler run <algo> [options] [fichier.sim]
```

| Option | Argument | Description |
|---|---|---|
| `--quantum` / `-q` | entier (ms) | Quantum pour Round Robin (défaut : `50`). |
| `--output` / `-o` | chemin `.csv` | Nom du fichier CSV de sortie (défaut : auto). |
| `--gantt` / `-g` | — | Afficher le diagramme de Gantt ASCII. |
| `--gui` / `-G` | — | Ouvrir l'interface graphique GTK4 (nécessite GTK4). |
| `--plot` / `-P` | — | Générer des graphiques PNG (nécessite Python + matplotlib). |
| `--sequential-io` / `-S` | — | E/S séquentielles : les entrées/sorties bloquent le CPU. |
| `--verbose` / `-v` | — | Mode verbeux : liste les processus chargés avant simulation. |
| `--process` | spec | Ajouter un processus. Format : `"[Nom:]cpu=<ms>[,io=<ms>,cpu=<ms>...]"` |
| `-h` / `--help` | — | Afficher l'aide de `run` et quitter. |

Le fichier `.sim` peut être passé comme argument positionnel (détection automatique par l'extension `.sim`).

> Si votre fichier `.sim` contient une directive `quantum`, elle prend la priorité sur `--quantum` — sauf si vous passez `--quantum` explicitement.

### Format de `--process`

Chaque spec décrit un processus complet : un préfixe de nom optionnel suivi de paires `clé=valeur` séparées par des virgules.

```
"[Nom:]cpu=<ms>[,io=<ms>,cpu=<ms>...]"
```

| Token | Description |
|---|---|
| `Nom:` | Nom optionnel du processus (ex: `WebServer:`) |
| `cpu=<ms>` | Durée d'un burst CPU (obligatoire, au moins un) |
| `io=<ms>` | Durée de l'E/S qui suit le burst CPU précédent |

Exemples valides :
```
"cpu=200"                           → 1 burst CPU, pas d'E/S
"P1:cpu=200,io=50,cpu=100"          → burst 200ms CPU, 50ms E/S, 100ms CPU
"WebServer:cpu=150,io=80,cpu=100,io=40,cpu=50"
```

### Syntaxe legacy (rétrocompatibilité)

```
scheduler -a <algo> [options] [<burst_spec>...]
scheduler -a <algo> [options] -f <fichier.sim>
```

| Option | Description |
|---|---|
| `-a <algo>` | **Obligatoire.** Algorithme : `fifo`, `sjf`, `srjf`, `rr`. |
| `-f <fichier>` | Fichier `.sim` d'entrée. |
| `-q <ms>` | Quantum Round Robin. |
| `-o <fichier>` | Sortie CSV. |
| `-g` `-G` `-P` `-S` `-v` | Identiques aux options longues ci-dessus. |
| `-l` | Lister les algorithmes et quitter. |

---

## 6. Format du fichier `.sim`

Un fichier `.sim` est un fichier texte qui décrit un jeu de processus à simuler. C'est comme une "partition" : vous décrivez chaque processus, ses rafales CPU, ses pauses E/S, son heure d'arrivée. Placez vos fichiers dans `tests/sample_inputs/`.

### Syntaxe complète

```
# Commentaire (ligne commençant par '#')

quantum <valeur_ms>          # Optionnel — défaut 50

process <pid> "<nom>" arrival=<ms> priority=<n>
    burst cpu=<ms> io=<ms>   # Une rafale CPU suivie d'une pause E/S
    burst cpu=<ms> io=<ms>   # Autant de bursts que nécessaire
    burst cpu=<ms>           # Dernier burst : pas d'io= (le processus se termine)
end
```

### Champs et valeurs par défaut

| Champ | Obligatoire | Défaut |
|---|---|---|
| `quantum` | Non | `50` ms |
| `arrival=` | Non | `0` ms (arrive dès le début) |
| `priority=` | Non | `0` |
| `io=` sur un burst | Non | `0` (pas d'E/S après ce burst) |

### Exemple minimal : `basic.sim`

Trois processus qui ne font que du CPU. Parfait pour tester FIFO, SJF et RR.

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

### Exemple complet : `io_heavy.sim`

Quatre processus qui alternent CPU et E/S, avec des arrivées décalées.

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

- **FIFO** : les processus s'exécutent dans l'ordre d'arrivée, jusqu'à la fin. Simple mais injuste pour les longs processus.
- **SJF** : le processus avec le burst CPU le plus court passe en premier. Optimal pour le temps d'attente moyen, mais peut affamer les longs processus.
- **SRJF** : version préemptive de SJF — si un nouveau processus plus court arrive, il prend la main immédiatement.
- **RR** : chaque processus reçoit un quantum de temps fixe, puis cède le CPU au suivant. Équitable, mais le choix du quantum est crucial.

Pour voir la liste des algorithmes disponibles à l'exécution :

```bash
./scheduler list
# ou (legacy) :
./scheduler -l
```

---

## 8. Sorties produites

Après chaque simulation, trois types de sorties sont disponibles.

### Table des métriques (toujours affichée)

Le simulateur affiche automatiquement un tableau dans le terminal à la fin de chaque simulation. Il est aligné en colonnes pour pouvoir être copié-collé directement dans un tableur.

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

### Diagramme de Gantt ASCII (option `--gantt`)

Un diagramme de Gantt montre la timeline de chaque processus : quand il utilise le CPU, quand il attend, quand il fait des E/S. C'est la meilleure façon de visualiser le comportement de l'ordonnanceur.

```bash
./scheduler run rr tests/sample_inputs/io_heavy.sim --quantum 50 --gantt
```

Produit une ligne de timeline par processus dans le terminal, avec les états CPU, IDLE et BLOCKED visibles.

### Fichier CSV (toujours généré)

Un fichier CSV est **toujours créé** après une simulation réussie. Le nom par défaut suit le format `<ALGO>_<AAAAMMJJ>_<HHMMSS>.csv` (ex: `RR_20260402_141617.csv`).

Pour choisir le nom vous-même :

```bash
./scheduler run rr tests/sample_inputs/io_heavy.sim --output resultats_rr.csv
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

### Interface graphique GTK4 (option `--gui`)

Si vous avez compilé avec GTK4, cette option ouvre une fenêtre native avec :

- **Panneau gauche** : résumé de la simulation (algorithme, quantum, métriques moyennes, taux CPU).
- **Panneau droit haut** : diagramme de Gantt dessiné avec Cairo (CPU en vert, E/S en orange, IDLE en gris).
- **Panneau droit bas** : tableau de métriques scrollable par processus.

Le programme attend que vous fermiez la fenêtre avant de rendre la main au terminal.

---

## 9. Génération des graphiques

Vous pouvez générer des graphiques PNG à partir des résultats de simulation. Deux modes sont disponibles.

### Mode immédiat (`--plot`)

Génère les PNG directement après la simulation :

```bash
./scheduler run rr tests/sample_inputs/io_heavy.sim --quantum 50 --gantt --plot
```

Produit dans le dossier courant :
- `RR_<timestamp>_gantt.png` — diagramme de Gantt horizontal
- `RR_<timestamp>_metrics.png` — barres groupées (turnaround / attente / réponse)

### Mode différé (`make plot`)

Si vous avez déjà un CSV et que vous voulez juste générer les graphiques :

```bash
make plot CSV=RR_20260402_141617.csv
```

### Dépendance Python requise

```bash
pip3 install matplotlib
```

---

## 10. Documentation Doxygen

Doxygen lit les commentaires du code source et génère une documentation HTML navigable — pratique pour comprendre l'architecture du projet sans lire tout le code.

```bash
make doc
```

La documentation est générée dans `docs/`. Ouvrez ensuite `docs/html/index.html` dans votre navigateur.

> Requiert `doxygen` installé sur le système.

---

## 11. Ajouter un algorithme

Le simulateur est conçu pour accueillir facilement de nouveaux algorithmes. L'architecture repose sur une **vtable** (un ensemble de pointeurs de fonctions dans une struct `Scheduler`) : chaque algorithme implémente les mêmes "hooks" (admit, pick_next, on_yield…), et le moteur de simulation les appelle sans savoir quel algorithme tourne. Ajouter un algorithme ne nécessite de modifier **qu'un seul fichier existant**.

### Étape 1 — Créer `src/algorithms/mon_algo.c`

Implémentez les hooks et la fonction d'initialisation qui remplit la vtable :

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

Ajoutez une entrée dans le tableau `scheduler_registry[]` — c'est le seul fichier existant à modifier :

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
./scheduler run mon tests/sample_inputs/basic.sim --gantt
```

---

## 12. Nettoyage

| Commande | Quand l'utiliser | Effet |
|---|---|---|
| `make clean` | Avant un rebuild propre, ou pour libérer de l'espace | Supprime `build/` et l'exécutable `scheduler` |
| `make distclean` | Avant de livrer ou archiver le projet | `clean` + supprime aussi `docs/` |

---

*Généré le 2 avril 2026 — Projet L3 Informatique, Simulation d'Ordonnancement de Processus.*
