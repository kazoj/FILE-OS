/**
 * @file output.h
 * @brief Rendu des résultats : table terminale, Gantt ASCII, CSV.
 *
 * Toutes les fonctions de rendu prennent un `MetricsReport` comme
 * source de vérité. Le Gantt utilise `GanttEntry[]` de `SimulationState`.
 */

#ifndef OUTPUT_H
#define OUTPUT_H

#include "metrics.h"
#include "simulation.h"
#include <stdio.h>

/**
 * @brief Configuration du rendu de sortie.
 */
typedef struct {
    int         show_gantt;    /**< 1 = afficher le Gantt ASCII dans le terminal */
    int         show_table;    /**< 1 = afficher la table des métriques */
    const char *csv_path;      /**< Chemin du fichier CSV, ou NULL pour auto */
    int         show_ncurses;  /**< 1 = IHM ncurses interactive (-i) */
    int         auto_plot;     /**< 1 = générer les graphiques PNG via Python (-P) */
} OutputConfig;

/**
 * @brief Effectue tous les rendus demandés dans la configuration.
 *
 * @param sim     Simulation terminée (contient gantt[]).
 * @param report  Rapport de métriques calculé.
 * @param cfg     Configuration de sortie.
 * @return        0 en cas de succès, -1 si erreur d'écriture.
 */
int output_render(const SimulationState *sim,
                  const MetricsReport   *report,
                  const OutputConfig    *cfg);

/**
 * @brief Affiche la table des métriques sur un flux de sortie.
 *
 * Format aligné en colonnes, avec ligne de moyennes en bas.
 * Compatible copier-coller tableur si redirigé.
 *
 * @param report  Rapport de métriques.
 * @param stream  Flux de sortie (stdout ou fichier).
 */
void output_print_table(const MetricsReport *report, FILE *stream);

/**
 * @brief Affiche le diagramme de Gantt ASCII compact sur un flux.
 *
 * Une ligne par processus, colonnes proportionnelles au quantum.
 * PID 0 = IDLE.
 *
 * @param gantt   Tableau d'entrées Gantt.
 * @param count   Nombre d'entrées.
 * @param report  Rapport (pour le nom de l'algo et le quantum).
 * @param stream  Flux de sortie.
 */
void output_print_gantt(const GanttEntry    *gantt,
                        size_t               count,
                        const MetricsReport *report,
                        FILE                *stream);

/**
 * @brief Écrit le rapport de métriques dans un fichier CSV.
 *
 * Si path est NULL, génère un nom automatique : "<algo>_<timestamp>.csv".
 * Le fichier est écrasé s'il existe.
 *
 * @param report  Rapport de métriques.
 * @param path    Chemin cible, ou NULL pour nom automatique.
 * @return        0 en cas de succès, -1 en cas d'erreur.
 */
int output_write_csv(const MetricsReport *report, const char *path);

/**
 * @brief Lance le script Python de génération de graphiques sur le CSV produit.
 *
 * Appelle `python3 tools/plot_results.py <csv_path>`. Non-fatal si Python
 * ou matplotlib est absent.
 *
 * @param csv_path  Chemin du fichier CSV à traiter.
 */
void output_plot(const char *csv_path);

/**
 * @brief Lance l'IHM ncurses interactive (si compilé avec HAVE_NCURSES).
 *
 * Affiche un diagramme de Gantt coloré et le tableau de métriques.
 * Navigation : q = quitter, flèches = défiler si dépassement.
 *
 * @param sim     Simulation terminée.
 * @param report  Rapport de métriques.
 */
void output_ncurses_render(const SimulationState *sim,
                           const MetricsReport   *report);

#endif /* OUTPUT_H */
