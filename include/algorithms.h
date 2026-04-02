/**
 * @file algorithms.h
 * @brief Déclarations des fonctions d'initialisation de chaque algorithme.
 *
 * Chaque fonction remplit une instance de Scheduler avec le nom, les
 * drapeaux et la vtable propres à l'algorithme. Elle est référencée
 * dans le registre `scheduler_registry[]` de `scheduler.c`.
 *
 * Pour ajouter un algorithme :
 *  1. Déclarer sa fonction d'init ici.
 *  2. L'implémenter dans `src/algorithms/mon_algo.c`.
 *  3. Ajouter une entrée dans `scheduler_registry[]`.
 */

#ifndef ALGORITHMS_H
#define ALGORITHMS_H

#include "scheduler.h"

/**
 * @brief Initialise un ordonnanceur FIFO (First In First Out).
 *
 * Non préemptif. Utilise une Queue FIFO simple. Le premier processus
 * arrivé est le premier servi. Le quantum est ignoré.
 *
 * @param s          Instance de Scheduler à remplir.
 * @param quantum_ms Ignoré.
 */
void fifo_init(Scheduler *s, uint32_t quantum_ms);

/**
 * @brief Initialise un ordonnanceur SJF (Shortest Job First).
 *
 * Non préemptif. Utilise une PriorityQueue triée par burst CPU total
 * restant. En cas d'égalité, tie-break par PID. Le quantum est ignoré.
 *
 * @param s          Instance de Scheduler à remplir.
 * @param quantum_ms Ignoré.
 */
void sjf_init(Scheduler *s, uint32_t quantum_ms);

/**
 * @brief Initialise un ordonnanceur SRJF (Shortest Remaining Job First).
 *
 * Préemptif. Variante préemptive de SJF : à chaque arrivée d'un
 * processus ou à chaque tick, si un processus prêt a moins de temps
 * CPU restant que le processus courant, il y a préemption.
 *
 * @param s          Instance de Scheduler à remplir.
 * @param quantum_ms Ignoré.
 */
void srjf_init(Scheduler *s, uint32_t quantum_ms);

/**
 * @brief Initialise un ordonnanceur Round Robin.
 *
 * Préemptif. Chaque processus reçoit un quantum de CPU. S'il ne
 * termine pas dans ce délai, il est replacé en queue de la file FIFO.
 *
 * @param s          Instance de Scheduler à remplir.
 * @param quantum_ms Durée du quantum en ms (défaut : 50 ms si 0).
 */
void round_robin_init(Scheduler *s, uint32_t quantum_ms);

#endif /* ALGORITHMS_H */
