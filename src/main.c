/**
 * @file main.c
 * @brief Point d'entrée du simulateur d'ordonnancement de processus.
 *
 * ## Usage
 * ```
 * scheduler -a <algo> [-q <quantum>] [-g] [-f <fichier.sim>] [-o <sortie.csv>]
 * ```
 *
 * ## Exemples
 * ```
 * scheduler -a fifo -g -f tests/sample_inputs/basic.sim
 * scheduler -a rr -q 50 -g -f tests/sample_inputs/io_heavy.sim
 * scheduler -a srjf "200,50,100" "300" "150,30,50"
 * ```
 */

#include "input.h"
#include "scheduler.h"
#include "simulation.h"
#include "output.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    /* ---------------------------------------------------------------- */
    /*  1. Parsing de la ligne de commande                              */
    /* ---------------------------------------------------------------- */

    CLIConfig cfg;
    int parse_rc = cli_parse(argc, argv, &cfg);

    if (parse_rc == -2) {
        /* Option -l : lister les algos */
        printf("Algorithmes disponibles : ");
        scheduler_list_available();
        return 0;
    }
    if (parse_rc != 0) {
        return 1;
    }

    /* ---------------------------------------------------------------- */
    /*  2. Chargement des processus                                     */
    /* ---------------------------------------------------------------- */

    Process *procs      = NULL;
    size_t   n          = 0;
    uint32_t file_quantum = 0;

    if (cfg.input_file) {
        if (input_load_file(cfg.input_file, &procs, &n, &file_quantum) != 0) {
            fprintf(stderr, "Erreur lors du chargement de '%s'\n",
                    cfg.input_file);
            return 1;
        }
        /* Le quantum du fichier surcharge le défaut CLI si non spécifié */
        if (file_quantum > 0 && cfg.quantum_ms == 50) {
            cfg.quantum_ms = file_quantum;
        }
    } else {
        /* Processus depuis les arguments restants (non reconnus par cli_parse) */
        /* Rechercher les arguments positionnels restants après options */
        int    first_proc_arg = 0;
        char **proc_argv      = NULL;
        int    proc_argc      = 0;

        for (int i = 1; i < argc; i++) {
            if (argv[i][0] != '-') {
                if (!proc_argv) {
                    proc_argv   = &argv[i];
                    first_proc_arg = i;
                }
                proc_argc++;
            } else if (argv[i][1] == 'a' || argv[i][1] == 'q' ||
                       argv[i][1] == 'f' || argv[i][1] == 'o') {
                i++; /* sauter la valeur */
            }
        }
        (void)first_proc_arg;

        if (proc_argc == 0) {
            fprintf(stderr,
                "Aucun processus fourni. Utilisez -f <fichier.sim> ou "
                "spécifiez des bursts en argument.\n");
            cli_usage(argv[0]);
            return 1;
        }

        if (input_from_cli(proc_argv, proc_argc, &procs, &n) != 0) {
            return 1;
        }
    }

    if (n == 0) {
        fprintf(stderr, "Aucun processus chargé.\n");
        free(procs);
        return 1;
    }

    if (cfg.verbose) {
        printf("Processus chargés : %zu\n", n);
        for (size_t i = 0; i < n; i++) {
            printf("  P%u '%s' arrival=%u bursts=%u\n",
                   procs[i].pid, procs[i].name,
                   procs[i].arrival_time_ms, procs[i].num_bursts);
        }
    }

    /* ---------------------------------------------------------------- */
    /*  3. Création du scheduler                                        */
    /* ---------------------------------------------------------------- */

    Scheduler *sched = scheduler_create(cfg.algo, cfg.quantum_ms);
    if (!sched) {
        free(procs);
        return 1;
    }

    /* ---------------------------------------------------------------- */
    /*  4. Simulation                                                   */
    /* ---------------------------------------------------------------- */

    SimulationState *sim = sim_create(procs, n, sched);
    if (!sim) {
        fprintf(stderr, "Erreur d'initialisation de la simulation.\n");
        scheduler_destroy(sched);
        free(procs);
        return 1;
    }

    printf("Démarrage de la simulation [%s", sched->name);
    if (sched->quantum_ms > 0) printf(", quantum=%u ms", sched->quantum_ms);
    printf("] avec %zu processus...\n", n);

    /* Passer les options d'extension à la simulation */
    sim->sequential_io = cfg.sequential_io;

    if (sim_run(sim) != 0) {
        fprintf(stderr, "Erreur pendant la simulation.\n");
        sim_destroy(sim);
        scheduler_destroy(sched);
        free(procs);
        return 1;
    }

    printf("Simulation terminée en %u ms.\n", sim->clock_ms);

    /* ---------------------------------------------------------------- */
    /*  5. Rendu des résultats                                          */
    /* ---------------------------------------------------------------- */

    OutputConfig out_cfg = {
        .show_gantt   = cfg.show_gantt,
        .show_table   = 1,
        .csv_path     = cfg.csv_output,  /* NULL = nom automatique */
        .show_gtk     = cfg.show_gtk,
        .auto_plot    = cfg.auto_plot
    };

    output_render(sim, sim->report, &out_cfg);

    /* ---------------------------------------------------------------- */
    /*  6. Nettoyage                                                    */
    /* ---------------------------------------------------------------- */

    sim_destroy(sim);
    scheduler_destroy(sched);
    free(procs);

    return 0;
}
