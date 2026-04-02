/**
 * @file process.h
 * @brief Définition du Process, du PCB et des états du simulateur.
 *
 * Un `Process` est le descripteur statique (immuable) d'un processus,
 * tel que défini dans le fichier d'entrée. Un `PCB` (Process Control Block)
 * contient l'état mutable du processus pendant la simulation.
 */

#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

/** Nombre maximum de paires (CPU burst, I/O burst) par processus. */
#define MAX_BURSTS 32

/**
 * @brief États possibles d'un processus dans le simulateur.
 */
typedef enum {
    STATE_NEW        = 0, /**< Processus créé, pas encore admis dans la file */
    STATE_READY      = 1, /**< En file d'attente, prêt à utiliser le CPU */
    STATE_RUNNING    = 2, /**< En cours d'exécution sur le CPU */
    STATE_BLOCKED    = 3, /**< En attente d'une opération d'E/S */
    STATE_TERMINATED = 4  /**< Exécution terminée */
} ProcessState;

/**
 * @brief Une entrée de burst : une phase CPU suivie d'une phase E/S optionnelle.
 *
 * Les bursts s'enchaînent : CPU, E/S, CPU, E/S, ..., CPU.
 * Le dernier burst ne comporte jamais d'E/S (io_duration_ms == 0).
 */
typedef struct {
    uint32_t cpu_duration_ms; /**< Durée du burst CPU en millisecondes */
    uint32_t io_duration_ms;  /**< Durée de l'E/S qui suit (0 = pas d'E/S) */
} BurstEntry;

/**
 * @brief Descripteur statique d'un processus, chargé depuis l'entrée.
 *
 * Cette structure est immuable une fois initialisée. Toute l'évolution
 * de l'état pendant la simulation se trouve dans le PCB correspondant.
 */
typedef struct {
    uint32_t   pid;                      /**< Identifiant unique du processus */
    char       name[64];                 /**< Nom lisible du processus */
    uint32_t   arrival_time_ms;          /**< Date d'arrivée dans le système (ms) */
    uint32_t   priority;                 /**< Priorité statique (utilisée par certains algos) */
    uint32_t   num_bursts;               /**< Nombre d'entrées dans burst_table */
    BurstEntry burst_table[MAX_BURSTS];  /**< Table des bursts CPU/E/S */
} Process;

/**
 * @brief Process Control Block — état mutable d'un processus en simulation.
 *
 * Un PCB est créé pour chaque Process au début de la simulation et
 * détruit à la fin. Les champs `next` permettent d'intégrer le PCB
 * dans des listes chaînées intrusivement (files d'attente).
 */
typedef struct PCB {
    Process        *proc;              /**< Pointeur vers le descripteur statique */
    ProcessState    state;             /**< État courant du processus */
    uint32_t        current_burst_idx; /**< Index dans proc->burst_table */
    uint32_t        remaining_cpu_ms;  /**< Temps CPU restant pour le burst courant */
    uint32_t        remaining_io_ms;   /**< Temps E/S restant pour le burst courant */

    /* Timestamps enregistrés pendant la simulation */
    uint32_t        arrival_time_ms;     /**< Copie de proc->arrival_time_ms */
    uint32_t        first_run_time_ms;   /**< Premier accès au CPU (pour le temps de réponse) */
    uint32_t        finish_time_ms;      /**< Heure de fin (état TERMINATED) */
    uint32_t        total_wait_ms;       /**< Temps total passé à l'état READY */
    uint32_t        total_cpu_ms;        /**< Temps CPU total consommé */
    uint32_t        total_io_ms;         /**< Temps E/S total effectué */
    uint32_t        last_ready_time_ms;  /**< Horodatage du dernier passage à READY */

    int             has_run;             /**< 1 si le processus a déjà eu le CPU, 0 sinon */

    struct PCB     *next;               /**< Pointeur pour liste chaînée intrusive */
} PCB;

/* ------------------------------------------------------------------ */
/*  API process.c                                                       */
/* ------------------------------------------------------------------ */

/**
 * @brief Alloue et initialise un PCB à partir d'un Process.
 * @param proc  Pointeur vers le descripteur statique du processus.
 * @return      PCB alloué sur le tas, ou NULL en cas d'échec.
 */
PCB *pcb_create(Process *proc);

/**
 * @brief Libère un PCB. Ne libère pas le Process sous-jacent.
 * @param pcb   PCB à libérer.
 */
void pcb_destroy(PCB *pcb);

/**
 * @brief Effectue une transition d'état sur un PCB en enregistrant les timestamps.
 * @param pcb       PCB cible.
 * @param new_state Nouvel état.
 * @param now_ms    Temps courant de simulation en ms.
 */
void pcb_transition(PCB *pcb, ProcessState new_state, uint32_t now_ms);

/**
 * @brief Retourne une chaîne lisible pour un état de processus.
 * @param state État du processus.
 * @return      Chaîne statique (ex: "READY", "RUNNING").
 */
const char *process_state_str(ProcessState state);

#endif /* PROCESS_H */
