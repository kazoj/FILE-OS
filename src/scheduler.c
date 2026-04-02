/**
 * @file scheduler.c
 * @brief Registre des algorithmes et factory de Scheduler.
 *
 * ## Ajouter un nouvel algorithme
 *
 * Ce fichier est le **seul** à modifier pour enregistrer un nouvel algorithme :
 *
 *  1. Inclure son header (ou ajouter sa déclaration dans algorithms.h).
 *  2. Ajouter une entrée `{ "nom", ma_fonction_init }` dans `scheduler_registry[]`.
 *
 * Aucun autre fichier ne doit être touché.
 */

#include "scheduler.h"
#include "algorithms.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* ------------------------------------------------------------------ */
/*  Registre des algorithmes disponibles                               */
/* ------------------------------------------------------------------ */

/**
 * @brief Table de registre : nom → fonction d'initialisation.
 *
 * La recherche est insensible à la casse. La table se termine par
 * une entrée sentinelle { NULL, NULL }.
 */
static const SchedulerRegistryEntry scheduler_registry[] = {
    { "fifo", fifo_init        },
    { "sjf",  sjf_init         },
    { "srjf", srjf_init        },
    { "rr",   round_robin_init },
    { NULL,   NULL             }  /* sentinelle */
};

/* ------------------------------------------------------------------ */
/*  API publique                                                       */
/* ------------------------------------------------------------------ */

/**
 * @brief Recherche insensible à la casse d'un nom dans le registre.
 * @param a  Première chaîne.
 * @param b  Deuxième chaîne.
 * @return   1 si égales (insensible à la casse), 0 sinon.
 */
static int strcaseeq(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
        a++; b++;
    }
    return (*a == '\0' && *b == '\0');
}

/**
 * @brief Crée et initialise un Scheduler par son nom.
 *
 * @param name        Nom de l'algorithme (insensible à la casse).
 * @param quantum_ms  Quantum pour RR ; ignoré pour les autres.
 * @return            Scheduler alloué, ou NULL si non trouvé.
 */
Scheduler *scheduler_create(const char *name, uint32_t quantum_ms)
{
    if (!name) return NULL;

    for (int i = 0; scheduler_registry[i].name != NULL; i++) {
        if (strcaseeq(name, scheduler_registry[i].name)) {
            Scheduler *s = calloc(1, sizeof(Scheduler));
            if (!s) { perror("scheduler_create"); return NULL; }
            scheduler_registry[i].init_fn(s, quantum_ms);
            return s;
        }
    }

    fprintf(stderr, "Algorithme inconnu : '%s'\n", name);
    fprintf(stderr, "Algorithmes disponibles : ");
    scheduler_list_available();
    return NULL;
}

/**
 * @brief Libère un Scheduler et son état interne.
 *
 * Appelle d'abord s->destroy() pour libérer private_data,
 * puis libère la structure elle-même.
 *
 * @param s  Scheduler à libérer (peut être NULL).
 */
void scheduler_destroy(Scheduler *s)
{
    if (!s) return;
    if (s->destroy) s->destroy(s);
    free(s);
}

/**
 * @brief Affiche la liste des algorithmes disponibles sur stdout.
 */
void scheduler_list_available(void)
{
    for (int i = 0; scheduler_registry[i].name != NULL; i++) {
        if (i > 0) printf(", ");
        printf("%s", scheduler_registry[i].name);
    }
    printf("\n");
}
