/**
 * @file output_ncurses.h
 * @brief IHM ncurses : diagramme de Gantt coloré + tableau de métriques.
 *
 * Ce fichier n'est inclus que si HAVE_NCURSES est défini (détecté par le Makefile).
 *
 * Layout à l'écran :
 * ```
 * ┌──────────────────────────── GANTT ─────────────────────────────┐
 * │ PID  | 0    50   100  150  200  ...                            │
 * │   1  [CPU ][I/O ][I/O ][CPU ]                                  │
 * │   2  [    ][CPU ][CPU ][I/O ]                                  │
 * │ IDLE [    ][    ]                                              │
 * ├──────────────────────────── MÉTRIQUES ─────────────────────────┤
 * │ PID  Nom       Arrivée  Fin  Restitution  Attente  Réponse     │
 * │  1   WebServer      0   350          350       50       50     │
 * │ ...                                                            │
 * │ Moy                            300.00   100.00    75.00        │
 * │ CPU util : 95.2%    Durée : 860 ms                             │
 * └────────────────────────────────────────────────────────────────┘
 *   q = quitter   ↑/↓ = défiler
 * ```
 *
 * Palette de couleurs :
 *  - COLOR_PAIR(1) = vert   → CPU actif
 *  - COLOR_PAIR(2) = jaune  → E/S (BLOCKED)
 *  - COLOR_PAIR(3) = rouge  → IDLE
 *  - COLOR_PAIR(4) = cyan   → en-têtes
 *  - COLOR_PAIR(5) = blanc  → données
 */

#ifndef OUTPUT_NCURSES_H
#define OUTPUT_NCURSES_H

#ifdef HAVE_NCURSES

#include "simulation.h"
#include "metrics.h"

/**
 * @brief Lance l'IHM ncurses interactive.
 *
 * Prend le contrôle du terminal jusqu'à ce que l'utilisateur appuie sur 'q'.
 * Restaure le terminal à la sortie.
 *
 * @param sim     Simulation terminée (contient gantt[]).
 * @param report  Rapport de métriques calculé.
 */
void output_ncurses_render(const SimulationState *sim,
                           const MetricsReport   *report);

#endif /* HAVE_NCURSES */
#endif /* OUTPUT_NCURSES_H */
