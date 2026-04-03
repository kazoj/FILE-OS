/**
 * @file metrics.c
 * @brief Calcul des indicateurs de performance après simulation.
 */

#include "metrics.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Calcule toutes les métriques à partir des PCBs finalisés.
 *
 * Formules appliquées :
 *  - turnaround = finish_time - arrival_time
 *  - waiting    = total_wait_ms (accumulé pendant la simulation)
 *  - response   = first_run_time - arrival_time
 *  - cpu_util   = (cpu_busy_ms / sim_duration) × 100
 *
 * @param pcbs         Tableau de pointeurs vers les PCBs.
 * @param n            Nombre de processus.
 * @param sim_duration Durée totale de la simulation en ms.
 * @param cpu_busy_ms  Ticks où le CPU était occupé.
 * @param algo_name    Nom complet de l'algorithme.
 * @param algo_short   Abréviation.
 * @param quantum_ms   Quantum RR (0 si sans objet).
 * @return             MetricsReport alloué, ou NULL en cas d'erreur.
 */
MetricsReport *metrics_compute(PCB **pcbs, size_t n,
                               uint32_t sim_duration,
                               uint32_t cpu_busy_ms,
                               const char *algo_name,
                               const char *algo_short,
                               uint32_t quantum_ms)
{
    if (!pcbs || n == 0) return NULL;

    MetricsReport *r = calloc(1, sizeof(MetricsReport));
    if (!r) { perror("metrics_compute"); return NULL; }

    r->per_process = calloc(n, sizeof(ProcessMetrics));
    if (!r->per_process) { free(r); return NULL; }

    r->num_processes      = n;
    r->total_simulation_ms = sim_duration;
    r->algorithm_name     = algo_name;
    r->algorithm_short    = algo_short;
    r->quantum_ms         = quantum_ms;

    double sum_turnaround = 0.0;
    double sum_waiting    = 0.0;
    double sum_response   = 0.0;

    for (size_t i = 0; i < n; i++) {
        PCB           *pcb = pcbs[i];
        ProcessMetrics *pm = &r->per_process[i];

        pm->pid            = pcb->proc->pid;
        strncpy(pm->name, pcb->proc->name, sizeof(pm->name) - 1);
        pm->arrival_time_ms    = pcb->arrival_time_ms;
        pm->finish_time_ms     = pcb->finish_time_ms;
        pm->turnaround_time_ms = pcb->finish_time_ms - pcb->arrival_time_ms;
        pm->waiting_time_ms    = pcb->total_wait_ms;
        pm->response_time_ms   = pcb->first_run_time_ms - pcb->arrival_time_ms;
        pm->total_cpu_ms       = pcb->total_cpu_ms;
        pm->total_io_ms        = pcb->total_io_ms;

        sum_turnaround += pm->turnaround_time_ms;
        sum_waiting    += pm->waiting_time_ms;
        sum_response   += pm->response_time_ms;
    }

    r->avg_turnaround_ms  = sum_turnaround / (double)n;
    r->avg_waiting_ms     = sum_waiting    / (double)n;
    r->avg_response_ms    = sum_response   / (double)n;
    r->cpu_utilization_pct = (sim_duration > 0)
        ? ((double)cpu_busy_ms / (double)sim_duration) * 100.0
        : 0.0;

    return r;
}

/**
 * @brief Libère un MetricsReport et son tableau interne.
 * @param report  Rapport à libérer (peut être NULL).
 */
void metrics_destroy(MetricsReport *report)
{
    if (!report) return;
    free(report->per_process);
    free(report);
}
