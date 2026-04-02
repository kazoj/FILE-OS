/**
 * @file srjf.c
 * @brief Algorithme d'ordonnancement SRJF (Shortest Remaining Job First).
 *
 * Variante préemptive de SJF. À chaque tick, si un processus prêt a
 * moins de temps CPU restant que le processus courant, il y a préemption.
 * Le processus préempté est replacé dans la PriorityQueue.
 *
 * État interne : une PriorityQueue triée par remaining_cpu_ms.
 */

#include "algorithms.h"
#include "queue.h"
#include "process.h"
#include <stdlib.h>
#include <stdio.h>

static void srjf_do_init(Scheduler *s)
{
    PriorityQueue *pq = malloc(sizeof(PriorityQueue));
    if (!pq) { perror("srjf_init"); return; }
    pqueue_init(pq, cmp_remaining_cpu);
    s->private_data = pq;
}

static void srjf_admit(Scheduler *s, PCB *pcb, uint32_t now)
{
    PriorityQueue *pq = (PriorityQueue *)s->private_data;
    pcb_transition(pcb, STATE_READY, now);
    pqueue_insert(pq, pcb);
}

static PCB *srjf_pick_next(Scheduler *s, uint32_t now)
{
    (void)now;
    PriorityQueue *pq = (PriorityQueue *)s->private_data;
    return pqueue_pop(pq);
}

/**
 * @brief Gère la libération du CPU.
 *
 * En cas de préemption, le processus est replacé dans la PriorityQueue
 * (il sera trié selon son remaining_cpu_ms courant).
 *
 * @param s       Scheduler.
 * @param pcb     PCB libérant le CPU.
 * @param now     Temps courant.
 * @param reason  Raison de la libération.
 */
static void srjf_on_yield(Scheduler *s, PCB *pcb, uint32_t now, int reason)
{
    if (reason == YIELD_PREEMPTED) {
        /* Le processus est déjà passé à READY par sim_tick_check_preemption */
        PriorityQueue *pq = (PriorityQueue *)s->private_data;
        pqueue_insert(pq, pcb);
    }
    (void)now;
}

static void srjf_on_io_done(Scheduler *s, PCB *pcb, uint32_t now)
{
    PriorityQueue *pq = (PriorityQueue *)s->private_data;
    pcb_transition(pcb, STATE_READY, now);
    pqueue_insert(pq, pcb);
}

/**
 * @brief Détermine si le processus courant doit être préempté.
 *
 * Préemption si le meilleur candidat en file a un remaining_cpu_ms
 * strictement inférieur à celui du processus courant.
 *
 * @param s        Scheduler.
 * @param running  PCB en cours.
 * @param now      Temps courant.
 * @return         1 si préemption requise, 0 sinon.
 */
static int srjf_should_preempt(Scheduler *s, const PCB *running, uint32_t now)
{
    (void)now;
    PriorityQueue *pq   = (PriorityQueue *)s->private_data;
    const PCB     *best = pqueue_peek(pq);
    if (!best) return 0;
    return (best->remaining_cpu_ms < running->remaining_cpu_ms) ? 1 : 0;
}

static void srjf_destroy(Scheduler *s)
{
    if (!s || !s->private_data) return;
    PriorityQueue *pq = (PriorityQueue *)s->private_data;
    pqueue_destroy(pq);
    free(pq);
    s->private_data = NULL;
}

/**
 * @brief Remplit une instance de Scheduler avec la vtable SRJF.
 * @param s          Instance à remplir.
 * @param quantum_ms Ignoré.
 */
void srjf_init(Scheduler *s, uint32_t quantum_ms)
{
    (void)quantum_ms;
    s->name          = "Shortest Remaining Job First";
    s->short_name    = "SRJF";
    s->is_preemptive = 1;
    s->quantum_ms    = 0;

    s->init           = srjf_do_init;
    s->admit          = srjf_admit;
    s->pick_next      = srjf_pick_next;
    s->on_yield       = srjf_on_yield;
    s->on_io_done     = srjf_on_io_done;
    s->should_preempt = srjf_should_preempt;
    s->destroy        = srjf_destroy;

    s->private_data   = NULL;
}
