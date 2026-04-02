/**
 * @file simulation.c
 * @brief Moteur de simulation par cycles (1 ms/tick).
 *
 * Chaque appel à sim_run() exécute la boucle suivante jusqu'à ce que
 * tous les processus soient à l'état TERMINATED :
 *
 *  tick N :
 *    1. sim_tick_admit_arrivals   — admet les processus arrivant à clock_ms
 *    2. sim_tick_advance_io       — avance les E/S de 1 ms
 *    3. sim_tick_check_preemption — préempte si nécessaire
 *    4. pick_next                 — sélectionne un processus si CPU libre
 *    5. sim_tick_run_cpu          — exécute 1 ms CPU
 *    6. sim_tick_record_gantt     — enregistre dans le Gantt
 *    7. gestion fin de burst      — I/O ou TERMINATED
 *    clock_ms++
 */

#include "simulation.h"
#include "process.h"
#include "scheduler.h"
#include "metrics.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Capacité initiale du tableau Gantt */
#define GANTT_INIT_CAP 256

/* ------------------------------------------------------------------ */
/*  Utilitaires internes                                                */
/* ------------------------------------------------------------------ */

/**
 * @brief Ajoute ou étend une entrée dans le tableau Gantt.
 *
 * Si la dernière entrée a le même PID, on étend simplement end_ms.
 * Sinon on crée une nouvelle entrée (avec doublement de la capacité
 * si nécessaire).
 *
 * @param sim   État de simulation.
 * @param pid   PID du processus courant (0 = IDLE).
 */
static void gantt_push(SimulationState *sim, uint32_t pid, int is_io)
{
    /* Extension de la dernière entrée si même PID et même type */
    if (sim->gantt_count > 0) {
        GanttEntry *last = &sim->gantt[sim->gantt_count - 1];
        if (last->pid == pid && last->is_io == is_io) {
            last->end_ms = sim->clock_ms + 1;
            return;
        }
    }

    /* Agrandissement du tableau si nécessaire */
    if (sim->gantt_count >= sim->gantt_capacity) {
        size_t new_cap = sim->gantt_capacity * 2;
        GanttEntry *tmp = realloc(sim->gantt, new_cap * sizeof(GanttEntry));
        if (!tmp) { perror("gantt_push: realloc"); return; }
        sim->gantt          = tmp;
        sim->gantt_capacity = new_cap;
    }

    /* Nouvelle entrée */
    GanttEntry *e = &sim->gantt[sim->gantt_count++];
    e->pid      = pid;
    e->start_ms = sim->clock_ms;
    e->end_ms   = sim->clock_ms + 1;
    e->is_io    = is_io;
}

/* ------------------------------------------------------------------ */
/*  API publique                                                        */
/* ------------------------------------------------------------------ */

/**
 * @brief Alloue et initialise un SimulationState.
 *
 * Crée un PCB pour chaque processus et alloue le tableau Gantt initial.
 *
 * @param procs  Tableau de Process (doit rester valide pendant la simulation).
 * @param n      Nombre de processus.
 * @param sched  Scheduler initialisé.
 * @return       SimulationState alloué, ou NULL en cas d'erreur.
 */
SimulationState *sim_create(Process *procs, size_t n, Scheduler *sched)
{
    if (!procs || n == 0 || !sched) return NULL;

    SimulationState *sim = calloc(1, sizeof(SimulationState));
    if (!sim) { perror("sim_create"); return NULL; }

    sim->processes     = procs;
    sim->num_processes = n;
    sim->scheduler     = sched;
    sim->clock_ms      = 0;

    /* Créer les PCBs */
    sim->pcb_table = calloc(n, sizeof(PCB *));
    if (!sim->pcb_table) { free(sim); return NULL; }

    for (size_t i = 0; i < n; i++) {
        sim->pcb_table[i] = pcb_create(&procs[i]);
        if (!sim->pcb_table[i]) {
            for (size_t j = 0; j < i; j++) pcb_destroy(sim->pcb_table[j]);
            free(sim->pcb_table);
            free(sim);
            return NULL;
        }
    }

    /* Allouer le tableau Gantt */
    sim->gantt          = malloc(GANTT_INIT_CAP * sizeof(GanttEntry));
    sim->gantt_capacity = GANTT_INIT_CAP;
    sim->gantt_count    = 0;
    if (!sim->gantt) { perror("sim_create: gantt"); }

    return sim;
}

/**
 * @brief Admet les processus dont la date d'arrivée correspond à clock_ms.
 *
 * Transition NEW → READY et appel à scheduler->admit().
 *
 * @param sim  État de simulation.
 */
void sim_tick_admit_arrivals(SimulationState *sim)
{
    for (size_t i = 0; i < sim->num_processes; i++) {
        PCB *pcb = sim->pcb_table[i];
        if (pcb->state == STATE_NEW &&
            pcb->proc->arrival_time_ms == sim->clock_ms)
        {
            sim->scheduler->admit(sim->scheduler, pcb, sim->clock_ms);
        }
    }
}

/**
 * @brief Avance les E/S de 1 ms. Remet les processus terminés en READY.
 *
 * @param sim  État de simulation.
 */
void sim_tick_advance_io(SimulationState *sim)
{
    if (sim->sequential_io) {
        /* Mode E/S séquentiel : un seul device, seule la tête de io_queue avance.
         * Les autres processus BLOCKED attendent dans la file. */

        /* Enregistrer tous les processus BLOCKED dans le Gantt (attente ou actif) */
        for (size_t i = 0; i < sim->num_processes; i++) {
            if (sim->pcb_table[i]->state == STATE_BLOCKED)
                gantt_push(sim, sim->pcb_table[i]->proc->pid, 1);
        }

        PCB *head = queue_peek(&sim->io_queue);
        if (!head) return;

        head->remaining_io_ms--;
        head->total_io_ms++;

        if (head->remaining_io_ms == 0) {
            queue_dequeue(&sim->io_queue);
            head->current_burst_idx++;
            if (head->current_burst_idx < head->proc->num_bursts) {
                head->remaining_cpu_ms =
                    head->proc->burst_table[head->current_burst_idx].cpu_duration_ms;
            }
            sim->scheduler->on_io_done(sim->scheduler, head, sim->clock_ms);
        }
    } else {
        /* Mode E/S parallèle (défaut) : toutes les E/S progressent simultanément */
        for (size_t i = 0; i < sim->num_processes; i++) {
            PCB *pcb = sim->pcb_table[i];
            if (pcb->state != STATE_BLOCKED) continue;

            gantt_push(sim, pcb->proc->pid, 1);
            pcb->remaining_io_ms--;
            pcb->total_io_ms++;

            if (pcb->remaining_io_ms == 0) {
                /* Passer au burst CPU suivant */
                pcb->current_burst_idx++;
                if (pcb->current_burst_idx < pcb->proc->num_bursts) {
                    pcb->remaining_cpu_ms =
                        pcb->proc->burst_table[pcb->current_burst_idx].cpu_duration_ms;
                }
                sim->scheduler->on_io_done(sim->scheduler, pcb, sim->clock_ms);
            }
        }
    }
}

/**
 * @brief Vérifie si le processus courant doit être préempté.
 *
 * Deux cas de préemption :
 *  1. Le quantum est épuisé (quantum_ms > 0 && quantum_elapsed_ms >= quantum_ms).
 *  2. L'algorithme signale une préemption via should_preempt() (ex: SRJF).
 *
 * Si préemption, replace le processus dans la file via on_yield(YIELD_PREEMPTED).
 *
 * @param sim  État de simulation.
 */
void sim_tick_check_preemption(SimulationState *sim)
{
    if (!sim->running) return;
    if (!sim->scheduler->is_preemptive) return;

    int preempt = 0;

    /* Cas 1 : quantum épuisé (Round Robin) */
    if (sim->scheduler->quantum_ms > 0 &&
        sim->quantum_elapsed_ms >= sim->scheduler->quantum_ms)
    {
        preempt = 1;
    }

    /* Cas 2 : décision de l'algorithme (SRJF) */
    if (!preempt && sim->scheduler->should_preempt(sim->scheduler,
                                                    sim->running,
                                                    sim->clock_ms))
    {
        preempt = 1;
    }

    if (preempt) {
        PCB *preempted = sim->running;
        sim->running   = NULL;
        sim->quantum_elapsed_ms = 0;
        pcb_transition(preempted, STATE_READY, sim->clock_ms);
        sim->scheduler->on_yield(sim->scheduler, preempted,
                                 sim->clock_ms, YIELD_PREEMPTED);
    }
}

/**
 * @brief Exécute 1 ms CPU pour le processus courant.
 *
 * @param sim  État de simulation.
 */
void sim_tick_run_cpu(SimulationState *sim)
{
    if (!sim->running) return;
    sim->running->remaining_cpu_ms--;
    sim->running->total_cpu_ms++;
    sim->quantum_elapsed_ms++;
    sim->cpu_busy_ms++;
}

/**
 * @brief Enregistre le tick courant dans le diagramme de Gantt.
 *
 * @param sim   État de simulation.
 * @param idle  1 si le CPU est inactif ce tick, 0 sinon.
 */
void sim_tick_record_gantt(SimulationState *sim, int idle)
{
    uint32_t pid = idle ? 0 : (sim->running ? sim->running->proc->pid : 0);
    gantt_push(sim, pid, 0);
}

/**
 * @brief Lance la simulation jusqu'à la terminaison de tous les processus.
 *
 * Remplit sim->gantt et sim->report à la fin.
 *
 * @param sim  État de simulation.
 * @return     0 en cas de succès.
 */
int sim_run(SimulationState *sim)
{
    if (!sim) return -1;

    /* Initialisation de l'algorithme */
    sim->scheduler->init(sim->scheduler);

    /* Boucle principale */
    while (sim->terminated_count < sim->num_processes) {

        /* 1. Admettre les nouveaux arrivants */
        sim_tick_admit_arrivals(sim);

        /* 2. Avancer les E/S */
        sim_tick_advance_io(sim);

        /* 3. Vérifier la préemption */
        sim_tick_check_preemption(sim);

        /* 4. Sélectionner un processus si CPU libre */
        if (!sim->running) {
            PCB *next = sim->scheduler->pick_next(sim->scheduler, sim->clock_ms);
            if (next) {
                sim->running = next;
                sim->quantum_elapsed_ms = 0;
                pcb_transition(next, STATE_RUNNING, sim->clock_ms);
            }
        }

        /* 5. Exécuter 1 ms CPU */
        int idle = (sim->running == NULL);
        sim_tick_run_cpu(sim);

        /* 6. Enregistrer le Gantt */
        sim_tick_record_gantt(sim, idle);

        /* 7. Gérer la fin d'un burst CPU */
        if (sim->running && sim->running->remaining_cpu_ms == 0) {
            PCB *done = sim->running;
            sim->running = NULL;
            sim->quantum_elapsed_ms = 0;

            uint32_t burst_idx = done->current_burst_idx;
            uint32_t io_dur    = done->proc->burst_table[burst_idx].io_duration_ms;

            if (io_dur > 0 && burst_idx + 1 < done->proc->num_bursts) {
                /* Passage en E/S */
                done->remaining_io_ms = io_dur;
                pcb_transition(done, STATE_BLOCKED, sim->clock_ms + 1);
                /* En mode séquentiel, ajouter à la file d'attente du device */
                if (sim->sequential_io)
                    queue_enqueue(&sim->io_queue, done);
                sim->scheduler->on_yield(sim->scheduler, done,
                                         sim->clock_ms + 1, YIELD_IO_START);
            } else {
                /* Dernier burst ou pas d'E/S : terminaison */
                pcb_transition(done, STATE_TERMINATED, sim->clock_ms + 1);
                sim->scheduler->on_yield(sim->scheduler, done,
                                         sim->clock_ms + 1, YIELD_CPU_BURST_DONE);
                sim->terminated_count++;
            }
        }

        sim->clock_ms++;
    }

    /* Calculer les métriques */
    sim->report = metrics_compute(
        sim->pcb_table,
        sim->num_processes,
        sim->clock_ms,
        sim->cpu_busy_ms,
        sim->scheduler->name,
        sim->scheduler->short_name,
        sim->scheduler->quantum_ms
    );

    return 0;
}

/**
 * @brief Libère un SimulationState et ses ressources internes.
 *
 * Ne libère pas le Scheduler ni le tableau de Process.
 *
 * @param sim  État à libérer.
 */
void sim_destroy(SimulationState *sim)
{
    if (!sim) return;
    if (sim->pcb_table) {
        for (size_t i = 0; i < sim->num_processes; i++) {
            pcb_destroy(sim->pcb_table[i]);
        }
        free(sim->pcb_table);
    }
    free(sim->gantt);
    queue_destroy(&sim->io_queue);
    metrics_destroy(sim->report);
    free(sim);
}
