/**
 * @file output_gtk.h
 * @brief IHM GTK4 : fenêtre native avec Gantt Cairo et tableau de métriques.
 *
 * Compilé uniquement si la bibliothèque GTK4 est détectée (HAVE_GTK4).
 * Activé par l'option `-G` en ligne de commande.
 */

#ifndef OUTPUT_GTK_H
#define OUTPUT_GTK_H

#include "simulation.h"
#include "metrics.h"

/**
 * @brief Lance la fenêtre GTK4 de visualisation des résultats.
 *
 * Crée une application GTK4 avec :
 *  - Un panneau latéral gauche (informations algo + CPU)
 *  - Un diagramme de Gantt (GtkDrawingArea + Cairo) en haut à droite
 *  - Un tableau de métriques (GtkColumnView) en bas à droite
 *
 * La fonction bloque jusqu'à la fermeture de la fenêtre.
 *
 * @param sim     Simulation terminée (contient gantt[]).
 * @param report  Rapport de métriques calculé.
 */
void output_gtk_render(const SimulationState *sim,
                       const MetricsReport   *report);

#endif /* OUTPUT_GTK_H */
