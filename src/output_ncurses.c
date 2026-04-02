/**
 * @file output_ncurses.c
 * @brief IHM ncurses : diagramme de Gantt coloré et tableau de métriques.
 *
 * Compilé uniquement si HAVE_NCURSES est défini (détecté par le Makefile).
 *
 * Layout :
 *  - Fenêtre supérieure (40% de la hauteur) : Gantt coloré
 *  - Fenêtre inférieure (60% de la hauteur) : tableau de métriques
 *  - Touches : q = quitter, ←/→ = faire défiler le Gantt
 */

#ifdef HAVE_NCURSES

#include "output_ncurses.h"
#include "simulation.h"
#include "metrics.h"
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Paires de couleurs                                                 */
/* ------------------------------------------------------------------ */
#define CP_CPU    1  /**< Vert  : CPU actif */
#define CP_IO     2  /**< Jaune : E/S (BLOCKED) */
#define CP_IDLE   3  /**< Rouge : IDLE */
#define CP_HEADER 4  /**< Cyan  : en-têtes */
#define CP_DATA   5  /**< Blanc : données */

/* ------------------------------------------------------------------ */
/*  Utilitaires internes                                               */
/* ------------------------------------------------------------------ */

/**
 * @brief Cherche le label Gantt d'un PID au temps `mid`.
 *
 * @return 'C' = CPU, 'I' = I/O, ' ' = absent
 */
static char gantt_label_at(const GanttEntry *gantt, size_t count,
                            uint32_t pid, uint32_t mid)
{
    for (size_t g = 0; g < count; g++) {
        if (gantt[g].pid == pid &&
            gantt[g].start_ms <= mid &&
            mid < gantt[g].end_ms)
        {
            return gantt[g].is_io ? 'I' : 'C';
        }
    }
    return ' ';
}

/* ------------------------------------------------------------------ */
/*  Rendu du Gantt (fenêtre supérieure)                               */
/* ------------------------------------------------------------------ */

/**
 * @brief Dessine le diagramme de Gantt coloré dans `win`.
 *
 * @param win       Fenêtre ncurses cible.
 * @param gantt     Tableau d'entrées Gantt.
 * @param count     Nombre d'entrées.
 * @param report    Rapport (processus, quantum, algo).
 * @param scroll_x  Décalage horizontal en nombre de colonnes.
 */
static void draw_gantt(WINDOW *win,
                       const GanttEntry *gantt, size_t count,
                       const MetricsReport *report,
                       int scroll_x)
{
    if (!win || !gantt || !report) return;

    int win_h, win_w;
    getmaxyx(win, win_h, win_w);
    (void)win_h;

    werase(win);
    box(win, 0, 0);

    /* Titre */
    wattron(win, COLOR_PAIR(CP_HEADER) | A_BOLD);
    mvwprintw(win, 0, 2, " Gantt : %s ", report->algorithm_name);
    if (report->quantum_ms > 0)
        wprintw(win, "(q=%u ms) ", report->quantum_ms);
    wattroff(win, COLOR_PAIR(CP_HEADER) | A_BOLD);

    uint32_t col_ms   = (report->quantum_ms > 0) ? report->quantum_ms : 50;
    uint32_t total_ms = report->total_simulation_ms;
    uint32_t num_cols = (total_ms + col_ms - 1) / col_ms;

    /* Largeur utile (sans les 2 bordures + 6 pour le label PID) */
    int usable_w = win_w - 8;
    if (usable_w < 1) usable_w = 1;
    /* Nombre de colonnes visibles */
    int vis_cols = usable_w / 6; /* chaque cellule = "[CPU ]" = 6 chars */
    if (vis_cols < 1) vis_cols = 1;

    /* Ligne d'en-tête des temps */
    wattron(win, COLOR_PAIR(CP_HEADER));
    mvwprintw(win, 1, 1, "PID  ");
    for (int c = scroll_x; c < scroll_x + vis_cols && (uint32_t)c < num_cols; c++) {
        wprintw(win, "%-6u", (uint32_t)c * col_ms);
    }
    wattroff(win, COLOR_PAIR(CP_HEADER));

    /* Une ligne par processus */
    int row = 2;
    for (size_t pi = 0; pi < report->num_processes && row < win_h - 1; pi++) {
        uint32_t pid = report->per_process[pi].pid;
        wattron(win, COLOR_PAIR(CP_DATA));
        mvwprintw(win, row, 1, "%4u ", pid);
        wattroff(win, COLOR_PAIR(CP_DATA));

        for (int c = scroll_x; c < scroll_x + vis_cols && (uint32_t)c < num_cols; c++) {
            uint32_t mid = (uint32_t)c * col_ms + col_ms / 2;
            char lbl = gantt_label_at(gantt, count, pid, mid);
            if (lbl == 'C') {
                wattron(win, COLOR_PAIR(CP_CPU) | A_BOLD);
                wprintw(win, "[CPU ]");
                wattroff(win, COLOR_PAIR(CP_CPU) | A_BOLD);
            } else if (lbl == 'I') {
                wattron(win, COLOR_PAIR(CP_IO) | A_BOLD);
                wprintw(win, "[I/O ]");
                wattroff(win, COLOR_PAIR(CP_IO) | A_BOLD);
            } else {
                wattron(win, COLOR_PAIR(CP_DATA));
                wprintw(win, "[    ]");
                wattroff(win, COLOR_PAIR(CP_DATA));
            }
        }
        row++;
    }

    /* Ligne IDLE */
    if (row < win_h - 1) {
        wattron(win, COLOR_PAIR(CP_DATA));
        mvwprintw(win, row, 1, "IDLE ");
        wattroff(win, COLOR_PAIR(CP_DATA));
        for (int c = scroll_x; c < scroll_x + vis_cols && (uint32_t)c < num_cols; c++) {
            uint32_t mid = (uint32_t)c * col_ms + col_ms / 2;
            char lbl = gantt_label_at(gantt, count, 0, mid);
            if (lbl == 'C') {
                wattron(win, COLOR_PAIR(CP_IDLE) | A_BOLD);
                wprintw(win, "[IDLE]");
                wattroff(win, COLOR_PAIR(CP_IDLE) | A_BOLD);
            } else {
                wattron(win, COLOR_PAIR(CP_DATA));
                wprintw(win, "[    ]");
                wattroff(win, COLOR_PAIR(CP_DATA));
            }
        }
    }

    /* Indicateur de défilement */
    wattron(win, COLOR_PAIR(CP_HEADER));
    if (scroll_x > 0)
        mvwprintw(win, 1, win_w - 4, " ◄ ");
    if ((uint32_t)(scroll_x + vis_cols) < num_cols)
        mvwprintw(win, 1, win_w - 8, " ► ");
    wattroff(win, COLOR_PAIR(CP_HEADER));

    wrefresh(win);
}

/* ------------------------------------------------------------------ */
/*  Rendu du tableau de métriques (fenêtre inférieure)                */
/* ------------------------------------------------------------------ */

/**
 * @brief Dessine le tableau de métriques dans `win`.
 *
 * @param win     Fenêtre ncurses cible.
 * @param report  Rapport de métriques.
 * @param scroll_y Décalage vertical (lignes de processus).
 */
static void draw_metrics(WINDOW *win,
                         const MetricsReport *report,
                         int scroll_y)
{
    if (!win || !report) return;

    int win_h, win_w;
    getmaxyx(win, win_h, win_w);
    (void)win_w;

    werase(win);
    box(win, 0, 0);

    /* Titre */
    wattron(win, COLOR_PAIR(CP_HEADER) | A_BOLD);
    mvwprintw(win, 0, 2, " Métriques ");
    wattroff(win, COLOR_PAIR(CP_HEADER) | A_BOLD);

    /* En-tête des colonnes */
    wattron(win, COLOR_PAIR(CP_HEADER) | A_UNDERLINE);
    mvwprintw(win, 1, 1,
        "%-4s %-16s %8s %8s %12s %8s %9s %8s %8s",
        "PID", "Nom",
        "Arrivée", "Fin",
        "Restitution", "Attente", "Réponse",
        "CPU(ms)", "E/S(ms)");
    wattroff(win, COLOR_PAIR(CP_HEADER) | A_UNDERLINE);

    /* Lignes de processus */
    int row = 2;
    for (size_t i = (size_t)scroll_y;
         i < report->num_processes && row < win_h - 4;
         i++, row++)
    {
        const ProcessMetrics *pm = &report->per_process[i];
        wattron(win, COLOR_PAIR(CP_DATA));
        mvwprintw(win, row, 1,
            "%-4u %-16s %8u %8u %12u %8u %9u %8u %8u",
            pm->pid, pm->name,
            pm->arrival_time_ms, pm->finish_time_ms,
            pm->turnaround_time_ms, pm->waiting_time_ms,
            pm->response_time_ms,
            pm->total_cpu_ms, pm->total_io_ms);
        wattroff(win, COLOR_PAIR(CP_DATA));
    }

    /* Ligne de moyennes */
    wattron(win, COLOR_PAIR(CP_HEADER) | A_BOLD);
    mvwprintw(win, win_h - 3, 1,
        "%-4s %-16s %8s %8s %12.2f %8.2f %9.2f",
        "MOY", "", "", "",
        report->avg_turnaround_ms,
        report->avg_waiting_ms,
        report->avg_response_ms);
    wattroff(win, COLOR_PAIR(CP_HEADER) | A_BOLD);

    /* Résumé CPU et durée */
    wattron(win, COLOR_PAIR(CP_HEADER));
    mvwprintw(win, win_h - 2, 1,
        "CPU util : %.1f%%   Durée totale : %u ms",
        report->cpu_utilization_pct,
        report->total_simulation_ms);
    wattroff(win, COLOR_PAIR(CP_HEADER));

    wrefresh(win);
}

/* ------------------------------------------------------------------ */
/*  Barre de statut (bas d'écran)                                     */
/* ------------------------------------------------------------------ */

static void draw_statusbar(int rows, int cols)
{
    attron(COLOR_PAIR(CP_HEADER) | A_REVERSE);
    mvhline(rows - 1, 0, ' ', cols);
    mvprintw(rows - 1, 1,
        " q=quitter  ←/→=défiler Gantt  ↑/↓=défiler métriques ");
    attroff(COLOR_PAIR(CP_HEADER) | A_REVERSE);
    refresh();
}

/* ------------------------------------------------------------------ */
/*  Fonction principale                                                */
/* ------------------------------------------------------------------ */

/**
 * @brief Lance l'IHM ncurses interactive.
 */
void output_ncurses_render(const SimulationState *sim,
                           const MetricsReport   *report)
{
    if (!sim || !report) return;

    /* Initialisation ncurses */
    initscr();
    if (!has_colors()) {
        endwin();
        fprintf(stderr, "Le terminal ne supporte pas les couleurs.\n");
        return;
    }
    start_color();
    use_default_colors();

    init_pair(CP_CPU,    COLOR_GREEN,  -1);
    init_pair(CP_IO,     COLOR_YELLOW, -1);
    init_pair(CP_IDLE,   COLOR_RED,    -1);
    init_pair(CP_HEADER, COLOR_CYAN,   -1);
    init_pair(CP_DATA,   -1,           -1);

    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    /* Partage de l'écran : 40% Gantt / 60% métriques (moins barre de statut) */
    int gantt_h   = (rows - 1) * 2 / 5;
    if (gantt_h < 4) gantt_h = 4;
    int metrics_h = rows - 1 - gantt_h;
    if (metrics_h < 4) metrics_h = 4;

    WINDOW *win_gantt   = newwin(gantt_h,   cols, 0,         0);
    WINDOW *win_metrics = newwin(metrics_h, cols, gantt_h,   0);

    int scroll_x  = 0;  /* défilement Gantt (colonnes) */
    int scroll_y  = 0;  /* défilement métriques (lignes) */

    /* Calcul du nombre max de colonnes Gantt pour le défilement */
    uint32_t col_ms   = (report->quantum_ms > 0) ? report->quantum_ms : 50;
    uint32_t total_ms = report->total_simulation_ms;
    int      max_col  = (int)((total_ms + col_ms - 1) / col_ms);
    int      max_row  = (int)report->num_processes;

    /* Boucle d'affichage */
    for (;;) {
        draw_gantt(win_gantt, sim->gantt, sim->gantt_count, report, scroll_x);
        draw_metrics(win_metrics, report, scroll_y);
        draw_statusbar(rows, cols);

        int ch = getch();
        switch (ch) {
        case 'q': case 'Q': case 27: /* ESC */
            goto done;
        case KEY_RIGHT:
            if (scroll_x < max_col - 1) scroll_x++;
            break;
        case KEY_LEFT:
            if (scroll_x > 0) scroll_x--;
            break;
        case KEY_DOWN:
            if (scroll_y < max_row - 1) scroll_y++;
            break;
        case KEY_UP:
            if (scroll_y > 0) scroll_y--;
            break;
        case KEY_RESIZE:
            /* Recalculer les fenêtres si le terminal change de taille */
            getmaxyx(stdscr, rows, cols);
            gantt_h   = (rows - 1) * 2 / 5;
            if (gantt_h < 4) gantt_h = 4;
            metrics_h = rows - 1 - gantt_h;
            if (metrics_h < 4) metrics_h = 4;
            wresize(win_gantt,   gantt_h,   cols);
            wresize(win_metrics, metrics_h, cols);
            mvwin(win_metrics, gantt_h, 0);
            clear();
            refresh();
            break;
        default:
            break;
        }
    }

done:
    delwin(win_gantt);
    delwin(win_metrics);
    endwin();
}

#endif /* HAVE_NCURSES */
