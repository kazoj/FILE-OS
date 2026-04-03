/**
 * @file process.c
 * @brief Gestion du cycle de vie des processus et des PCBs.
 */

#include "process.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * @brief Alloue et initialise un PCB à partir d'un descripteur de Process.
 *
 * Le PCB est mis dans l'état NEW. Le premier burst CPU est chargé dans
 * remaining_cpu_ms.
 *
 * @param proc  Pointeur vers le Process statique (doit rester valide).
 * @return      PCB alloué sur le tas, ou NULL si l'allocation échoue.
 */
PCB *pcb_create(Process *proc)
{
    if (!proc) return NULL;

    PCB *pcb = calloc(1, sizeof(PCB));
    if (!pcb) {
        perror("pcb_create: calloc");
        return NULL;
    }

    pcb->proc               = proc;
    pcb->state              = STATE_NEW;
    pcb->current_burst_idx  = 0;
    pcb->arrival_time_ms    = proc->arrival_time_ms;
    pcb->has_run            = 0;
    pcb->next               = NULL;

    if (proc->num_bursts > 0) {
        pcb->remaining_cpu_ms = proc->burst_table[0].cpu_duration_ms;
        pcb->remaining_io_ms  = 0;
    }

    return pcb;
}

/**
 * @brief Libère la mémoire d'un PCB.
 *
 * Ne libère pas le Process pointé par pcb->proc.
 *
 * @param pcb  PCB à libérer (peut être NULL sans effet).
 */
void pcb_destroy(PCB *pcb)
{
    free(pcb);
}

/**
 * @brief Effectue une transition d'état sur un PCB.
 *
 * Met à jour les compteurs de temps selon la transition effectuée :
 * - READY → RUNNING  : enregistre first_run_time_ms si premier accès.
 * - RUNNING → READY  : accumule total_wait_ms depuis last_ready_time_ms.
 * - * → READY        : mémorise last_ready_time_ms.
 * - * → TERMINATED   : enregistre finish_time_ms.
 *
 * @param pcb       PCB à modifier.
 * @param new_state Nouvel état cible.
 * @param now_ms    Temps courant de simulation en ms.
 */
void pcb_transition(PCB *pcb, ProcessState new_state, uint32_t now_ms)
{
    if (!pcb) return;

    /* Accumulation du temps d'attente : fin d'un séjour en READY */
    if (pcb->state == STATE_READY && new_state == STATE_RUNNING) {
        pcb->total_wait_ms += now_ms - pcb->last_ready_time_ms;
        if (!pcb->has_run) {
            pcb->first_run_time_ms = now_ms;
            pcb->has_run = 1;
        }
    }

    /* Début d'un séjour en READY : mémoriser l'heure d'entrée */
    if (new_state == STATE_READY) {
        pcb->last_ready_time_ms = now_ms;
    }

    /* Fin de simulation */
    if (new_state == STATE_TERMINATED) {
        pcb->finish_time_ms = now_ms;
    }

    pcb->state = new_state;
}

/**
 * @brief Retourne une représentation textuelle d'un état de processus.
 *
 * @param state  État à convertir.
 * @return       Chaîne statique constante.
 */
const char *process_state_str(ProcessState state)
{
    switch (state) {
        case STATE_NEW:        return "NEW";
        case STATE_READY:      return "READY";
        case STATE_RUNNING:    return "RUNNING";
        case STATE_BLOCKED:    return "BLOCKED";
        case STATE_TERMINATED: return "TERMINATED";
        default:               return "UNKNOWN";
    }
}
