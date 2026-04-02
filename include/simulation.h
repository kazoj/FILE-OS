/**
 * @file simulation.h
 * @brief Moteur de simulation par cycles (1 ms/tick).
 *
 * La simulation avance d'un milliseconde par tick. À chaque tick :
 *  1. Les processus arrivant à ce tick sont admis (NEW → READY).
 *  2. Les E/S en cours progressent ; les terminées passent BLOCKED → READY.
 *  3. Pour les algorithmes préemptifs, on vérifie si une préemption est requise.
 *  4. Si le CPU est libre, le scheduler sélectionne un processus.
 *  5. Le CPU avance d'1 ms pour le processus courant.
 *  6. L'entrée Gantt est enregistrée/étendue.
 *  7. Si le burst CPU se termine, on déclenche l'E/S ou la terminaison.
 */

#ifndef SIMULATION_H
#define SIMULATION_H

#include "process.h"
#include "scheduler.h"
#include "metrics.h"
#include "queue.h"
#include <stddef.h>

/**
 * @brief Entrée du diagramme de Gantt.
 *
 * pid == 0 signifie que le CPU était inactif (IDLE) pendant cet intervalle.
 * is_io == 1 signifie que le processus était en attente d'E/S (BLOCKED).
 */
typedef struct {
    uint32_t pid;      /**< PID du processus (0 = IDLE) */
    uint32_t start_ms; /**< Début de l'intervalle */
    uint32_t end_ms;   /**< Fin de l'intervalle (exclu) */
    int      is_io;    /**< 0 = CPU/IDLE, 1 = E/S (BLOCKED) */
} GanttEntry;

/**
 * @brief État complet de la simulation, passé à travers la boucle de tick.
 */
typedef struct {
    /* --- Entrées --- */
    Process        *processes;     /**< Tableau de descripteurs statiques */
    size_t          num_processes; /**< Nombre de processus */
    Scheduler      *scheduler;     /**< Algorithme pluggé */

    /* --- État runtime --- */
    PCB           **pcb_table;            /**< pcb_table[i] ↔ processes[i] */
    PCB            *running;              /**< Processus courant sur le CPU, ou NULL */
    uint32_t        clock_ms;             /**< Horloge de simulation */
    uint32_t        quantum_elapsed_ms;   /**< Temps écoulé du quantum courant (RR) */
    size_t          terminated_count;     /**< Nombre de processus terminés */
    uint32_t        cpu_busy_ms;          /**< Ticks où le CPU était occupé */

    /* --- Gantt --- */
    GanttEntry     *gantt;          /**< Tableau dynamique d'entrées Gantt */
    size_t          gantt_count;    /**< Nombre d'entrées remplies */
    size_t          gantt_capacity; /**< Capacité allouée */

    /* --- Métriques (remplies après la simulation) --- */
    MetricsReport  *report; /**< Rapport calculé par metrics_compute */

    /* --- Extension : E/S non parallélisables (-S) --- */
    int   sequential_io; /**< 1 = un seul device I/O à la fois */
    Queue io_queue;      /**< File FIFO d'attente du device I/O */
} SimulationState;

/* ------------------------------------------------------------------ */
/*  API publique (simulation.c)                                        */
/* ------------------------------------------------------------------ */

/**
 * @brief Alloue et initialise un SimulationState.
 *
 * @param procs  Tableau de Process (doit rester valide pendant la simulation).
 * @param n      Nombre de processus.
 * @param sched  Scheduler à utiliser (doit être initialisé).
 * @return       SimulationState alloué, ou NULL en cas d'erreur.
 */
SimulationState *sim_create(Process *procs, size_t n, Scheduler *sched);

/**
 * @brief Lance la simulation jusqu'à la terminaison de tous les processus.
 *
 * Remplit sim->gantt et sim->report à la fin.
 *
 * @param sim  État de simulation.
 * @return     0 en cas de succès, non-zéro en cas d'erreur.
 */
int sim_run(SimulationState *sim);

/**
 * @brief Libère un SimulationState et ses ressources internes.
 *
 * Ne libère pas le Scheduler ni le tableau de Process.
 *
 * @param sim  État à libérer.
 */
void sim_destroy(SimulationState *sim);

/* Fonctions internes exposées pour les tests unitaires */
void sim_tick_admit_arrivals(SimulationState *sim);
void sim_tick_advance_io(SimulationState *sim);
void sim_tick_check_preemption(SimulationState *sim);
void sim_tick_run_cpu(SimulationState *sim);
void sim_tick_record_gantt(SimulationState *sim, int idle);

#endif /* SIMULATION_H */
