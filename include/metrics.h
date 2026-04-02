/**
 * @file metrics.h
 * @brief Calcul et stockage des indicateurs de performance.
 *
 * Les métriques sont calculées une seule fois après la simulation,
 * à partir des PCBs finalisés. Le `MetricsReport` sert ensuite de
 * source unique pour tous les rendus (table terminale, CSV, Gantt).
 */

#ifndef METRICS_H
#define METRICS_H

#include "process.h"
#include <stddef.h>

/**
 * @brief Métriques individuelles d'un processus.
 */
typedef struct {
    uint32_t pid;                  /**< Identifiant du processus */
    char     name[64];             /**< Nom du processus */
    uint32_t arrival_time_ms;      /**< Date d'arrivée */
    uint32_t finish_time_ms;       /**< Date de fin */
    uint32_t turnaround_time_ms;   /**< finish - arrival */
    uint32_t waiting_time_ms;      /**< Temps passé à l'état READY */
    uint32_t response_time_ms;     /**< first_run - arrival */
    uint32_t total_cpu_ms;         /**< Temps CPU total consommé */
    uint32_t total_io_ms;          /**< Temps E/S total effectué */
} ProcessMetrics;

/**
 * @brief Rapport de performance pour une simulation complète.
 */
typedef struct {
    ProcessMetrics *per_process;       /**< Tableau de métriques par processus */
    size_t          num_processes;     /**< Taille du tableau per_process */

    double avg_turnaround_ms;          /**< Temps de restitution moyen */
    double avg_waiting_ms;             /**< Temps d'attente moyen */
    double avg_response_ms;            /**< Temps de réponse moyen */
    double cpu_utilization_pct;        /**< Taux d'occupation CPU en % */

    uint32_t    total_simulation_ms;   /**< Durée totale de la simulation */
    const char *algorithm_name;        /**< Nom de l'algorithme (pointeur statique) */
    const char *algorithm_short;       /**< Abréviation (pour CSV) */
    uint32_t    quantum_ms;            /**< Quantum RR, ou 0 */
} MetricsReport;

/**
 * @brief Calcule toutes les métriques à partir des PCBs finalisés.
 *
 * @param pcbs           Tableau de pointeurs vers les PCBs (finalisés).
 * @param n              Nombre de processus.
 * @param sim_duration   Durée totale de la simulation en ms.
 * @param cpu_busy_ms    Temps total où le CPU était occupé (pour le taux).
 * @param algo_name      Nom complet de l'algorithme.
 * @param algo_short     Abréviation de l'algorithme.
 * @param quantum_ms     Quantum RR, ou 0.
 * @return               Rapport alloué sur le tas, ou NULL en cas d'erreur.
 */
MetricsReport *metrics_compute(PCB **pcbs, size_t n,
                               uint32_t sim_duration,
                               uint32_t cpu_busy_ms,
                               const char *algo_name,
                               const char *algo_short,
                               uint32_t quantum_ms);

/**
 * @brief Libère un MetricsReport et son tableau interne.
 * @param report  Rapport à libérer (peut être NULL).
 */
void metrics_destroy(MetricsReport *report);

#endif /* METRICS_H */
