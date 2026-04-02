/**
 * @file scheduler.h
 * @brief Interface générique d'ordonnancement via pointeurs de fonctions (vtable).
 *
 * ## Architecture extensible
 *
 * Chaque algorithme d'ordonnancement est représenté par un `Scheduler`,
 * une structure dont les champs sont des pointeurs de fonctions. Ajouter
 * un nouvel algorithme consiste à :
 *
 *  1. Créer un fichier `src/algorithms/mon_algo.c`.
 *  2. Y implémenter une fonction `mon_algo_init(Scheduler *s, uint32_t q)`.
 *  3. Ajouter une entrée dans `scheduler_registry[]` dans `src/scheduler.c`.
 *
 * **Aucun autre fichier n'a besoin d'être modifié.**
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "process.h"
#include "queue.h"
#include <stdint.h>

/** Forward declaration pour les pointeurs de fonctions. */
typedef struct Scheduler Scheduler;

/* ------------------------------------------------------------------ */
/*  Raisons de libération du CPU                                        */
/* ------------------------------------------------------------------ */

#define YIELD_CPU_BURST_DONE  0 /**< Burst CPU terminé normalement */
#define YIELD_PREEMPTED       1 /**< Préemption par l'ordonnanceur */
#define YIELD_IO_START        2 /**< Le processus passe en E/S */

/* ------------------------------------------------------------------ */
/*  Types des pointeurs de fonctions (vtable)                          */
/* ------------------------------------------------------------------ */

/**
 * @brief Initialise l'état interne de l'algorithme.
 *
 * Appelée une fois avant le début de la simulation. L'algorithme doit
 * allouer son `private_data` ici.
 *
 * @param s  Instance du scheduler.
 */
typedef void (*SchedInitFn)(Scheduler *s);

/**
 * @brief Admet un nouveau processus dans la file de l'algorithme.
 *
 * Appelée à chaque fois qu'un processus passe de NEW à READY.
 *
 * @param s    Instance du scheduler.
 * @param pcb  PCB du processus arrivant.
 * @param now  Temps courant de simulation en ms.
 */
typedef void (*SchedAdmitFn)(Scheduler *s, PCB *pcb, uint32_t now);

/**
 * @brief Sélectionne le prochain processus à exécuter.
 *
 * @param s    Instance du scheduler.
 * @param now  Temps courant de simulation en ms.
 * @return     PCB à exécuter, ou NULL si aucun processus n'est prêt.
 */
typedef PCB *(*SchedPickNextFn)(Scheduler *s, uint32_t now);

/**
 * @brief Notifie l'algorithme qu'un processus libère le CPU.
 *
 * Appelée quand le processus courant termine son burst, est préempté,
 * ou part en E/S. L'algorithme gère le replacement dans la file.
 *
 * @param s       Instance du scheduler.
 * @param pcb     PCB qui libère le CPU.
 * @param now     Temps courant de simulation en ms.
 * @param reason  YIELD_CPU_BURST_DONE | YIELD_PREEMPTED | YIELD_IO_START
 */
typedef void (*SchedYieldFn)(Scheduler *s, PCB *pcb, uint32_t now, int reason);

/**
 * @brief Notifie l'algorithme qu'une E/S vient de se terminer.
 *
 * Le processus doit être remis dans la file des prêts.
 *
 * @param s    Instance du scheduler.
 * @param pcb  PCB dont l'E/S est terminée.
 * @param now  Temps courant de simulation en ms.
 */
typedef void (*SchedIoDoneFn)(Scheduler *s, PCB *pcb, uint32_t now);

/**
 * @brief Détermine si le processus en cours doit être préempté.
 *
 * Appelée à chaque tick pour les algorithmes préemptifs.
 *
 * @param s        Instance du scheduler.
 * @param running  PCB actuellement en cours d'exécution.
 * @param now      Temps courant de simulation en ms.
 * @return         Non-zéro si le scheduler veut préempter.
 */
typedef int (*SchedShouldPreemptFn)(Scheduler *s, const PCB *running, uint32_t now);

/**
 * @brief Libère l'état interne de l'algorithme (private_data, etc.).
 *
 * Appelée une fois après la fin de la simulation.
 *
 * @param s  Instance du scheduler.
 */
typedef void (*SchedDestroyFn)(Scheduler *s);

/* ------------------------------------------------------------------ */
/*  Structure Scheduler (la vtable)                                    */
/* ------------------------------------------------------------------ */

/**
 * @brief Instance d'un algorithme d'ordonnancement.
 *
 * Combine identité, paramètres, vtable et état privé de l'algorithme.
 * Les algorithmes remplissent les pointeurs de fonctions dans leur
 * fonction `*_init`. La simulation appelle ces fonctions via la vtable
 * sans connaître l'algorithme concret.
 */
struct Scheduler {
    const char *name;          /**< Nom complet (ex: "Round Robin") */
    const char *short_name;    /**< Abréviation pour CSV (ex: "RR") */
    int         is_preemptive; /**< 1 si l'algo peut préempter, 0 sinon */
    uint32_t    quantum_ms;    /**< Quantum RR en ms ; 0 si sans objet */

    /* --- Vtable --- */
    SchedInitFn          init;            /**< Initialisation interne */
    SchedAdmitFn         admit;           /**< Admission d'un nouveau processus */
    SchedPickNextFn      pick_next;       /**< Sélection du prochain processus */
    SchedYieldFn         on_yield;        /**< Libération du CPU */
    SchedIoDoneFn        on_io_done;      /**< Fin d'une E/S */
    SchedShouldPreemptFn should_preempt;  /**< Test de préemption (tick) */
    SchedDestroyFn       destroy;         /**< Nettoyage interne */

    /**
     * @brief État privé de l'algorithme.
     *
     * Chaque algorithme alloue et caste ce pointeur vers sa propre
     * structure interne :
     *  - FIFO       → `Queue *`
     *  - SJF / SRJF → `PriorityQueue *`
     *  - RR         → `struct { Queue q; uint32_t time_on_cpu_ms; } *`
     */
    void *private_data;
};

/* ------------------------------------------------------------------ */
/*  Registre des algorithmes (scheduler.c)                             */
/* ------------------------------------------------------------------ */

/**
 * @brief Entrée du registre des algorithmes disponibles.
 */
typedef struct {
    const char *name;  /**< Clé de recherche (insensible à la casse) */
    /** Fonction d'initialisation de l'algorithme. */
    void (*init_fn)(Scheduler *s, uint32_t quantum_ms);
} SchedulerRegistryEntry;

/**
 * @brief Crée et initialise un Scheduler par son nom.
 *
 * Recherche dans `scheduler_registry[]` (insensible à la casse),
 * alloue une instance et appelle sa fonction d'initialisation.
 *
 * @param name        Nom de l'algorithme ("fifo", "sjf", "srjf", "rr").
 * @param quantum_ms  Quantum pour RR ; ignoré pour les autres algorithmes.
 * @return            Scheduler alloué sur le tas, ou NULL si non trouvé.
 */
Scheduler *scheduler_create(const char *name, uint32_t quantum_ms);

/**
 * @brief Libère un Scheduler et ses ressources internes.
 * @param s  Scheduler à libérer (peut être NULL).
 */
void scheduler_destroy(Scheduler *s);

/**
 * @brief Affiche sur stdout la liste des algorithmes disponibles.
 */
void scheduler_list_available(void);

#endif /* SCHEDULER_H */
