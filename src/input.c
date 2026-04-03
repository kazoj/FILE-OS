/**
 * @file input.c
 * @brief Parsing des entrées : ligne de commande et fichier .sim.
 */

#include "input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ------------------------------------------------------------------ */
/*  Utilitaires internes                                               */
/* ------------------------------------------------------------------ */

/** @brief Supprime les espaces en début et fin d'une chaîne (en place). */
static char *trim(char *s)
{
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
    return s;
}

/** @brief Retourne 1 si la ligne est un commentaire ou vide. */
static int is_comment_or_blank(const char *line)
{
    while (isspace((unsigned char)*line)) line++;
    return (*line == '#' || *line == '\0');
}

/* ------------------------------------------------------------------ */
/*  CLI                                                                */
/* ------------------------------------------------------------------ */

/**
 * @brief Affiche l'aide d'utilisation.
 * @param prog  Nom du programme.
 */
void cli_usage(const char *prog)
{
    printf("Usage: %s -a <algo> [options] [fichier.sim]\n\n", prog);
    printf("Algorithmes disponibles : fifo, sjf, srjf, rr\n\n");
    printf("Options :\n");
    printf("  -a <algo>      Algorithme d'ordonnancement (obligatoire)\n");
    printf("  -q <ms>        Quantum pour Round Robin (défaut : 50 ms)\n");
    printf("  -f <fichier>   Fichier d'entrée .sim\n");
    printf("  -o <fichier>   Chemin de sortie CSV (défaut : auto)\n");
    printf("  -g             Afficher le diagramme de Gantt ASCII\n");
    printf("  -G             IHM GTK4 native (fenêtre desktop, si disponible)\n");
    printf("  -P             Générer les graphiques PNG via Python\n");
    printf("  -S             E/S non parallélisables (device unique)\n");
    printf("  -v             Mode verbeux\n");
    printf("  -l             Lister les algorithmes disponibles\n");
    printf("  -h             Afficher cette aide\n\n");
    printf("Exemple :\n");
    printf("  %s -a rr -q 50 -g -f tests/sample_inputs/basic.sim\n", prog);
}

/**
 * @brief Parse les arguments de la ligne de commande.
 *
 * @param argc  Nombre d'arguments.
 * @param argv  Tableau d'arguments.
 * @param cfg   Structure de configuration à remplir.
 * @return      0 en cas de succès, -1 en cas d'erreur.
 */
int cli_parse(int argc, char **argv, CLIConfig *cfg)
{
    if (!cfg) return -1;
    memset(cfg, 0, sizeof(CLIConfig));
    cfg->quantum_ms  = 50; /* défaut */
    cfg->show_gantt  = 0;
    cfg->verbose     = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            cli_usage(argv[0]);
            return -1;
        } else if (strcmp(argv[i], "-l") == 0) {
            /* Liste des algos : géré dans main.c */
            return -2;
        } else if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) {
            cfg->algo = argv[++i];
        } else if (strcmp(argv[i], "-q") == 0 && i + 1 < argc) {
            cfg->quantum_ms = (uint32_t)atoi(argv[++i]);
            if (cfg->quantum_ms == 0) cfg->quantum_ms = 50;
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            cfg->input_file = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            cfg->csv_output = argv[++i];
        } else if (strcmp(argv[i], "-g") == 0) {
            cfg->show_gantt = 1;
        } else if (strcmp(argv[i], "-G") == 0) {
            cfg->show_gtk = 1;
        } else if (strcmp(argv[i], "-P") == 0) {
            cfg->auto_plot = 1;
        } else if (strcmp(argv[i], "-S") == 0) {
            cfg->sequential_io = 1;
        } else if (strcmp(argv[i], "-v") == 0) {
            cfg->verbose = 1;
        } else if (argv[i][0] != '-') {
            /* Argument positionnel : fichier d'entrée */
            cfg->input_file = argv[i];
        } else {
            fprintf(stderr, "Option inconnue : %s\n", argv[i]);
            cli_usage(argv[0]);
            return -1;
        }
    }

    if (!cfg->algo) {
        fprintf(stderr, "Erreur : algorithme non spécifié. Utilisez -a <algo>.\n");
        cli_usage(argv[0]);
        return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Lecture du fichier .sim                                            */
/* ------------------------------------------------------------------ */

#define MAX_PROCESSES 256
#define LINE_BUF 512

/**
 * @brief Charge un jeu de processus depuis un fichier .sim.
 *
 * Le parser est tolérant aux espaces. Il gère les commentaires (#),
 * les blocs `process ... end`, et les lignes `burst cpu= io=`.
 *
 * @param path       Chemin vers le fichier.
 * @param procs      Sortie : tableau alloué de Process.
 * @param n          Sortie : nombre de processus lus.
 * @param quantum_ms Sortie : quantum lu dans le fichier (0 si absent).
 * @return           0 si succès, -1 si erreur.
 */
int input_load_file(const char *path,
                    Process   **procs,
                    size_t     *n,
                    uint32_t   *quantum_ms)
{
    *procs      = NULL;
    *n          = 0;
    *quantum_ms = 0;

    FILE *f = fopen(path, "r");
    if (!f) {
        perror(path);
        return -1;
    }

    Process *buf = calloc(MAX_PROCESSES, sizeof(Process));
    if (!buf) { fclose(f); return -1; }

    char line[LINE_BUF];
    int  lineno    = 0;
    int  in_proc   = 0;
    int  proc_idx  = -1;
    int  ok        = 1;

    while (fgets(line, sizeof(line), f)) {
        lineno++;
        char *l = trim(line);
        if (is_comment_or_blank(l)) continue;

        /* Directive globale : quantum */
        if (strncmp(l, "quantum", 7) == 0 && isspace((unsigned char)l[7])) {
            *quantum_ms = (uint32_t)atoi(l + 8);
            continue;
        }

        /* Début d'un bloc process */
        if (strncmp(l, "process", 7) == 0 && isspace((unsigned char)l[7])) {
            if (in_proc) {
                fprintf(stderr, "%s:%d: 'process' imbriqué non supporté\n",
                        path, lineno);
                ok = 0; break;
            }
            if ((size_t)(proc_idx + 1) >= MAX_PROCESSES) {
                fprintf(stderr, "%s:%d: trop de processus (max %d)\n",
                        path, lineno, MAX_PROCESSES);
                ok = 0; break;
            }
            proc_idx++;
            in_proc = 1;
            Process *p = &buf[proc_idx];
            memset(p, 0, sizeof(Process));

            /* Parser : process <pid> "<name>" [arrival=N] [priority=N] */
            char  name_buf[64] = {0};
            uint32_t pid = 0, arr = 0, prio = 0;

            /* Lire le PID */
            char *tok = strtok(l + 8, " \t");
            if (tok) pid = (uint32_t)atoi(tok);
            p->pid = pid;

            /* Lire le nom entre guillemets (optionnel) */
            char *q = strchr(l + 8, '"');
            if (q) {
                char *qend = strchr(q + 1, '"');
                if (qend) {
                    size_t len = (size_t)(qend - q - 1);
                    if (len >= sizeof(name_buf)) len = sizeof(name_buf) - 1;
                    strncpy(name_buf, q + 1, len);
                    name_buf[len] = '\0';
                }
            }
            if (name_buf[0] == '\0') snprintf(name_buf, sizeof(name_buf), "P%u", pid);
            strncpy(p->name, name_buf, sizeof(p->name) - 1);

            /* Lire les attributs key=value */
            char *attr = strstr(l + 8, "arrival=");
            if (attr) arr = (uint32_t)atoi(attr + 8);
            attr = strstr(l + 8, "priority=");
            if (attr) prio = (uint32_t)atoi(attr + 9);

            p->arrival_time_ms = arr;
            p->priority        = prio;
            p->num_bursts      = 0;
            continue;
        }

        /* Fin d'un bloc process */
        if (strcmp(l, "end") == 0) {
            if (!in_proc) {
                fprintf(stderr, "%s:%d: 'end' sans 'process'\n", path, lineno);
                ok = 0; break;
            }
            if (buf[proc_idx].num_bursts == 0) {
                fprintf(stderr, "%s:%d: processus %u sans burst\n",
                        path, lineno, buf[proc_idx].pid);
                ok = 0; break;
            }
            in_proc = 0;
            continue;
        }

        /* Ligne burst à l'intérieur d'un bloc */
        if (in_proc && strncmp(l, "burst", 5) == 0) {
            Process *p = &buf[proc_idx];
            if (p->num_bursts >= MAX_BURSTS) {
                fprintf(stderr, "%s:%d: trop de bursts pour P%u\n",
                        path, lineno, p->pid);
                ok = 0; break;
            }
            BurstEntry *b = &p->burst_table[p->num_bursts];
            b->cpu_duration_ms = 0;
            b->io_duration_ms  = 0;

            char *attr = strstr(l, "cpu=");
            if (attr) b->cpu_duration_ms = (uint32_t)atoi(attr + 4);
            attr = strstr(l, "io=");
            if (attr) b->io_duration_ms = (uint32_t)atoi(attr + 3);

            if (b->cpu_duration_ms == 0) {
                fprintf(stderr, "%s:%d: burst sans durée CPU valide\n",
                        path, lineno);
                ok = 0; break;
            }
            p->num_bursts++;
            continue;
        }

        fprintf(stderr, "%s:%d: ligne non reconnue : %s\n", path, lineno, l);
        ok = 0;
        break;
    }

    fclose(f);

    if (in_proc && ok) {
        fprintf(stderr, "%s: bloc 'process' non fermé (manque 'end')\n", path);
        ok = 0;
    }

    if (!ok) {
        free(buf);
        return -1;
    }

    *procs = buf;
    *n     = (size_t)(proc_idx + 1);
    return 0;
}

/**
 * @brief Crée un jeu de processus simple depuis des arguments CLI.
 *
 * Chaque argument représente un processus avec ses bursts séparés par
 * des virgules. Format : "cpu[,io,cpu[,io,cpu...]]"
 * Exemple : "200,50,100" → burst cpu=200 io=50, burst cpu=100
 *
 * @param argv   Arguments.
 * @param argc   Nombre d'arguments.
 * @param procs  Sortie : tableau alloué.
 * @param n      Sortie : nombre de processus.
 * @return       0 si succès, -1 si erreur.
 */
int input_from_cli(char **argv, int argc, Process **procs, size_t *n)
{
    if (argc <= 0) {
        fprintf(stderr, "Aucun processus spécifié.\n");
        return -1;
    }

    Process *buf = calloc((size_t)argc, sizeof(Process));
    if (!buf) return -1;

    for (int i = 0; i < argc; i++) {
        Process *p = &buf[i];
        p->pid = (uint32_t)(i + 1);
        snprintf(p->name, sizeof(p->name), "P%d", i + 1);
        p->arrival_time_ms = 0;
        p->priority        = 0;
        p->num_bursts      = 0;

        char tmp[256];
        strncpy(tmp, argv[i], sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';

        char *tok = strtok(tmp, ",");
        int   phase = 0; /* 0=cpu, 1=io, alternance */
        while (tok && p->num_bursts < MAX_BURSTS) {
            uint32_t val = (uint32_t)atoi(tok);
            if (phase % 2 == 0) {
                /* CPU burst */
                p->burst_table[p->num_bursts].cpu_duration_ms = val;
                p->burst_table[p->num_bursts].io_duration_ms  = 0;
                p->num_bursts++;
            } else {
                /* I/O associée au burst précédent */
                if (p->num_bursts > 0) {
                    p->burst_table[p->num_bursts - 1].io_duration_ms = val;
                }
            }
            phase++;
            tok = strtok(NULL, ",");
        }

        if (p->num_bursts == 0) {
            fprintf(stderr, "Processus %d : aucun burst valide ('%s')\n",
                    i + 1, argv[i]);
            free(buf);
            return -1;
        }
    }

    *procs = buf;
    *n     = (size_t)argc;
    return 0;
}
