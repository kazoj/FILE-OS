/**
 * @file queue.h
 * @brief File FIFO générique et file à priorité pour la gestion des PCBs.
 *
 * Les deux structures stockent des pointeurs vers des PCBs.
 * La file à priorité utilise un comparateur injecté à la création,
 * ce qui permet de l'utiliser pour SJF (tri par remaining_cpu_ms),
 * par date d'arrivée, ou par priorité statique.
 */

#ifndef QUEUE_H
#define QUEUE_H

#include "process.h"
#include <stddef.h>

/* ------------------------------------------------------------------ */
/*  Types                                                               */
/* ------------------------------------------------------------------ */

/**
 * @brief Type d'une fonction de comparaison de PCBs.
 *
 * @return Valeur négative si a doit passer avant b,
 *         valeur positive si b doit passer avant a,
 *         0 si égalité.
 */
typedef int (*PCBComparator)(const PCB *a, const PCB *b);

/**
 * @brief Nœud interne partagé par la Queue et la PriorityQueue.
 */
typedef struct QueueNode {
    PCB              *pcb;  /**< Processus stocké dans ce nœud */
    struct QueueNode *next; /**< Nœud suivant dans la liste */
} QueueNode;

/**
 * @brief File FIFO (premier entré, premier sorti).
 */
typedef struct {
    QueueNode *head;  /**< Tête de la file (prochain à sortir) */
    QueueNode *tail;  /**< Queue de la file (dernier entré) */
    size_t     count; /**< Nombre d'éléments dans la file */
} Queue;

/**
 * @brief File à priorité triée par un comparateur injecté.
 *
 * L'insertion maintient l'ordre : O(n) en insertion, O(1) en extraction.
 * Convient pour SJF, SRJF (reorder), ou une file par priorité statique.
 */
typedef struct {
    QueueNode    *head;    /**< Tête (le meilleur candidat est toujours en tête) */
    size_t        count;   /**< Nombre d'éléments */
    PCBComparator compare; /**< Fonction de tri injectée à la création */
} PriorityQueue;

/* ------------------------------------------------------------------ */
/*  API Queue FIFO (queue.c)                                           */
/* ------------------------------------------------------------------ */

/** @brief Initialise une Queue vide. */
void  queue_init(Queue *q);

/** @brief Enfile un PCB en queue. */
void  queue_enqueue(Queue *q, PCB *pcb);

/** @brief Défile le PCB en tête. Retourne NULL si vide. */
PCB  *queue_dequeue(Queue *q);

/** @brief Consulte le PCB en tête sans le retirer. Retourne NULL si vide. */
PCB  *queue_peek(const Queue *q);

/** @brief Retourne 1 si la file est vide, 0 sinon. */
int   queue_is_empty(const Queue *q);

/** @brief Libère tous les nœuds internes (pas les PCBs eux-mêmes). */
void  queue_destroy(Queue *q);

/* ------------------------------------------------------------------ */
/*  API PriorityQueue (queue.c)                                        */
/* ------------------------------------------------------------------ */

/**
 * @brief Initialise une file à priorité avec le comparateur donné.
 * @param pq   File à initialiser.
 * @param cmp  Comparateur : négatif si a passe avant b.
 */
void  pqueue_init(PriorityQueue *pq, PCBComparator cmp);

/** @brief Insère un PCB en maintenant l'ordre défini par le comparateur. */
void  pqueue_insert(PriorityQueue *pq, PCB *pcb);

/** @brief Retire et retourne le PCB en tête (meilleur candidat). */
PCB  *pqueue_pop(PriorityQueue *pq);

/** @brief Consulte le PCB en tête sans le retirer. Retourne NULL si vide. */
PCB  *pqueue_peek(const PriorityQueue *pq);

/** @brief Retourne 1 si la file est vide, 0 sinon. */
int   pqueue_is_empty(const PriorityQueue *pq);

/**
 * @brief Retrie la file après modification d'une clé (ex: remaining_cpu_ms
 *        d'un processus change à chaque tick pour SRJF).
 */
void  pqueue_reorder(PriorityQueue *pq);

/** @brief Libère tous les nœuds internes (pas les PCBs eux-mêmes). */
void  pqueue_destroy(PriorityQueue *pq);

/* ------------------------------------------------------------------ */
/*  Comparateurs prédéfinis (queue.c)                                  */
/* ------------------------------------------------------------------ */

/** @brief Tri par date d'arrivée croissante. */
int cmp_arrival_time(const PCB *a, const PCB *b);

/** @brief Tri par temps CPU restant croissant (pour SJF / SRJF). */
int cmp_remaining_cpu(const PCB *a, const PCB *b);

/** @brief Tri par priorité statique décroissante (valeur haute = priorité haute). */
int cmp_priority(const PCB *a, const PCB *b);

#endif /* QUEUE_H */
