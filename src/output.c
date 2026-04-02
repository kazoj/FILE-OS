/**
 * @file output.c
 * @brief Rendu des résultats : table terminale, Gantt ASCII, CSV.
 */

#include "output.h"
#include "simulation.h"
#include "metrics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef HAVE_NCURSES
#  include "output_ncurses.h"
#endif

/* ------------------------------------------------------------------ */
/*  Table des métriques                                                */
/* ------------------------------------------------------------------ */

/**
 * @brief Affiche la table des métriques sur un flux.
 *
 * Colonnes : PID | Nom | Arrivée | Fin | Restitution | Attente | Réponse | CPU | E/S
 *
 * @param report  Rapport de métriques.
 * @param stream  Flux de sortie.
 */
void output_print_table(const MetricsReport *report, FILE *stream)
{
    if (!report || !stream) return;

    fprintf(stream, "\n");
    fprintf(stream, "=== Résultats : %s", report->algorithm_name);
    if (report->quantum_ms > 0)
        fprintf(stream, " (quantum=%u ms)", report->quantum_ms);
    fprintf(stream, " ===\n\n");

    /* En-tête */
    fprintf(stream,
        "%-5s %-16s %8s %8s %12s %8s %9s %8s %8s\n",
        "PID", "Nom",
        "Arrivée", "Fin",
        "Restitution", "Attente", "Réponse",
        "CPU(ms)", "E/S(ms)");
    fprintf(stream,
        "%-5s %-16s %8s %8s %12s %8s %9s %8s %8s\n",
        "---", "---",
        "(ms)", "(ms)",
        "(ms)", "(ms)", "(ms)",
        "", "");
    fprintf(stream, "%s\n",
        "----------------------------------------------------------------------"
        "-------------------");

    /* Lignes par processus */
    for (size_t i = 0; i < report->num_processes; i++) {
        const ProcessMetrics *pm = &report->per_process[i];
        fprintf(stream,
            "%-5u %-16s %8u %8u %12u %8u %9u %8u %8u\n",
            pm->pid,
            pm->name,
            pm->arrival_time_ms,
            pm->finish_time_ms,
            pm->turnaround_time_ms,
            pm->waiting_time_ms,
            pm->response_time_ms,
            pm->total_cpu_ms,
            pm->total_io_ms);
    }

    /* Ligne de séparation + moyennes */
    fprintf(stream, "%s\n",
        "----------------------------------------------------------------------"
        "-------------------");
    fprintf(stream,
        "%-5s %-16s %8s %8s %12.2f %8.2f %9.2f\n",
        "MOY", "",
        "", "",
        report->avg_turnaround_ms,
        report->avg_waiting_ms,
        report->avg_response_ms);

    fprintf(stream, "\nTaux d'occupation CPU : %.1f%%\n",
            report->cpu_utilization_pct);
    fprintf(stream, "Durée totale de simulation : %u ms\n\n",
            report->total_simulation_ms);
}

/* ------------------------------------------------------------------ */
/*  Diagramme de Gantt ASCII compact                                   */
/* ------------------------------------------------------------------ */

/**
 * @brief Affiche le diagramme de Gantt ASCII compact.
 *
 * Format :
 * ```
 * [FIFO]  0ms → 860ms
 *
 * PID   0    100  200  300  400
 *   1  [CPU ][CPU ][I/O ]
 *   2  [    ][CPU ]
 * IDLE [    ]
 * ```
 *
 * Chaque cellule représente une tranche de `col_width` ms.
 * Si un intervalle Gantt ne s'aligne pas exactement, on arrondit.
 *
 * @param gantt   Tableau d'entrées Gantt.
 * @param count   Nombre d'entrées.
 * @param report  Rapport (nom algo, quantum, processus).
 * @param stream  Flux de sortie.
 */
void output_print_gantt(const GanttEntry    *gantt,
                        size_t               count,
                        const MetricsReport *report,
                        FILE                *stream)
{
    if (!gantt || count == 0 || !report || !stream) return;

    uint32_t total_ms  = report->total_simulation_ms;
    /* Largeur d'une colonne = quantum ou 50 ms par défaut */
    uint32_t col_ms    = (report->quantum_ms > 0) ? report->quantum_ms : 50;
    uint32_t num_cols  = (total_ms + col_ms - 1) / col_ms;
    if (num_cols == 0) num_cols = 1;

    fprintf(stream, "\n--- Diagramme de Gantt : %s", report->algorithm_name);
    if (report->quantum_ms > 0)
        fprintf(stream, " (q=%u ms)", report->quantum_ms);
    fprintf(stream, "  [0 → %u ms] ---\n\n", total_ms);

    /* Ligne d'en-tête des temps */
    fprintf(stream, "     ");
    for (uint32_t c = 0; c < num_cols; c++) {
        fprintf(stream, "%-5u", c * col_ms);
    }
    fprintf(stream, "%u\n", total_ms);

    /* Une ligne par processus */
    for (size_t pi = 0; pi < report->num_processes; pi++) {
        uint32_t pid = report->per_process[pi].pid;
        fprintf(stream, "%4u ", pid);

        for (uint32_t c = 0; c < num_cols; c++) {
            uint32_t col_start = c * col_ms;
            uint32_t col_end   = col_start + col_ms;
            /* Trouver l'entrée Gantt couvrant le milieu de cette colonne */
            uint32_t mid = col_start + col_ms / 2;
            const char *label = "    "; /* vide */

            for (size_t gi = 0; gi < count; gi++) {
                if (gantt[gi].pid == pid &&
                    gantt[gi].start_ms <= mid &&
                    mid < gantt[gi].end_ms)
                {
                    label = gantt[gi].is_io ? "I/O " : "CPU ";
                    break;
                }
                (void)col_end;
            }
            fprintf(stream, "[%s]", label);
        }
        fprintf(stream, "\n");
    }

    /* Ligne IDLE */
    fprintf(stream, "IDLE ");
    for (uint32_t c = 0; c < num_cols; c++) {
        uint32_t mid = c * col_ms + col_ms / 2;
        const char *label = "    ";
        for (size_t gi = 0; gi < count; gi++) {
            if (gantt[gi].pid == 0 &&
                gantt[gi].start_ms <= mid &&
                mid < gantt[gi].end_ms)
            {
                label = "IDLE";
                break;
            }
        }
        fprintf(stream, "[%s]", label);
    }
    fprintf(stream, "\n\n");
}

/* ------------------------------------------------------------------ */
/*  Export CSV                                                         */
/* ------------------------------------------------------------------ */

/**
 * @brief Génère un nom de fichier CSV automatique.
 *
 * Format : "<algo_short>_YYYYMMDD_HHMMSS.csv"
 *
 * @param buf   Buffer de sortie.
 * @param size  Taille du buffer.
 * @param algo  Abréviation de l'algorithme.
 */
static void csv_auto_name(char *buf, size_t size, const char *algo)
{
    time_t    now = time(NULL);
    struct tm *t  = localtime(&now);
    snprintf(buf, size, "%s_%04d%02d%02d_%02d%02d%02d.csv",
             algo ? algo : "result",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);
}

/**
 * @brief Écrit le rapport de métriques dans un fichier CSV.
 *
 * En-tête :
 *   algorithm,quantum_ms,pid,name,arrival_ms,finish_ms,
 *   turnaround_ms,waiting_ms,response_ms,cpu_ms,io_ms
 *
 * Dernières lignes : moyennes et taux d'occupation.
 *
 * @param report  Rapport de métriques.
 * @param path    Chemin cible (NULL = nom automatique).
 * @return        0 si succès, -1 si erreur.
 */
int output_write_csv(const MetricsReport *report, const char *path)
{
    if (!report) return -1;

    char auto_name[128];
    if (!path) {
        csv_auto_name(auto_name, sizeof(auto_name), report->algorithm_short);
        path = auto_name;
    }

    FILE *f = fopen(path, "w");
    if (!f) {
        perror(path);
        return -1;
    }

    /* En-tête CSV */
    fprintf(f, "algorithm,quantum_ms,pid,name,"
               "arrival_ms,finish_ms,turnaround_ms,"
               "waiting_ms,response_ms,cpu_ms,io_ms\n");

    /* Données par processus */
    for (size_t i = 0; i < report->num_processes; i++) {
        const ProcessMetrics *pm = &report->per_process[i];
        fprintf(f, "%s,%u,%u,%s,%u,%u,%u,%u,%u,%u,%u\n",
                report->algorithm_short,
                report->quantum_ms,
                pm->pid,
                pm->name,
                pm->arrival_time_ms,
                pm->finish_time_ms,
                pm->turnaround_time_ms,
                pm->waiting_time_ms,
                pm->response_time_ms,
                pm->total_cpu_ms,
                pm->total_io_ms);
    }

    /* Ligne de moyennes */
    fprintf(f, "%s,%u,AVG,,,,%.2f,%.2f,%.2f,,\n",
            report->algorithm_short,
            report->quantum_ms,
            report->avg_turnaround_ms,
            report->avg_waiting_ms,
            report->avg_response_ms);

    /* Taux d'occupation CPU */
    fprintf(f, "%s,%u,CPU_UTIL,,,,,,,%.2f%%,\n",
            report->algorithm_short,
            report->quantum_ms,
            report->cpu_utilization_pct);

    fclose(f);
    fprintf(stdout, "CSV sauvegardé : %s\n", path);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Génération de graphiques Python                                    */
/* ------------------------------------------------------------------ */

/**
 * @brief Lance le script Python de visualisation sur le CSV produit.
 *
 * Cherche `tools/plot_results.py` dans le répertoire courant.
 * Échec non-fatal : affiche un avertissement seulement.
 *
 * @param csv_path  Chemin du fichier CSV à traiter.
 */
void output_plot(const char *csv_path)
{
    if (!csv_path) return;

    fflush(stdout); /* éviter l'entrelacement stdout/subprocess */

    char cmd[768];
    snprintf(cmd, sizeof(cmd),
             "python3 tools/plot_results.py \"%s\"", csv_path);

    int rc = system(cmd);
    if (rc != 0) {
        fprintf(stderr,
            "Avertissement : génération des graphiques échouée "
            "(Python3/matplotlib absent ou tools/plot_results.py introuvable).\n");
    }
}

/* ------------------------------------------------------------------ */
/*  IHM ncurses (stub si non compilé)                                  */
/* ------------------------------------------------------------------ */

#ifndef HAVE_NCURSES
/**
 * @brief Stub ncurses : affiché si la lib n'est pas disponible.
 */
void output_ncurses_render(const SimulationState *sim,
                           const MetricsReport   *report)
{
    (void)sim; (void)report;
    fprintf(stderr,
        "Avertissement : l'option -i requiert ncurses "
        "(recompilez avec libncurses installée).\n");
}
#endif

/* ------------------------------------------------------------------ */
/*  Rendu global                                                       */
/* ------------------------------------------------------------------ */

/**
 * @brief Effectue tous les rendus demandés dans la configuration.
 *
 * @param sim     Simulation terminée.
 * @param report  Rapport de métriques.
 * @param cfg     Configuration de sortie.
 * @return        0 si succès, -1 si erreur.
 */
int output_render(const SimulationState *sim,
                  const MetricsReport   *report,
                  const OutputConfig    *cfg)
{
    if (!sim || !report || !cfg) return -1;
    int rc = 0;

    if (cfg->show_table) {
        output_print_table(report, stdout);
    }

    if (cfg->show_gantt) {
        output_print_gantt(sim->gantt, sim->gantt_count, report, stdout);
    }

    /* Toujours écrire le CSV */
    char auto_csv[128] = {0};
    if (!cfg->csv_path) {
        /* Mémoriser le nom généré pour le passer à output_plot */
        time_t    now = time(NULL);
        struct tm *t  = localtime(&now);
        snprintf(auto_csv, sizeof(auto_csv), "%s_%04d%02d%02d_%02d%02d%02d.csv",
                 report->algorithm_short ? report->algorithm_short : "result",
                 t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                 t->tm_hour, t->tm_min, t->tm_sec);
    }
    const char *csv_used = cfg->csv_path ? cfg->csv_path : auto_csv;
    rc = output_write_csv(report, cfg->csv_path);

    /* Graphiques Python */
    if (cfg->auto_plot && rc == 0) {
        output_plot(csv_used);
    }

    /* IHM ncurses interactive */
    if (cfg->show_ncurses) {
        output_ncurses_render(sim, report);
    }

    return rc;
}
