/**
 * @file queue.c
 * @brief Implémentation de la file FIFO et de la file à priorité.
 */

#include "queue.h"
#include <stdlib.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/*  Utilitaire interne : alloue un nœud                                */
/* ------------------------------------------------------------------ */

static QueueNode *node_alloc(PCB *pcb)
{
    QueueNode *n = malloc(sizeof(QueueNode));
    if (!n) {
        perror("queue: node_alloc");
        return NULL;
    }
    n->pcb  = pcb;
    n->next = NULL;
    return n;
}

/* ================================================================== */
/*  Queue FIFO                                                          */
/* ================================================================== */

/**
 * @brief Initialise une Queue FIFO vide.
 * @param q  Pointeur vers la Queue à initialiser.
 */
void queue_init(Queue *q)
{
    if (!q) return;
    q->head  = NULL;
    q->tail  = NULL;
    q->count = 0;
}

/**
 * @brief Enfile un PCB en queue (FIFO).
 * @param q    File cible.
 * @param pcb  PCB à ajouter.
 */
void queue_enqueue(Queue *q, PCB *pcb)
{
    if (!q || !pcb) return;
    QueueNode *n = node_alloc(pcb);
    if (!n) return;

    if (q->tail) {
        q->tail->next = n;
    } else {
        q->head = n;
    }
    q->tail = n;
    q->count++;
}

/**
 * @brief Défile le PCB en tête de la FIFO.
 * @param q  File cible.
 * @return   PCB défilé, ou NULL si la file est vide.
 */
PCB *queue_dequeue(Queue *q)
{
    if (!q || !q->head) return NULL;

    QueueNode *n   = q->head;
    PCB       *pcb = n->pcb;
    q->head        = n->next;
    if (!q->head) q->tail = NULL;
    free(n);
    q->count--;
    return pcb;
}

/**
 * @brief Consulte le PCB en tête sans le retirer.
 * @param q  File cible (const).
 * @return   PCB en tête, ou NULL si vide.
 */
PCB *queue_peek(const Queue *q)
{
    if (!q || !q->head) return NULL;
    return q->head->pcb;
}

/**
 * @brief Teste si la file FIFO est vide.
 * @param q  File cible (const).
 * @return   1 si vide, 0 sinon.
 */
int queue_is_empty(const Queue *q)
{
    return (!q || q->count == 0);
}

/**
 * @brief Libère tous les nœuds internes de la Queue.
 *
 * Les PCBs pointés ne sont pas libérés.
 *
 * @param q  File à vider.
 */
void queue_destroy(Queue *q)
{
    if (!q) return;
    QueueNode *cur = q->head;
    while (cur) {
        QueueNode *tmp = cur->next;
        free(cur);
        cur = tmp;
    }
    q->head  = NULL;
    q->tail  = NULL;
    q->count = 0;
}

/* ================================================================== */
/*  PriorityQueue                                                       */
/* ================================================================== */

/**
 * @brief Initialise une file à priorité avec le comparateur donné.
 * @param pq   File à initialiser.
 * @param cmp  Comparateur : retourne négatif si a passe avant b.
 */
void pqueue_init(PriorityQueue *pq, PCBComparator cmp)
{
    if (!pq) return;
    pq->head    = NULL;
    pq->count   = 0;
    pq->compare = cmp;
}

/**
 * @brief Insère un PCB dans la file en maintenant l'ordre du comparateur.
 *
 * Insertion triée en O(n). Acceptable pour les tailles de file
 * utilisées dans un simulateur pédagogique (< 100 processus).
 *
 * @param pq   File à priorité.
 * @param pcb  PCB à insérer.
 */
void pqueue_insert(PriorityQueue *pq, PCB *pcb)
{
    if (!pq || !pcb) return;
    QueueNode *n = node_alloc(pcb);
    if (!n) return;

    /* Insertion en tête si la file est vide ou si pcb passe avant la tête */
    if (!pq->head || pq->compare(pcb, pq->head->pcb) < 0) {
        n->next  = pq->head;
        pq->head = n;
        pq->count++;
        return;
    }

    /* Recherche de la position d'insertion */
    QueueNode *cur = pq->head;
    while (cur->next && pq->compare(pcb, cur->next->pcb) >= 0) {
        cur = cur->next;
    }
    n->next   = cur->next;
    cur->next = n;
    pq->count++;
}

/**
 * @brief Retire et retourne le PCB en tête (meilleur selon le comparateur).
 * @param pq  File à priorité.
 * @return    PCB retiré, ou NULL si vide.
 */
PCB *pqueue_pop(PriorityQueue *pq)
{
    if (!pq || !pq->head) return NULL;

    QueueNode *n   = pq->head;
    PCB       *pcb = n->pcb;
    pq->head       = n->next;
    free(n);
    pq->count--;
    return pcb;
}

/**
 * @brief Consulte le PCB en tête sans le retirer.
 * @param pq  File à priorité (const).
 * @return    PCB en tête, ou NULL si vide.
 */
PCB *pqueue_peek(const PriorityQueue *pq)
{
    if (!pq || !pq->head) return NULL;
    return pq->head->pcb;
}

/**
 * @brief Teste si la file à priorité est vide.
 * @param pq  File à priorité (const).
 * @return    1 si vide, 0 sinon.
 */
int pqueue_is_empty(const PriorityQueue *pq)
{
    return (!pq || pq->count == 0);
}

/**
 * @brief Retrie la file à priorité après modification d'une clé.
 *
 * Utilisé par SRJF quand remaining_cpu_ms d'un processus en attente
 * est mis à jour (ce qui n'arrive pas dans notre modèle — les clés ne
 * changent qu'à la préemption, où le processus est réinséré). Cette
 * fonction est fournie pour la complétude de l'API.
 *
 * Implémentation : extraction de tous les nœuds, reconstruction triée.
 *
 * @param pq  File à retrier.
 */
void pqueue_reorder(PriorityQueue *pq)
{
    if (!pq || pq->count <= 1) return;

    /* Détacher tous les nœuds dans un tableau temporaire */
    PCB **tmp = malloc(pq->count * sizeof(PCB *));
    if (!tmp) return;

    size_t n = 0;
    QueueNode *cur = pq->head;
    while (cur) {
        tmp[n++] = cur->pcb;
        QueueNode *next = cur->next;
        free(cur);
        cur = next;
    }
    pq->head  = NULL;
    pq->count = 0;

    /* Réinsérer dans l'ordre */
    for (size_t i = 0; i < n; i++) {
        pqueue_insert(pq, tmp[i]);
    }
    free(tmp);
}

/**
 * @brief Libère tous les nœuds internes de la PriorityQueue.
 *
 * Les PCBs pointés ne sont pas libérés.
 *
 * @param pq  File à vider.
 */
void pqueue_destroy(PriorityQueue *pq)
{
    if (!pq) return;
    QueueNode *cur = pq->head;
    while (cur) {
        QueueNode *tmp = cur->next;
        free(cur);
        cur = tmp;
    }
    pq->head  = NULL;
    pq->count = 0;
}

/* ================================================================== */
/*  Comparateurs prédéfinis                                             */
/* ================================================================== */

/**
 * @brief Comparateur par date d'arrivée croissante.
 * @param a  Premier PCB.
 * @param b  Deuxième PCB.
 * @return   Négatif si a arrive avant b.
 */
int cmp_arrival_time(const PCB *a, const PCB *b)
{
    if (a->arrival_time_ms < b->arrival_time_ms) return -1;
    if (a->arrival_time_ms > b->arrival_time_ms) return  1;
    return (int)a->proc->pid - (int)b->proc->pid; /* tie-break par PID */
}

/**
 * @brief Comparateur par temps CPU restant croissant (SJF / SRJF).
 * @param a  Premier PCB.
 * @param b  Deuxième PCB.
 * @return   Négatif si a a moins de temps CPU restant que b.
 */
int cmp_remaining_cpu(const PCB *a, const PCB *b)
{
    if (a->remaining_cpu_ms < b->remaining_cpu_ms) return -1;
    if (a->remaining_cpu_ms > b->remaining_cpu_ms) return  1;
    return (int)a->proc->pid - (int)b->proc->pid; /* tie-break par PID */
}

/**
 * @brief Comparateur par priorité statique décroissante.
 *
 * Une valeur de priorité plus grande signifie une priorité plus haute.
 *
 * @param a  Premier PCB.
 * @param b  Deuxième PCB.
 * @return   Négatif si a a une priorité plus haute que b.
 */
int cmp_priority(const PCB *a, const PCB *b)
{
    if (a->proc->priority > b->proc->priority) return -1;
    if (a->proc->priority < b->proc->priority) return  1;
    return (int)a->proc->pid - (int)b->proc->pid;
}
