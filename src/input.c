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
 * @brief Affiche l'aide générale (mode legacy et sous-commandes).
 * @param prog  Nom du programme.
 */
void cli_usage(const char *prog)
{
    printf("Usage: %s <sous-commande> [options]\n\n", prog);
    printf("Sous-commandes :\n");
    printf("  run <algo>      Lancer une simulation\n");
    printf("  list            Lister les algorithmes disponibles\n");
    printf("  help [cmd]      Afficher l'aide d'une sous-commande\n");
    printf("  interactive     Mode guidé interactif\n\n");
    printf("Ancienne syntaxe (rétrocompatibilité) :\n");
    printf("  %s -a <algo> [options] [fichier.sim]\n\n", prog);
    printf("Exemples :\n");
    printf("  %s run rr tests/sample_inputs/basic.sim --quantum 50 --gantt\n", prog);
    printf("  %s run fifo --process \"P1:cpu=200,io=50\" --process \"P2:cpu=300\"\n", prog);
    printf("  %s -a rr -q 50 -g -f tests/sample_inputs/basic.sim\n", prog);
}

/**
 * @brief Affiche l'aide détaillée d'une sous-commande.
 * @param prog  Nom du programme.
 * @param sub   Nom de la sous-commande, ou NULL pour l'aide générale.
 */
void cli_usage_subcommand(const char *prog, const char *sub)
{
    if (!sub || strcmp(sub, "run") == 0) {
        printf("Usage: %s run <algo> [options] [fichier.sim]\n\n", prog);
        printf("Algorithmes : fifo, sjf, srjf, rr\n\n");
        printf("Options :\n");
        printf("  --quantum <ms>       Quantum pour Round Robin (défaut : 50 ms)\n");
        printf("  --gantt              Afficher le diagramme de Gantt ASCII\n");
        printf("  --gui                Ouvrir l'IHM GTK4 native\n");
        printf("  --plot               Générer les graphiques PNG via Python\n");
        printf("  --sequential-io      E/S non parallélisables (device unique)\n");
        printf("  --verbose            Mode verbeux\n");
        printf("  --output <fichier>   Chemin de sortie CSV (défaut : auto)\n");
        printf("  --process <spec>     Ajouter un processus manuellement\n");
        printf("                       Format : \"[Nom:]cpu=<ms>[,io=<ms>,cpu=<ms>...]\"\n");
        printf("                       Exemple : \"P1:cpu=200,io=50,cpu=100\"\n");
        printf("  -h / --help          Cette aide\n\n");
        printf("Exemples :\n");
        printf("  %s run rr tests/basic.sim --quantum 50 --gantt\n", prog);
        printf("  %s run fifo --process \"P1:cpu=200,io=50\" --process \"P2:cpu=300\"\n", prog);
        printf("  %s run srjf tests/io_heavy.sim --verbose --plot\n\n", prog);
        return;
    }
    if (strcmp(sub, "list") == 0) {
        printf("Usage: %s list\n\n", prog);
        printf("Affiche les algorithmes d'ordonnancement disponibles.\n\n");
        return;
    }
    if (strcmp(sub, "interactive") == 0) {
        printf("Usage: %s interactive\n\n", prog);
        printf("Lance un assistant interactif qui guide la configuration\n");
        printf("de la simulation pas à pas.\n\n");
        return;
    }
    /* Sous-commande inconnue → aide générale */
    cli_usage(prog);
}

/* ------------------------------------------------------------------ */
/*  Parseur interne : options communes à run et mode legacy            */
/* ------------------------------------------------------------------ */

/**
 * @brief Parse une option longue ou courte et met à jour cfg.
 *
 * @param argv    Tableau d'arguments.
 * @param argc    Taille du tableau.
 * @param i       Indice courant (modifié si l'option consomme une valeur).
 * @param cfg     Configuration à remplir.
 * @param prog    Nom du programme (pour les messages d'erreur).
 * @return        0 si option reconnue, 1 si option inconnue, -1 si erreur.
 */
static int parse_one_option(char **argv, int argc, int *i,
                            CLIConfig *cfg, const char *prog)
{
    const char *opt = argv[*i];

    if (strcmp(opt, "--quantum") == 0 || strcmp(opt, "-q") == 0) {
        if (*i + 1 >= argc) {
            fprintf(stderr, "%s nécessite une valeur en ms.\n", opt);
            return -1;
        }
        cfg->quantum_ms = (uint32_t)atoi(argv[++(*i)]);
        if (cfg->quantum_ms == 0) cfg->quantum_ms = 50;
    } else if (strcmp(opt, "--gantt") == 0 || strcmp(opt, "-g") == 0) {
        cfg->show_gantt = 1;
    } else if (strcmp(opt, "--gui") == 0 || strcmp(opt, "-G") == 0) {
        cfg->show_gtk = 1;
    } else if (strcmp(opt, "--plot") == 0 || strcmp(opt, "-P") == 0) {
        cfg->auto_plot = 1;
    } else if (strcmp(opt, "--sequential-io") == 0 || strcmp(opt, "-S") == 0) {
        cfg->sequential_io = 1;
    } else if (strcmp(opt, "--verbose") == 0 || strcmp(opt, "-v") == 0) {
        cfg->verbose = 1;
    } else if (strcmp(opt, "--output") == 0 || strcmp(opt, "-o") == 0) {
        if (*i + 1 >= argc) {
            fprintf(stderr, "%s nécessite un chemin de fichier.\n", opt);
            return -1;
        }
        cfg->csv_output = argv[++(*i)];
    } else if (strcmp(opt, "--process") == 0) {
        if (*i + 1 >= argc) {
            fprintf(stderr, "--process nécessite une spec de processus.\n");
            return -1;
        }
        if (cfg->process_count < MAX_PROCESS_SPECS)
            cfg->process_specs[cfg->process_count++] = argv[++(*i)];
    } else {
        (void)prog;
        return 1; /* option non reconnue ici, à traiter par l'appelant */
    }
    return 0;
}

/**
 * @brief Parse les arguments de la ligne de commande.
 *
 * Supporte deux modes :
 * - Sous-commandes : argv[1] ∈ {run, list, help, interactive}
 * - Legacy         : argv[1] commence par '-'
 *
 * @param argc  Nombre d'arguments.
 * @param argv  Tableau d'arguments.
 * @param cfg   Structure de configuration à remplir.
 * @return      0 succès, -1 erreur/aide, -2 liste des algos (-l legacy).
 */
int cli_parse(int argc, char **argv, CLIConfig *cfg)
{
    if (!cfg) return -1;
    memset(cfg, 0, sizeof(CLIConfig));
    cfg->quantum_ms = 50; /* défaut */

    /* Aucun argument → mode interactif */
    if (argc < 2) {
        cfg->subcommand = "interactive";
        return 0;
    }

    /* ---------------------------------------------------------------- */
    /*  Mode sous-commandes : argv[1] ne commence pas par '-'           */
    /* ---------------------------------------------------------------- */
    if (argv[1][0] != '-') {
        const char *sub = argv[1];

        if (strcmp(sub, "list") == 0 || strcmp(sub, "ls") == 0) {
            cfg->subcommand = "list";
            return 0;
        }

        if (strcmp(sub, "interactive") == 0) {
            cfg->subcommand = "interactive";
            return 0;
        }

        if (strcmp(sub, "help") == 0) {
            cfg->subcommand = "help";
            cfg->subcommand_arg = (argc >= 3) ? argv[2] : NULL;
            return 0;
        }

        if (strcmp(sub, "run") == 0) {
            cfg->subcommand = "run";
            if (argc < 3) {
                fprintf(stderr, "Erreur : 'run' requiert un algorithme.\n\n");
                cli_usage_subcommand(argv[0], "run");
                return -1;
            }
            /* argv[2] = algo positionnel */
            cfg->algo = argv[2];

            for (int i = 3; i < argc; i++) {
                /* Argument positionnel : auto-détection fichier .sim */
                if (argv[i][0] != '-') {
                    size_t len = strlen(argv[i]);
                    if (len > 4 && strcmp(argv[i] + len - 4, ".sim") == 0) {
                        cfg->input_file = argv[i];
                    } else {
                        fprintf(stderr, "Argument non reconnu : '%s'\n", argv[i]);
                        cli_usage_subcommand(argv[0], "run");
                        return -1;
                    }
                    continue;
                }
                /* Aide contextuelle */
                if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
                    cli_usage_subcommand(argv[0], "run");
                    return -1;
                }
                /* Options communes */
                int rc = parse_one_option(argv, argc, &i, cfg, argv[0]);
                if (rc == -1) return -1;
                if (rc == 1) {
                    fprintf(stderr, "Option inconnue : '%s'\n", argv[i]);
                    cli_usage_subcommand(argv[0], "run");
                    return -1;
                }
            }

            if (!cfg->input_file && cfg->process_count == 0) {
                fprintf(stderr,
                    "Aucun processus fourni.\n"
                    "Utilisez un fichier .sim ou --process \"P1:cpu=200,...\"\n");
                return -1;
            }
            return 0;
        }

        fprintf(stderr, "Sous-commande inconnue : '%s'\n\n", sub);
        cli_usage(argv[0]);
        return -1;
    }

    /* ---------------------------------------------------------------- */
    /*  Mode legacy : argv[1] commence par '-'                          */
    /* ---------------------------------------------------------------- */
    cfg->subcommand = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            cli_usage(argv[0]);
            return -1;
        } else if (strcmp(argv[i], "-l") == 0) {
            return -2; /* liste des algos, géré dans main.c */
        } else if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) {
            cfg->algo = argv[++i];
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            cfg->input_file = argv[++i];
        } else if (argv[i][0] != '-') {
            /* argument positionnel : traité dans main.c */
        } else {
            int rc = parse_one_option(argv, argc, &i, cfg, argv[0]);
            if (rc == -1) return -1;
            if (rc == 1) {
                fprintf(stderr, "Option inconnue : %s\n", argv[i]);
                cli_usage(argv[0]);
                return -1;
            }
        }
    }

    if (!cfg->algo) {
        fprintf(stderr,
            "Erreur : algorithme non spécifié.\n"
            "Utilisez 'scheduler run <algo>' ou '-a <algo>'.\n");
        cli_usage(argv[0]);
        return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Mode interactif                                                    */
/* ------------------------------------------------------------------ */

/**
 * @brief Lance le wizard interactif pour configurer une simulation.
 *
 * Pose des questions sur stdin et remplit cfg en conséquence.
 * Les réponses vides utilisent la valeur par défaut indiquée entre [].
 *
 * @param cfg  Configuration à remplir.
 * @return     0 si succès, -1 si EOF inattendu.
 */
int cli_interactive(CLIConfig *cfg)
{
    char buf[256];

    /* Buffers statiques : leur durée de vie dépasse le retour de la fonction */
    static char s_algo[32];
    static char s_file[256];
    /* Buffers pour les specs de processus saisies manuellement */
    static char s_proc_bufs[MAX_PROCESS_SPECS][256];

    printf("\n=== Simulateur d'ordonnancement — Mode interactif ===\n\n");

    /* --- Choix de l'algorithme --- */
    printf("Algorithme [fifo/sjf/srjf/rr] (défaut: fifo) : ");
    fflush(stdout);
    if (!fgets(buf, sizeof(buf), stdin)) return -1;
    char *ans = trim(buf);
    strncpy(s_algo, (*ans != '\0') ? ans : "fifo", sizeof(s_algo) - 1);
    s_algo[sizeof(s_algo) - 1] = '\0';
    cfg->algo = s_algo;

    /* --- Quantum (seulement pour RR) --- */
    if (strcmp(cfg->algo, "rr") == 0) {
        printf("Quantum en ms (défaut: 50) : ");
        fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin)) return -1;
        ans = trim(buf);
        cfg->quantum_ms = (*ans != '\0') ? (uint32_t)atoi(ans) : 50;
        if (cfg->quantum_ms == 0) cfg->quantum_ms = 50;
    }

    /* --- Fichier .sim ou saisie manuelle --- */
    printf("Fichier .sim ? (Entrée = saisie manuelle) : ");
    fflush(stdout);
    if (!fgets(buf, sizeof(buf), stdin)) return -1;
    ans = trim(buf);
    if (*ans != '\0') {
        strncpy(s_file, ans, sizeof(s_file) - 1);
        s_file[sizeof(s_file) - 1] = '\0';
        cfg->input_file = s_file;
    } else {
        /* Saisie manuelle des processus */
        printf("\nSaisie des processus (format : [Nom:]cpu=<ms>[,io=<ms>,cpu=<ms>...])\n");
        printf("Laissez vide pour terminer.\n\n");
        cfg->process_count = 0;
        for (int idx = 0; idx < MAX_PROCESS_SPECS; idx++) {
            printf("  Processus %d : ", idx + 1);
            fflush(stdout);
            if (!fgets(buf, sizeof(buf), stdin)) break;
            ans = trim(buf);
            if (*ans == '\0') break;
            strncpy(s_proc_bufs[idx], ans, sizeof(s_proc_bufs[idx]) - 1);
            s_proc_bufs[idx][sizeof(s_proc_bufs[idx]) - 1] = '\0';
            cfg->process_specs[cfg->process_count++] = s_proc_bufs[idx];
        }
        if (cfg->process_count == 0) {
            fprintf(stderr, "Aucun processus saisi.\n");
            return -1;
        }
    }

    /* --- Options de sortie --- */
    printf("\nAfficher le diagramme de Gantt ASCII ? [o/N] : ");
    fflush(stdout);
    if (!fgets(buf, sizeof(buf), stdin)) return -1;
    ans = trim(buf);
    cfg->show_gantt = (*ans == 'o' || *ans == 'O');

    printf("Générer les graphiques PNG ? [o/N] : ");
    fflush(stdout);
    if (!fgets(buf, sizeof(buf), stdin)) return -1;
    ans = trim(buf);
    cfg->auto_plot = (*ans == 'o' || *ans == 'O');

    printf("\n");
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

/* ------------------------------------------------------------------ */
/*  input_from_process_args — specs --process                         */
/* ------------------------------------------------------------------ */

/**
 * @brief Crée un jeu de processus depuis des specs --process.
 *
 * Chaque spec a le format : `[Nom:]cpu=<ms>[,io=<ms>,cpu=<ms>...]`
 * Le préfixe `Nom:` est optionnel. Les tokens sont séparés par des
 * virgules. Les clés reconnues sont `cpu=` et `io=`.
 *
 * Exemples valides :
 *   - `"cpu=200"`
 *   - `"P1:cpu=200,io=50,cpu=100"`
 *   - `"WebServer:cpu=150,io=80,cpu=100,io=40,cpu=50"`
 *
 * @param specs  Tableau de chaînes de spec brutes.
 * @param count  Nombre de specs.
 * @param procs  Sortie : tableau de Process alloué.
 * @param n      Sortie : nombre de processus.
 * @return       0 si succès, -1 si erreur.
 */
int input_from_process_args(const char **specs, int count,
                            Process **procs, size_t *n)
{
    if (count <= 0) {
        fprintf(stderr, "Aucune spec --process fournie.\n");
        return -1;
    }

    Process *buf = calloc((size_t)count, sizeof(Process));
    if (!buf) return -1;

    for (int i = 0; i < count; i++) {
        Process *p = &buf[i];
        p->pid             = (uint32_t)(i + 1);
        p->arrival_time_ms = 0;
        p->priority        = 0;
        p->num_bursts      = 0;
        snprintf(p->name, sizeof(p->name), "P%d", i + 1);

        char work[256];
        strncpy(work, specs[i], sizeof(work) - 1);
        work[sizeof(work) - 1] = '\0';

        /* Détecter un préfixe "Nom:" avant le premier '=' */
        char *colon   = strchr(work, ':');
        char *first_eq = strchr(work, '=');
        const char *burst_part = work;

        if (colon && (!first_eq || colon < first_eq)) {
            *colon = '\0';
            strncpy(p->name, trim(work), sizeof(p->name) - 1);
            p->name[sizeof(p->name) - 1] = '\0';
            burst_part = colon + 1;
        }

        /* Parser les tokens séparés par des virgules */
        char burst_work[256];
        strncpy(burst_work, burst_part, sizeof(burst_work) - 1);
        burst_work[sizeof(burst_work) - 1] = '\0';

        char *tok = strtok(burst_work, ",");
        while (tok && p->num_bursts < MAX_BURSTS) {
            tok = trim(tok);
            if (strncmp(tok, "cpu=", 4) == 0) {
                uint32_t val = (uint32_t)atoi(tok + 4);
                if (val == 0) {
                    fprintf(stderr,
                        "Processus %d : durée cpu=0 invalide dans '%s'\n",
                        i + 1, specs[i]);
                    free(buf);
                    return -1;
                }
                p->burst_table[p->num_bursts].cpu_duration_ms = val;
                p->burst_table[p->num_bursts].io_duration_ms  = 0;
                p->num_bursts++;
            } else if (strncmp(tok, "io=", 3) == 0) {
                if (p->num_bursts == 0) {
                    fprintf(stderr,
                        "Processus %d : 'io=' avant 'cpu=' dans '%s'\n",
                        i + 1, specs[i]);
                    free(buf);
                    return -1;
                }
                p->burst_table[p->num_bursts - 1].io_duration_ms =
                    (uint32_t)atoi(tok + 3);
            } else {
                fprintf(stderr,
                    "Processus %d : token non reconnu '%s' dans '%s'\n"
                    "  Clés valides : cpu=<ms>, io=<ms>\n",
                    i + 1, tok, specs[i]);
                free(buf);
                return -1;
            }
            tok = strtok(NULL, ",");
        }

        if (p->num_bursts == 0) {
            fprintf(stderr,
                "Processus %d : aucun burst cpu= trouvé dans '%s'\n",
                i + 1, specs[i]);
            free(buf);
            return -1;
        }
    }

    *procs = buf;
    *n     = (size_t)count;
    return 0;
}
