/**
 * @file input.h
 * @brief Parsing des entrées : ligne de commande et fichier .sim.
 *
 * ## Format du fichier .sim
 *
 * ```
 * # Commentaires (ligne commençant par '#')
 * quantum 50
 *
 * process 1 "NomDuProcessus" arrival=0 priority=2
 *     burst cpu=200 io=50
 *     burst cpu=100
 * end
 * ```
 *
 * Champs optionnels : `quantum` (défaut 50), `arrival` (défaut 0),
 * `priority` (défaut 0), `io=` sur un burst (défaut 0).
 * Le dernier burst d'un processus ne doit pas avoir de champ `io=`.
 */

#ifndef INPUT_H
#define INPUT_H

#include "process.h"
#include <stddef.h>

/**
 * @brief Configuration extraite de la ligne de commande.
 */
typedef struct {
    const char *algo;          /**< Nom de l'algorithme ("fifo", "sjf", etc.) */
    uint32_t    quantum_ms;    /**< Quantum pour RR (défaut : 50) */
    const char *input_file;    /**< Chemin vers le fichier .sim, ou NULL */
    const char *csv_output;    /**< Chemin de sortie CSV, ou NULL (auto) */
    int         show_gantt;    /**< 1 = afficher le Gantt ASCII, 0 = non */
    int         verbose;       /**< 1 = mode verbeux */
    int         sequential_io; /**< 1 = E/S non parallélisables (-S) */
    int         show_ncurses;  /**< 1 = IHM ncurses interactive (-i) */
    int         auto_plot;     /**< 1 = générer les graphiques PNG (-P) */
} CLIConfig;

/**
 * @brief Parse les arguments de la ligne de commande.
 *
 * En cas d'erreur ou de --help, affiche l'aide et retourne -1.
 *
 * @param argc   Nombre d'arguments.
 * @param argv   Tableau d'arguments.
 * @param cfg    Structure de configuration à remplir.
 * @return       0 en cas de succès, -1 en cas d'erreur / aide.
 */
int cli_parse(int argc, char **argv, CLIConfig *cfg);

/**
 * @brief Affiche l'aide d'utilisation sur stdout.
 * @param prog  Nom du programme (argv[0]).
 */
void cli_usage(const char *prog);

/**
 * @brief Charge un jeu de processus depuis un fichier .sim.
 *
 * Alloue dynamiquement le tableau `*procs` et `*quantum_ms`
 * (si défini dans le fichier). L'appelant est responsable de
 * libérer `*procs` avec free().
 *
 * @param path       Chemin vers le fichier .sim.
 * @param procs      Sortie : tableau de Process alloué.
 * @param n          Sortie : nombre de processus.
 * @param quantum_ms Sortie : quantum lu dans le fichier (0 si absent).
 * @return           0 en cas de succès, -1 en cas d'erreur.
 */
int input_load_file(const char *path,
                    Process   **procs,
                    size_t     *n,
                    uint32_t   *quantum_ms);

/**
 * @brief Crée un jeu de processus simple depuis les arguments CLI.
 *
 * Utilisé pour les cas simples sans fichier d'entrée. Format attendu :
 * chaque argument est un burst CPU en ms, séparé par des virgules pour
 * les E/S, ex: "200,50,100" = burst cpu=200 io=50 burst cpu=100.
 *
 * @param argv    Arguments restants après le parsing de cfg.
 * @param argc    Nombre d'arguments restants.
 * @param procs   Sortie : tableau de Process alloué.
 * @param n       Sortie : nombre de processus.
 * @return        0 en cas de succès, -1 en cas d'erreur.
 */
int input_from_cli(char **argv, int argc, Process **procs, size_t *n);

#endif /* INPUT_H */
