# Simulateur d'Ordonnancement de Processus
## Document de conception — Structures de données et algorithmes

**Fichier :** CONCEPTION.md  
**Objectif :** Aider un développeur souhaitant modifier ou étendre le simulateur (ajout d'un algorithme, nouvelle IHM, nouvelles métriques…) à comprendre les choix d'architecture et à localiser les fonctions clés.

---

## Table des matières

1. [Vue d'ensemble de l'architecture](#1-vue-densemble-de-larchitecture)
2. [Structures de données principales](#2-structures-de-données-principales)
   - 2.1 [Process et BurstEntry](#21-process-et-burstentry--processh)
   - 2.2 [PCB — Process Control Block](#22-pcb--process-control-block--processh)
   - 2.3 [Queue FIFO](#23-queue-fifo--queueh--queuec)
   - 2.4 [PriorityQueue](#24-priorityqueue--queueh--queuec)
   - 2.5 [Scheduler — la vtable](#25-scheduler--la-vtable--schedulerh--schedulerc)
   - 2.6 [SimulationState](#26-simulationstate--simulationh--simulationc)
   - 2.7 [GanttEntry](#27-ganttentry--simulationh)
   - 2.8 [MetricsReport et ProcessMetrics](#28-metricsreport-et-processmetrics--metricsh)
3. [Algorithmes d'ordonnancement](#3-algorithmes-dordonnancement)
   - 3.1 [FIFO](#31-fifo-first-in-first-out--srcalgorithmsfifo.c)
   - 3.2 [SJF](#32-sjf-shortest-job-first--srcalgorithmssjfc)
   - 3.3 [SRJF](#33-srjf-shortest-remaining-job-first--srcalgorithmssrjfc)
   - 3.4 [Round Robin](#34-round-robin--srcalgorithmsround_robinc)
4. [Moteur de simulation (boucle de ticks)](#4-moteur-de-simulation-boucle-de-ticks)
5. [Calcul des métriques](#5-calcul-des-métriques)
6. [Module d'entrée / sortie](#6-module-dentrée--sortie)
7. [Comment ajouter un nouvel algorithme](#7-comment-ajouter-un-nouvel-algorithme)
8. [Comment ajouter une nouvelle sortie / IHM](#8-comment-ajouter-une-nouvelle-sortie--ihm)
9. [Choix de conception justifiés](#9-choix-de-conception-justifiés)

---

## 1. Vue d'ensemble de l'architecture

Le simulateur est organisé en couches indépendantes :

```
┌────────────────────────────────────────────────────────────┐
│                         main.c                             │
│   (orchestration : parsing → simulation → affichage)       │
└──────────────┬────────────────────────────────────────────┘
               │
     ┌─────────▼─────────┐
     │    input.c         │  Lecture .sim / arguments CLI
     └─────────┬──────────┘
               │  Process[]
     ┌─────────▼──────────┐      ┌──────────────────────────┐
     │  simulation.c       │◄────►│  scheduler.c + algos/*.c  │
     │  (boucle de ticks)  │      │  (vtable + algorithmes)   │
     └─────────┬───────────┘      └──────────────────────────┘
               │  SimulationState + MetricsReport
     ┌─────────▼───────────────────────────────────────────┐
     │              Sorties                                 │
     │   output.c (table, Gantt ASCII, CSV)                 │
     │   output_gtk.c (IHM desktop GTK4)                    │
     │   tools/plot_results.py (graphes PNG)                │
     └─────────────────────────────────────────────────────┘
```

- Chaque couche ne connaît que les interfaces des couches adjacentes.
- La simulation ne sait pas quel algorithme est utilisé (vtable).
- Les sorties ne savent pas comment la simulation a fonctionné (rapport).

---

## 2. Structures de données principales

### 2.1 Process et BurstEntry — `process.h`

```c
typedef struct {
    uint32_t cpu_duration_ms;   /* durée du burst CPU */
    uint32_t io_duration_ms;    /* durée de l'E/S qui suit (0 = aucune) */
} BurstEntry;

typedef struct {
    uint32_t   pid;
    char       name[64];
    uint32_t   arrival_time_ms;
    uint32_t   priority;          /* non utilisé par les algos actuels */
    uint32_t   num_bursts;
    BurstEntry burst_table[MAX_BURSTS];  /* MAX_BURSTS = 32 */
} Process;
```

**Rôle :** Descripteur statique et immuable d'un processus. Chargé une seule fois depuis le fichier `.sim` ou la CLI. Jamais modifié pendant la simulation.

**Choix :** Tableau statique de taille `MAX_BURSTS` pour éviter les allocations dynamiques dans la structure la plus souvent lue. 32 bursts est suffisant pour les cas d'usage pédagogiques.

---

### 2.2 PCB — Process Control Block — `process.h`

```c
typedef struct PCB {
    Process       *proc;               /* pointeur vers le Process immuable */
    ProcessState   state;              /* NEW, READY, RUNNING, BLOCKED, TERMINATED */
    uint32_t       current_burst_idx;  /* index dans proc->burst_table */
    uint32_t       remaining_cpu_ms;   /* temps CPU restant du burst courant */
    uint32_t       remaining_io_ms;    /* temps E/S restant du burst courant */

    uint32_t       arrival_time_ms;    /* copie pour accès rapide */
    uint32_t       first_run_time_ms;  /* pour le temps de réponse */
    uint32_t       finish_time_ms;
    uint32_t       total_wait_ms;      /* accumulé à chaque tick en READY */
    uint32_t       total_cpu_ms;
    uint32_t       total_io_ms;
    uint32_t       last_ready_time_ms; /* horodatage du dernier passage READY */
    int            has_run;

    struct PCB    *next;               /* liste chaînée intrusive */
} PCB;
```

**Rôle :** État mutable du processus pendant la simulation. Un PCB est créé par `pcb_create()` au début de la simulation et détruit à la fin.

**Points importants pour un développeur :**
- `remaining_cpu_ms` et `remaining_io_ms` sont décrémentés à chaque tick.
- `last_ready_time_ms` permet de calculer le temps d'attente accumulé : lorsqu'un processus passe de READY à RUNNING, on ajoute `(now - last_ready_time_ms)` à `total_wait_ms`.
- Le champ `next` est un pointeur **intrusif** : le PCB peut appartenir directement à une `Queue` ou `PriorityQueue` sans wrapper.
- Toute transition d'état passe par `pcb_transition()` (`process.c`) qui met à jour les timestamps automatiquement.

**Transitions d'état :**
```
NEW       → READY      (admission à arrival_time_ms)
READY     → RUNNING    (sélection par l'ordonnanceur)
RUNNING   → READY      (préemption)
RUNNING   → BLOCKED    (début d'une E/S)
BLOCKED   → READY      (fin d'une E/S)
RUNNING   → TERMINATED (dernier burst terminé)
```

---

### 2.3 Queue FIFO — `queue.h` / `queue.c`

```c
typedef struct {
    QueueNode *head;   /* prochain à sortir */
    QueueNode *tail;   /* dernier entré */
    size_t     count;
} Queue;
```

Liste chaînée simple (head/tail), insertion en O(1) en queue, extraction en O(1) en tête.

Utilisée par : FIFO, Round Robin, et la file d'attente des E/S séquentielles (`io_queue` dans `SimulationState`).

**API principale :**
| Fonction | Description |
|----------|-------------|
| `queue_enqueue(q, pcb)` | Enfile le PCB |
| `queue_dequeue(q)` | Défile et retourne, ou `NULL` si vide |
| `queue_peek(q)` | Consulte sans retirer |
| `queue_is_empty(q)` | Test de vacuité |

---

### 2.4 PriorityQueue — `queue.h` / `queue.c`

```c
typedef struct {
    QueueNode    *head;    /* meilleur candidat toujours en tête */
    size_t        count;
    PCBComparator compare; /* comparateur injecté à la création */
} PriorityQueue;

typedef int (*PCBComparator)(const PCB *a, const PCB *b);
/* retourne < 0 si a passe avant b */
```

Liste chaînée triée. L'insertion maintient l'ordre en O(n). L'extraction du meilleur candidat est O(1) (toujours en tête).

**Comparateurs prédéfinis :**
| Comparateur | Usage |
|-------------|-------|
| `cmp_remaining_cpu(a, b)` | Tri par temps CPU restant (SJF, SRJF) |
| `cmp_arrival_time(a, b)` | Tri par date d'arrivée |
| `cmp_priority(a, b)` | Tri par priorité statique décroissante |

**Fonction spéciale :** `pqueue_reorder(pq)` — retrie la file après modification d'une clé. Utilisée par SRJF : après chaque tick, `remaining_cpu_ms` du processus en cours a changé, il faut re-trier la file.

Utilisée par : SJF, SRJF.

---

### 2.5 Scheduler — la vtable — `scheduler.h` / `scheduler.c`

```c
struct Scheduler {
    const char *name;
    const char *short_name;
    int         is_preemptive;
    uint32_t    quantum_ms;

    /* Vtable */
    SchedInitFn          init;
    SchedAdmitFn         admit;
    SchedPickNextFn      pick_next;
    SchedYieldFn         on_yield;
    SchedIoDoneFn        on_io_done;
    SchedShouldPreemptFn should_preempt;
    SchedDestroyFn       destroy;

    void *private_data;   /* état privé de l'algorithme */
};
```

Le `Scheduler` est le cœur du patron de conception utilisé. C'est une implémentation manuelle du polymorphisme en C via pointeurs de fonctions, équivalente à une classe abstraite en C++/Java.

Chaque algorithme possède son `private_data` :

| Algorithme | `private_data` |
|------------|----------------|
| FIFO | `Queue *` |
| SJF | `PriorityQueue *` |
| SRJF | `PriorityQueue *` |
| RR | `struct { Queue q; uint32_t quantum_ms; } *` |

La simulation ne manipule que des pointeurs de type `Scheduler*`. Elle ne sait jamais si elle parle à FIFO ou Round Robin.

**Registre des algorithmes** (`src/scheduler.c`, tableau `scheduler_registry[]`) :
```c
{ "fifo", fifo_init        }
{ "sjf",  sjf_init         }
{ "srjf", srjf_init        }
{ "rr",   round_robin_init }
{ NULL,   NULL             }   // sentinelle obligatoire
```

`scheduler_create("fifo", 0)` cherche dans le registre (insensible à la casse), alloue un `Scheduler` et appelle la fonction `init` correspondante.

---

### 2.6 SimulationState — `simulation.h` / `simulation.c`

```c
typedef struct {
    Process        *processes;          /* tableau statique des Process */
    size_t          num_processes;
    Scheduler      *scheduler;

    PCB           **pcb_table;          /* pcb_table[i] ↔ processes[i] */
    PCB            *running;            /* NULL = CPU idle */
    uint32_t        clock_ms;           /* horloge de simulation */
    uint32_t        quantum_elapsed_ms; /* ticks depuis le début du quantum RR */
    size_t          terminated_count;
    uint32_t        cpu_busy_ms;

    GanttEntry     *gantt;              /* tableau dynamique */
    size_t          gantt_count;
    size_t          gantt_capacity;

    MetricsReport  *report;             /* rempli après sim_run() */

    int   sequential_io;                /* drapeau -S */
    Queue io_queue;                     /* file pour E/S séquentielle */
} SimulationState;
```

- Le tableau `pcb_table` est indexé comme `processes[]` : `pcb_table[i]` est le PCB du processus `processes[i]`. Cela permet de retrouver en O(1) le PCB d'un processus par son indice.
- `gantt` est un tableau dynamique réalloué par doublement de capacité (pattern classique, amorti O(1)) pour éviter de connaître à l'avance la durée de la simulation.

---

### 2.7 GanttEntry — `simulation.h`

```c
typedef struct {
    uint32_t pid;       /* 0 = IDLE */
    uint32_t start_ms;
    uint32_t end_ms;    /* exclu */
    int      is_io;     /* 0 = CPU/IDLE, 1 = E/S */
} GanttEntry;
```

Chaque entrée représente un intervalle continu d'activité d'un processus. Des intervalles consécutifs identiques sont fusionnés (extension de `end_ms`) pour éviter un tableau de taille égale à la durée de simulation en ms.

La sortie Gantt (ASCII ou GTK4) lit ce tableau après simulation.

---

### 2.8 MetricsReport et ProcessMetrics — `metrics.h`

```c
typedef struct {
    uint32_t pid, arrival_time_ms, finish_time_ms;
    uint32_t turnaround_time_ms, waiting_time_ms, response_time_ms;
    uint32_t total_cpu_ms, total_io_ms;
    char name[64];
} ProcessMetrics;

typedef struct {
    ProcessMetrics *per_process;
    size_t          num_processes;
    double avg_turnaround_ms, avg_waiting_ms, avg_response_ms;
    double cpu_utilization_pct;
    uint32_t total_simulation_ms, quantum_ms;
    const char *algorithm_name, *algorithm_short;
} MetricsReport;
```

Calculé une seule fois après simulation par `metrics_compute()`. Sert ensuite de source unique pour tous les rendus (table terminale, CSV, IHM GTK4). Aucun rendu ne recalcule les métriques lui-même.

---

## 3. Algorithmes d'ordonnancement

### 3.1 FIFO (First In First Out) — `src/algorithms/fifo.c`

| Propriété | Valeur |
|-----------|--------|
| Politique | Non préemptif |
| Structure | `Queue` (liste chaînée FIFO simple) |
| Complexité | O(1) admission, O(1) sélection |

**Fonctionnement détaillé :**
- `fifo_admit()` : enfile le PCB en queue.
- `fifo_pick_next()` : défile la tête, quel que soit `remaining_cpu_ms`.
- `fifo_on_yield()` : ne fait rien (non préemptif).
- `fifo_on_io_done()` : remet le PCB en file après son E/S.
- `fifo_should_preempt()` : retourne toujours 0.

**Cas particuliers :** Si plusieurs processus arrivent au même tick, ils sont enfilés dans l'ordre d'apparition dans le tableau `processes[]` (ordre du fichier `.sim`).

---

### 3.2 SJF (Shortest Job First) — `src/algorithms/sjf.c`

| Propriété | Valeur |
|-----------|--------|
| Politique | Non préemptif |
| Structure | `PriorityQueue` triée par `cmp_remaining_cpu` |
| Complexité | O(n) admission, O(1) sélection |

**Fonctionnement détaillé :**
- `sjf_admit()` : insère le PCB dans la `PriorityQueue` (maintien de l'ordre).
- `sjf_pick_next()` : extrait la tête (burst le plus court).
- `sjf_on_yield()` : ne fait rien (non préemptif).
- `sjf_on_io_done()` : réinsère le PCB dans la `PriorityQueue`.
- `sjf_should_preempt()` : retourne toujours 0.

> **Note :** `remaining_cpu_ms` au moment de l'admission est égal à la durée totale du burst courant (le processus n'a pas encore commencé à s'exécuter).

---

### 3.3 SRJF (Shortest Remaining Job First) — `src/algorithms/srjf.c`

| Propriété | Valeur |
|-----------|--------|
| Politique | Préemptif |
| Structure | `PriorityQueue` triée par `cmp_remaining_cpu` |
| Complexité | O(n) admission, O(1) sélection, O(n) réordonnancement |

**Fonctionnement détaillé :**
- `srjf_admit()` : insère dans la `PriorityQueue`.
- `srjf_pick_next()` : extrait la tête.
- `srjf_should_preempt()` : compare `remaining_cpu_ms` du processus en tête de file avec celui du processus en cours. Retourne 1 si la tête est plus courte → déclenche une préemption dans `sim_tick_check_preemption()`.
- `srjf_on_yield(YIELD_PREEMPTED)` : réinsère le processus préempté dans la `PriorityQueue` avec son `remaining_cpu_ms` actuel.
- Après chaque tick, la simulation décrémente `remaining_cpu_ms` du processus en cours → `pqueue_reorder()` est appelé si nécessaire pour maintenir l'ordre.

---

### 3.4 Round Robin — `src/algorithms/round_robin.c`

| Propriété | Valeur |
|-----------|--------|
| Politique | Préemptif |
| Structure | `Queue` FIFO + compteur `quantum_elapsed_ms` dans `SimulationState` |
| Complexité | O(1) admission, O(1) sélection, O(1) préemption |

**Fonctionnement détaillé :**
- `rr_admit()` : enfile le PCB en queue FIFO.
- `rr_pick_next()` : défile la tête ; remet `quantum_elapsed_ms` à 0 (reset géré dans `sim_tick_run_cpu`).
- `rr_on_yield(YIELD_PREEMPTED)` : remet le PCB **en queue** (fin de file), pas en tête — c'est ce qui garantit la rotation équitable.
- `rr_on_yield(YIELD_IO_START ou YIELD_CPU_BURST_DONE)` : ne fait rien, la simulation gère la transition vers BLOCKED/TERMINATED.
- `rr_should_preempt()` : retourne 0 car la logique de préemption RR est gérée directement dans `sim_tick_check_preemption()` qui compare `quantum_elapsed_ms >= scheduler->quantum_ms`.

**Quantum par défaut :** 50 ms (configurable via `-q` ou le mot-clé `quantum` dans le fichier `.sim`).

---

## 4. Moteur de simulation (boucle de ticks)

**Fichier :** `src/simulation.c` — fonction `sim_run()`

L'unité de temps est la milliseconde. La simulation avance d'1 tick = 1 ms par itération. Cela garantit un comportement déterministe et simplifie les calculs (pas de virgule flottante dans les compteurs de temps).

**Ordre d'exécution par tick :**

1. **`sim_tick_admit_arrivals()`**  
   Pour chaque processus en état NEW dont `arrival_time_ms == clock_ms` : appel à `scheduler->admit()` → processus passe en READY.

2. **`sim_tick_advance_io()`**  
   Pour chaque processus en état BLOCKED :
   - Décrémente `remaining_io_ms`.
   - Si `remaining_io_ms == 0` : appel à `scheduler->on_io_done()` → processus redevient READY.
   - En mode séquentiel (`-S`) : seul le premier de `io_queue` avance.

3. **`sim_tick_check_preemption()`**  
   Si `is_preemptive && running != NULL` :
   - Pour RR : si `quantum_elapsed_ms >= scheduler->quantum_ms` → préemption.
   - Pour SRJF : appel à `scheduler->should_preempt()` → si retourne 1, appel à `scheduler->on_yield(YIELD_PREEMPTED)`.
   - Le processus préempté passe de RUNNING à READY via `pcb_transition()`.

4. **`sim_tick_pick_and_run()`**  
   Si `running == NULL` (CPU libre) : appel à `scheduler->pick_next()` pour sélectionner le prochain processus. Le PCB sélectionné passe de READY à RUNNING.

5. **`sim_tick_run_cpu()`**  
   Si `running != NULL` :
   - Décrémente `remaining_cpu_ms`.
   - Incrémente `quantum_elapsed_ms` (pour RR) et `cpu_busy_ms`.
   - Si `remaining_cpu_ms == 0` : fin du burst courant.
     - Si un burst suivant existe avec `io_duration_ms > 0` : appel à `scheduler->on_yield(YIELD_IO_START)`, processus passe en BLOCKED (`remaining_io_ms` initialisé).
     - Si dernier burst : processus passe en TERMINATED.
     - Dans tous les cas : `running = NULL`.

6. **`sim_tick_record_gantt()`**  
   Si la dernière entrée Gantt a le même PID et `is_io == 0` : étendre `end_ms` (fusion). Sinon : nouvelle entrée. PID 0 pour IDLE.

**Condition d'arrêt :** `terminated_count == num_processes`. Un processus n'est compté terminé qu'une seule fois lorsque `pcb_transition()` est appelé avec `STATE_TERMINATED`.

---

## 5. Calcul des métriques

**Fichier :** `src/metrics.c` — fonction `metrics_compute()`

Appelée une seule fois après `sim_run()`. Parcourt `pcb_table[]` et calcule :

```
turnaround_time_ms = finish_time_ms - arrival_time_ms
waiting_time_ms    = total_wait_ms   (accumulé pendant la simulation)
response_time_ms   = first_run_time_ms - arrival_time_ms

avg_*              = somme / num_processes

cpu_utilization_pct = (cpu_busy_ms / total_simulation_ms) * 100.0
```

Les temps d'attente sont accumulés dans `pcb_transition()` : lorsqu'un processus quitte READY (→ RUNNING), on ajoute `(now - last_ready_time_ms)` à `total_wait_ms`.

---

## 6. Module d'entrée / sortie

### Entrée — `src/input.c`

Deux modes :

**a) Fichier `.sim`** : parsé ligne par ligne avec `sscanf`.  
Mots-clés reconnus : `quantum`, `process`, `burst`, `end`, `#` (commentaire).

**b) Arguments CLI** : les processus sont définis par leurs durées CPU passées en arguments positionnels après les options.  
Format : `"200"` (CPU seul) ou `"200,50,100"` (cpu=200, io=50, cpu=100).

La fonction d'entrée retourne un tableau `Process[]` alloué sur le tas. Ce tableau reste valide pendant toute la durée de la simulation.

---

### Sortie texte — `src/output.c`

| Fonction | Description |
|----------|-------------|
| `output_print_table()` | Affiche le tableau de métriques avec colonnes alignées |
| `output_print_gantt()` | Affiche le diagramme de Gantt ASCII |
| `output_save_csv()` | Sauvegarde le rapport en CSV (séparateur virgule) — nom auto-généré : `<ALGO>_<timestamp>.csv` |

---

### IHM GTK4 — `src/output_gtk.c`

Compilé seulement si `HAVE_GTK4` est défini par le Makefile (détection automatique via `pkg-config`). Activé par le flag `-G`.

**Layout :**
- Panneau gauche : informations de simulation (algo, quantum, métriques moyennes).
- Panneau droit haut (60%) : diagramme de Gantt dessiné avec Cairo.
- Panneau droit bas (40%) : tableau de métriques dans un `GtkGrid` scrollable.

Boucle d'événements : `gtk_application_run()`, bloque jusqu'à fermeture de fenêtre.

---

### Graphes Python — `tools/plot_results.py`

Lit le CSV produit par `output_save_csv()`. Génère deux fichiers PNG via matplotlib :
- `<algo>_metrics.png` : barres groupées (turnaround / attente / réponse).
- `<algo>_gantt.png` : barres horizontales (diagramme de Gantt).

Invocable via : `make plot CSV=<fichier.csv>`

---

## 7. Comment ajouter un nouvel algorithme

Exemple : ajouter "Priority Scheduling" (non préemptif, tri par priorité statique décroissante).

**Étape 1 — Créer `src/algorithms/priority.c`**

Implémenter les 6 fonctions de la vtable :

```c
static void  prio_do_init(Scheduler *s)
    { /* allouer PriorityQueue */ }

static void  prio_admit(Scheduler *s, PCB *pcb, uint32_t now)
    { /* pqueue_insert avec cmp_priority */ }

static PCB  *prio_pick_next(Scheduler *s, uint32_t now)
    { /* pqueue_pop */ }

static void  prio_on_yield(Scheduler *s, PCB *pcb, uint32_t now, int reason)
    { /* rien pour non-préemptif */ }

static void  prio_on_io_done(Scheduler *s, PCB *pcb, uint32_t now)
    { /* réinsérer dans pqueue */ }

static int   prio_should_preempt(Scheduler *s, const PCB *r, uint32_t now)
    { return 0; }

static void  prio_destroy(Scheduler *s)
    { /* libérer PriorityQueue */ }
```

Puis la fonction publique :

```c
void priority_init(Scheduler *s, uint32_t quantum_ms) {
    s->name          = "Priority Scheduling";
    s->short_name    = "PRIO";
    s->is_preemptive = 0;
    s->quantum_ms    = 0;
    s->init           = prio_do_init;
    s->admit          = prio_admit;
    s->pick_next      = prio_pick_next;
    s->on_yield       = prio_on_yield;
    s->on_io_done     = prio_on_io_done;
    s->should_preempt = prio_should_preempt;
    s->destroy        = prio_destroy;
    s->private_data   = NULL;
}
```

**Étape 2 — Déclarer dans `include/algorithms.h`**

```c
void priority_init(Scheduler *s, uint32_t quantum_ms);
```

**Étape 3 — Enregistrer dans `src/scheduler.c`**

```c
{ "priority", priority_init },
```

C'est tout. Aucun autre fichier n'est à modifier. La compilation et le routage CLI (`-a priority`) fonctionneront automatiquement.

---

## 8. Comment ajouter une nouvelle sortie / IHM

La sortie reçoit en entrée deux pointeurs :
- `const SimulationState *sim` → accès au Gantt (`sim->gantt`, `sim->gantt_count`)
- `const MetricsReport *report` → accès à toutes les métriques calculées

**Démarche :**
1. Créer `src/output_monformat.c` + `include/output_monformat.h`.
2. Implémenter une fonction :
   ```c
   void output_monformat_render(const SimulationState *sim,
                                const MetricsReport   *report);
   ```
3. Appeler cette fonction depuis `main.c` après `sim_run()` et `metrics_compute()`.
4. Ajouter si nécessaire un flag CLI dans `src/input.c` (struct `Config`).

**Exemples de données accessibles :**
```c
report->per_process[i].turnaround_time_ms  // métriques par processus
report->avg_waiting_ms                     // moyennes
report->cpu_utilization_pct
sim->gantt[j].pid, .start_ms, .end_ms      // intervalles Gantt
sim->gantt[j].is_io                        // 0=CPU, 1=E/S
```

---

## 9. Choix de conception justifiés

### a) Vtable (pointeurs de fonctions) pour le Scheduler

Permet d'ajouter un algorithme sans modifier le moteur de simulation. La simulation appelle `scheduler->pick_next()` sans savoir si c'est FIFO, RR ou un algorithme futur. C'est le patron **Stratégie** implémenté en C.

### b) Listes chaînées intrusives (champ `next` dans PCB)

Évite une allocation mémoire supplémentaire par élément en file. Le PCB lui-même porte son pointeur de chaînage. Inconvénient : un PCB ne peut appartenir qu'à une seule liste à la fois — acceptable car un processus ne peut être que dans un seul état à la fois.

### c) Simulation en ticks de 1 ms

Simplifie drastiquement la logique. Tout est entier, pas de virgule flottante dans les compteurs de temps. Le comportement est déterministe et facilement reproductible. Inconvénient : lent pour de très longs scénarios (10 000 ms = 10 000 itérations), mais acceptable en pédagogie.

### d) Séparation Process (statique) / PCB (mutable)

Le `Process` décrit ce que le processus **doit faire** (bursts). Le `PCB` décrit où il **en est** (`remaining_cpu_ms`, state, timestamps). Cette séparation permet de relancer plusieurs simulations sur le même jeu de `Process` en recréant uniquement les PCBs.

### e) Tableau dynamique pour Gantt (doublement de capacité)

La durée de simulation n'est pas connue à l'avance. Le doublement de capacité amortit le coût de réallocation à O(1) par entrée en moyenne. La mémoire est libérée par `sim_destroy()`.

### f) Rapport de métriques (MetricsReport) comme source unique

Aucun module de sortie ne recalcule quoi que ce soit. Ils lisent tous le même `MetricsReport`, ce qui garantit la cohérence entre la table terminale, le CSV et l'IHM GTK4.

### g) Registre d'algorithmes (`scheduler_registry[]`)

Ajouter un algorithme ne nécessite qu'une ligne dans ce tableau. Pas de `switch/case` dispersés dans le code. La recherche par nom est insensible à la casse et extensible sans recompiler les autres modules.
