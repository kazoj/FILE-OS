/**
 * @file fifo.c
 * @brief Algorithme d'ordonnancement FIFO (First In First Out).
 *
 * Non préemptif. Le premier processus arrivé dans la file est
 * le premier servi. En cas d'arrivée simultanée, tie-break par PID.
 *
 * État interne : une simple Queue FIFO (voir queue.h).
 */

#include "algorithms.h"
#include "queue.h"
#include "process.h"
#include <stdlib.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/*  Implémentation de la vtable                                         */
/* ------------------------------------------------------------------ */

/**
 * @brief Initialise la Queue FIFO interne.
 * @param s  Instance du scheduler (private_data sera une Queue *).
 */
static void fifo_do_init(Scheduler *s)
{
    Queue *q = malloc(sizeof(Queue));
    if (!q) { perror("fifo_init"); return; }
    queue_init(q);
    s->private_data = q;
}

/**
 * @brief Admet un processus en le plaçant en queue FIFO.
 * @param s    Instance du scheduler.
 * @param pcb  PCB du processus arrivant.
 * @param now  Temps courant (non utilisé par FIFO).
 */
static void fifo_admit(Scheduler *s, PCB *pcb, uint32_t now)
{
    (void)now;
    Queue *q = (Queue *)s->private_data;
    pcb_transition(pcb, STATE_READY, now);
    queue_enqueue(q, pcb);
}

/**
 * @brief Sélectionne le prochain processus à exécuter (tête de file).
 * @param s    Instance du scheduler.
 * @param now  Temps courant (non utilisé par FIFO).
 * @return     PCB en tête de file, ou NULL si vide.
 */
static PCB *fifo_pick_next(Scheduler *s, uint32_t now)
{
    (void)now;
    Queue *q = (Queue *)s->private_data;
    return queue_dequeue(q);
}

/**
 * @brief Gère la libération du CPU par un processus.
 *
 * - YIELD_CPU_BURST_DONE : le processus a terminé son burst. Si ce n'est
 *   pas le dernier burst, il partira en E/S (géré par la simulation).
 *   On ne le remet pas en file ici.
 * - YIELD_IO_START       : même traitement (le processus est mis BLOCKED
 *   par la simulation).
 * - YIELD_PREEMPTED      : ne se produit jamais pour FIFO (non préemptif).
 *
 * @param s       Instance du scheduler.
 * @param pcb     PCB libérant le CPU.
 * @param now     Temps courant.
 * @param reason  Raison de la libération.
 */
static void fifo_on_yield(Scheduler *s, PCB *pcb, uint32_t now, int reason)
{
    (void)s; (void)pcb; (void)now; (void)reason;
    /* FIFO ne remet jamais un processus en file lors d'un yield :
     * la simulation s'occupe des transitions d'état. */
}

/**
 * @brief Remet un processus en file FIFO après la fin de son E/S.
 * @param s    Instance du scheduler.
 * @param pcb  PCB dont l'E/S est terminée.
 * @param now  Temps courant.
 */
static void fifo_on_io_done(Scheduler *s, PCB *pcb, uint32_t now)
{
    Queue *q = (Queue *)s->private_data;
    pcb_transition(pcb, STATE_READY, now);
    queue_enqueue(q, pcb);
}

/**
 * @brief FIFO n'est pas préemptif — retourne toujours 0.
 */
static int fifo_should_preempt(Scheduler *s, const PCB *running, uint32_t now)
{
    (void)s; (void)running; (void)now;
    return 0;
}

/**
 * @brief Libère la Queue interne.
 * @param s  Instance du scheduler.
 */
static void fifo_destroy(Scheduler *s)
{
    if (!s || !s->private_data) return;
    Queue *q = (Queue *)s->private_data;
    queue_destroy(q);
    free(q);
    s->private_data = NULL;
}

/* ------------------------------------------------------------------ */
/*  Fonction d'initialisation publique                                  */
/* ------------------------------------------------------------------ */

/**
 * @brief Remplit une instance de Scheduler avec la vtable FIFO.
 *
 * @param s          Instance à remplir (déjà allouée par scheduler_create).
 * @param quantum_ms Ignoré pour FIFO.
 */
void fifo_init(Scheduler *s, uint32_t quantum_ms)
{
    (void)quantum_ms;
    s->name          = "First In First Out";
    s->short_name    = "FIFO";
    s->is_preemptive = 0;
    s->quantum_ms    = 0;

    s->init           = fifo_do_init;
    s->admit          = fifo_admit;
    s->pick_next      = fifo_pick_next;
    s->on_yield       = fifo_on_yield;
    s->on_io_done     = fifo_on_io_done;
    s->should_preempt = fifo_should_preempt;
    s->destroy        = fifo_destroy;

    s->private_data   = NULL; /* alloué dans fifo_do_init */
}
