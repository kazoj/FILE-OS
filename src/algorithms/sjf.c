/**
 * @file sjf.c
 * @brief Algorithme d'ordonnancement SJF (Shortest Job First).
 *
 * Non préemptif. Parmi les processus prêts, choisit celui dont le
 * burst CPU courant est le plus court. En cas d'égalité, tie-break
 * par PID croissant.
 *
 * État interne : une PriorityQueue triée par remaining_cpu_ms.
 */

#include "algorithms.h"
#include "queue.h"
#include "process.h"
#include <stdlib.h>
#include <stdio.h>

static void sjf_do_init(Scheduler *s)
{
    PriorityQueue *pq = malloc(sizeof(PriorityQueue));
    if (!pq) { perror("sjf_init"); return; }
    pqueue_init(pq, cmp_remaining_cpu);
    s->private_data = pq;
}

static void sjf_admit(Scheduler *s, PCB *pcb, uint32_t now)
{
    PriorityQueue *pq = (PriorityQueue *)s->private_data;
    pcb_transition(pcb, STATE_READY, now);
    pqueue_insert(pq, pcb);
}

static PCB *sjf_pick_next(Scheduler *s, uint32_t now)
{
    (void)now;
    PriorityQueue *pq = (PriorityQueue *)s->private_data;
    return pqueue_pop(pq);
}

static void sjf_on_yield(Scheduler *s, PCB *pcb, uint32_t now, int reason)
{
    (void)s; (void)pcb; (void)now; (void)reason;
    /* SJF non préemptif : pas de remise en file lors d'un yield normal */
}

static void sjf_on_io_done(Scheduler *s, PCB *pcb, uint32_t now)
{
    PriorityQueue *pq = (PriorityQueue *)s->private_data;
    pcb_transition(pcb, STATE_READY, now);
    pqueue_insert(pq, pcb);
}

static int sjf_should_preempt(Scheduler *s, const PCB *running, uint32_t now)
{
    (void)s; (void)running; (void)now;
    return 0; /* Non préemptif */
}

static void sjf_destroy(Scheduler *s)
{
    if (!s || !s->private_data) return;
    PriorityQueue *pq = (PriorityQueue *)s->private_data;
    pqueue_destroy(pq);
    free(pq);
    s->private_data = NULL;
}

/**
 * @brief Remplit une instance de Scheduler avec la vtable SJF.
 * @param s          Instance à remplir.
 * @param quantum_ms Ignoré.
 */
void sjf_init(Scheduler *s, uint32_t quantum_ms)
{
    (void)quantum_ms;
    s->name          = "Shortest Job First";
    s->short_name    = "SJF";
    s->is_preemptive = 0;
    s->quantum_ms    = 0;

    s->init           = sjf_do_init;
    s->admit          = sjf_admit;
    s->pick_next      = sjf_pick_next;
    s->on_yield       = sjf_on_yield;
    s->on_io_done     = sjf_on_io_done;
    s->should_preempt = sjf_should_preempt;
    s->destroy        = sjf_destroy;

    s->private_data   = NULL;
}
