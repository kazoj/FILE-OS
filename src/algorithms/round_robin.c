/**
 * @file round_robin.c
 * @brief Algorithme d'ordonnancement Round Robin.
 *
 * Préemptif. Chaque processus reçoit un quantum de CPU. S'il n'a pas
 * terminé son burst à la fin du quantum, il est replacé en queue FIFO.
 * Quantum par défaut : 50 ms.
 *
 * État interne : struct { Queue q; uint32_t quantum_ms; }
 */

#include "algorithms.h"
#include "queue.h"
#include "process.h"
#include <stdlib.h>
#include <stdio.h>

/** @brief État privé du Round Robin. */
typedef struct {
    Queue    q;          /**< File FIFO des processus prêts */
    uint32_t quantum_ms; /**< Durée du quantum en ms */
} RRData;

static void rr_do_init(Scheduler *s)
{
    RRData *d = malloc(sizeof(RRData));
    if (!d) { perror("rr_init"); return; }
    queue_init(&d->q);
    d->quantum_ms   = (s->quantum_ms > 0) ? s->quantum_ms : 50;
    s->private_data = d;
}

static void rr_admit(Scheduler *s, PCB *pcb, uint32_t now)
{
    RRData *d = (RRData *)s->private_data;
    pcb_transition(pcb, STATE_READY, now);
    queue_enqueue(&d->q, pcb);
}

static PCB *rr_pick_next(Scheduler *s, uint32_t now)
{
    (void)now;
    RRData *d = (RRData *)s->private_data;
    return queue_dequeue(&d->q);
}

/**
 * @brief Gère la libération du CPU.
 *
 * - YIELD_PREEMPTED : quantum expiré → replacer en queue FIFO.
 * - YIELD_IO_START / YIELD_CPU_BURST_DONE : processus bloqué ou terminé,
 *   ne pas remettre en file (la simulation gère la transition).
 *
 * @param s       Scheduler.
 * @param pcb     PCB libérant le CPU.
 * @param now     Temps courant.
 * @param reason  Raison de la libération.
 */
static void rr_on_yield(Scheduler *s, PCB *pcb, uint32_t now, int reason)
{
    if (reason == YIELD_PREEMPTED) {
        /* Le processus est déjà READY (géré par sim_tick_check_preemption) */
        RRData *d = (RRData *)s->private_data;
        queue_enqueue(&d->q, pcb);
    }
    (void)now;
}

static void rr_on_io_done(Scheduler *s, PCB *pcb, uint32_t now)
{
    RRData *d = (RRData *)s->private_data;
    pcb_transition(pcb, STATE_READY, now);
    queue_enqueue(&d->q, pcb);
}

/**
 * @brief Détermine si le quantum du processus courant est épuisé.
 *
 * Utilise sim->quantum_elapsed_ms via le champ du Scheduler qui est
 * passé en contexte. En pratique, la simulation maintient
 * `quantum_elapsed_ms` et le RR vérifie ici.
 *
 * Note : la simulation expose quantum_elapsed_ms dans SimulationState,
 * mais la vtable ne passe que (Scheduler*, PCB*, now). Pour éviter un
 * couplage, on passe le quantum_ms dans le Scheduler et la simulation
 * compare elle-même quantum_elapsed_ms >= s->quantum_ms pour YIELD_PREEMPTED.
 * Cette fonction est donc appelée depuis sim_tick_check_preemption qui
 * compare quantum_elapsed_ms avec s->quantum_ms.
 *
 * @param s        Scheduler.
 * @param running  PCB en cours.
 * @param now      Temps courant.
 * @return         1 si préemption requise, 0 sinon.
 */
static int rr_should_preempt(Scheduler *s, const PCB *running, uint32_t now)
{
    (void)running; (void)now;
    RRData *d = (RRData *)s->private_data;
    /*
     * La simulation passe quantum_elapsed_ms dans le contexte global.
     * Ici on retourne 0 : la logique de préemption RR est gérée directement
     * dans sim_tick_check_preemption qui compare quantum_elapsed_ms >= quantum_ms.
     * Cette fonction sert de fallback pour les algos qui veulent gérer eux-mêmes.
     */
    (void)d;
    (void)s;
    return 0;
}

static void rr_destroy(Scheduler *s)
{
    if (!s || !s->private_data) return;
    RRData *d = (RRData *)s->private_data;
    queue_destroy(&d->q);
    free(d);
    s->private_data = NULL;
}

/**
 * @brief Remplit une instance de Scheduler avec la vtable Round Robin.
 * @param s          Instance à remplir.
 * @param quantum_ms Durée du quantum (défaut 50 ms si 0).
 */
void round_robin_init(Scheduler *s, uint32_t quantum_ms)
{
    s->name          = "Round Robin";
    s->short_name    = "RR";
    s->is_preemptive = 1;
    s->quantum_ms    = (quantum_ms > 0) ? quantum_ms : 50;

    s->init           = rr_do_init;
    s->admit          = rr_admit;
    s->pick_next      = rr_pick_next;
    s->on_yield       = rr_on_yield;
    s->on_io_done     = rr_on_io_done;
    s->should_preempt = rr_should_preempt;
    s->destroy        = rr_destroy;

    s->private_data   = NULL;
}
