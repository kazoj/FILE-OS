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

/** @brief Nombre maximum de specs --process acceptées en CLI. */
#define MAX_PROCESS_SPECS 64

/**
 * @brief Configuration extraite de la ligne de commande.
 */
typedef struct {
    /* Sous-commande détectée ("run", "list", "help", "interactive", NULL=legacy) */
    const char *subcommand;     /**< Sous-commande active, ou NULL en mode legacy */
    const char *subcommand_arg; /**< Argument optionnel de la sous-commande (ex: "run" après "help") */

    const char *algo;          /**< Nom de l'algorithme ("fifo", "sjf", etc.) */
    uint32_t    quantum_ms;    /**< Quantum pour RR (défaut : 50) */
    const char *input_file;    /**< Chemin vers le fichier .sim, ou NULL */
    const char *csv_output;    /**< Chemin de sortie CSV, ou NULL (auto) */
    int         show_gantt;    /**< 1 = afficher le Gantt ASCII, 0 = non */
    int         verbose;       /**< 1 = mode verbeux */
    int         sequential_io; /**< 1 = E/S non parallélisables (-S) */
    int         show_gtk;      /**< 1 = IHM GTK4 native (-G) */
    int         auto_plot;     /**< 1 = générer les graphiques PNG (-P) */

    /* Specs de processus passées via --process "P1:cpu=200,io=50,cpu=100" */
    const char *process_specs[MAX_PROCESS_SPECS]; /**< Tableau de specs brutes */
    int         process_count;                    /**< Nombre de specs enregistrées */
} CLIConfig;

/**
 * @brief Parse les arguments de la ligne de commande.
 *
 * Supporte deux modes :
 * - **Mode sous-commandes** : `scheduler run <algo> [options] [fichier.sim]`
 * - **Mode legacy** : `scheduler -a <algo> [options]` (rétrocompatibilité)
 *
 * En cas d'erreur ou de --help, affiche l'aide et retourne -1.
 * Retourne -2 si l'option `-l` (liste des algos) est demandée.
 *
 * @param argc   Nombre d'arguments.
 * @param argv   Tableau d'arguments.
 * @param cfg    Structure de configuration à remplir.
 * @return       0 en cas de succès, -1 en cas d'erreur / aide, -2 pour -l.
 */
int cli_parse(int argc, char **argv, CLIConfig *cfg);

/**
 * @brief Affiche l'aide générale sur stdout.
 * @param prog  Nom du programme (argv[0]).
 */
void cli_usage(const char *prog);

/**
 * @brief Affiche l'aide d'une sous-commande spécifique.
 *
 * @param prog  Nom du programme (argv[0]).
 * @param sub   Nom de la sous-commande ("run", "list", etc.), ou NULL pour l'aide générale.
 */
void cli_usage_subcommand(const char *prog, const char *sub);

/**
 * @brief Lance le mode interactif guidé (wizard).
 *
 * Remplit `cfg` en posant des questions à l'utilisateur sur stdin.
 * Après retour, cfg->algo et les options de sortie sont définis.
 *
 * @param cfg  Structure de configuration à remplir.
 * @return     0 si succès, -1 si erreur (EOF inattendu).
 */
int cli_interactive(CLIConfig *cfg);

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

/**
 * @brief Crée un jeu de processus depuis les specs --process.
 *
 * Format de chaque spec : `[Nom:]cpu=<ms>[,io=<ms>,cpu=<ms>...]`
 * Exemple : `"P1:cpu=200,io=50,cpu=100"` ou `"cpu=300"`
 *
 * @param specs   Tableau de chaînes de spec brutes.
 * @param count   Nombre de specs.
 * @param procs   Sortie : tableau de Process alloué.
 * @param n       Sortie : nombre de processus.
 * @return        0 en cas de succès, -1 en cas d'erreur.
 */
int input_from_process_args(const char **specs, int count,
                            Process **procs, size_t *n);

#endif /* INPUT_H */
