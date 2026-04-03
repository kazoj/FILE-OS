/**
 * @file output_gtk.c
 * @brief IHM GTK4 : fenêtre native avec diagramme de Gantt (Cairo)
 *        et tableau de métriques.
 *
 * Compilé uniquement si GTK4 est détecté par le Makefile (HAVE_GTK4).
 * Activé par l'option -G en ligne de commande.
 *
 * Layout :
 *   ┌────────────────────────────────────────────────────────┐
 *   │  [Panneau gauche : infos]  │  [Paned vertical]        │
 *   │  - Algorithme              │  Haut : Gantt (Cairo)    │
 *   │  - Quantum                 │  Bas  : Métriques (Grid) │
 *   │  - CPU%, durée             │                          │
 *   │  - Moyennes                │                          │
 *   └────────────────────────────────────────────────────────┘
 */

#ifdef HAVE_GTK4

#include "output_gtk.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Constantes de rendu Gantt                                          */
/* ------------------------------------------------------------------ */

/** Marge gauche réservée aux noms de processus (px). */
#define GANTT_MARGIN_L   85
/** Marge haute réservée à l'axe des temps (px). */
#define GANTT_MARGIN_T   38
/** Marge basse réservée à la légende (px). */
#define GANTT_MARGIN_B   28
/** Hauteur minimale d'une ligne Gantt (px). */
#define GANTT_ROW_H_MIN  22

/* ------------------------------------------------------------------ */
/*  Structure de données partagée entre les callbacks GTK4             */
/* ------------------------------------------------------------------ */

/**
 * @brief Données passées aux callbacks GTK4 via gpointer.
 */
typedef struct {
    const SimulationState *sim;    /**< Résultats de simulation */
    const MetricsReport   *report; /**< Rapport de métriques */
} AppData;

/* ------------------------------------------------------------------ */
/*  Palette de couleurs Cairo                                          */
/* ------------------------------------------------------------------ */

static void color_cpu(cairo_t *cr)
{
    cairo_set_source_rgb(cr, 0.298, 0.686, 0.314); /* vert matériel */
}
static void color_io(cairo_t *cr)
{
    cairo_set_source_rgb(cr, 0.961, 0.576, 0.114); /* orange matériel */
}
static void color_idle(cairo_t *cr)
{
    cairo_set_source_rgb(cr, 0.62, 0.62, 0.62);    /* gris neutre */
}
static void color_bg(cairo_t *cr)
{
    cairo_set_source_rgb(cr, 0.97, 0.97, 0.97);    /* fond clair */
}
static void color_text(cairo_t *cr)
{
    cairo_set_source_rgb(cr, 0.12, 0.12, 0.12);    /* texte quasi-noir */
}
static void color_header(cairo_t *cr)
{
    cairo_set_source_rgb(cr, 0.18, 0.40, 0.73);    /* bleu titre */
}
static void color_grid_line(cairo_t *cr)
{
    cairo_set_source_rgba(cr, 0.60, 0.60, 0.60, 0.45);
}

/* ------------------------------------------------------------------ */
/*  Dessin du diagramme de Gantt (callback GtkDrawingArea)            */
/* ------------------------------------------------------------------ */

/**
 * @brief Fonction de dessin Cairo appelée par GTK4 à chaque redessinage.
 *
 * @param area      Zone de dessin.
 * @param cr        Contexte Cairo.
 * @param width     Largeur en pixels.
 * @param height    Hauteur en pixels.
 * @param user_data Pointeur vers AppData.
 */
static void draw_gantt_cb(GtkDrawingArea *area, cairo_t *cr,
                          int width, int height, gpointer user_data)
{
    (void)area;
    const AppData         *data   = (const AppData *)user_data;
    const SimulationState *sim    = data->sim;
    const MetricsReport   *report = data->report;

    if (!sim || !report || sim->gantt_count == 0) return;

    const size_t   n_procs  = report->num_processes;
    const uint32_t total_ms = report->total_simulation_ms;
    const int      n_rows   = (int)n_procs + 1; /* +1 pour la ligne IDLE */

    /* ---- Dimensions ---- */
    double usable_w = (double)(width  - GANTT_MARGIN_L - 8);
    double usable_h = (double)(height - GANTT_MARGIN_T - GANTT_MARGIN_B);
    double row_h    = (n_rows > 0) ? (usable_h / n_rows) : GANTT_ROW_H_MIN;
    if (row_h < GANTT_ROW_H_MIN) row_h = GANTT_ROW_H_MIN;

    double scale = (total_ms > 0) ? (usable_w / (double)total_ms) : 1.0;

    /* ---- Fond ---- */
    color_bg(cr);
    cairo_paint(cr);

    /* ---- Titre ---- */
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 12.5);
    color_header(cr);
    {
        char title[160];
        if (report->quantum_ms > 0)
            snprintf(title, sizeof(title), "Gantt — %s  (quantum = %u ms)",
                     report->algorithm_name ? report->algorithm_name : "?",
                     report->quantum_ms);
        else
            snprintf(title, sizeof(title), "Gantt — %s",
                     report->algorithm_name ? report->algorithm_name : "?");
        cairo_move_to(cr, (double)GANTT_MARGIN_L, 22.0);
        cairo_show_text(cr, title);
    }

    /* ---- Axe des temps : calcul du pas de graduation ---- */
    uint32_t tick_step = 1;
    if (total_ms > 0) {
        /* Viser environ 10 graduations */
        tick_step = total_ms / 10;
        if (tick_step == 0) tick_step = 1;
        /* Arrondir à la puissance de 10 supérieure */
        uint32_t mag = 1;
        while (mag * 10 <= tick_step) mag *= 10;
        tick_step = ((tick_step + mag - 1) / mag) * mag;
        if (tick_step == 0) tick_step = 1;
    }

    /* ---- Graduations verticales ---- */
    cairo_set_font_size(cr, 9.5);
    cairo_set_line_width(cr, 0.5);
    for (uint32_t t = 0; t <= total_ms; t += tick_step) {
        double x = GANTT_MARGIN_L + (double)t * scale;

        /* Ligne de grille */
        color_grid_line(cr);
        cairo_move_to(cr, x, (double)GANTT_MARGIN_T);
        cairo_line_to(cr, x, (double)GANTT_MARGIN_T + n_rows * row_h);
        cairo_stroke(cr);

        /* Label de l'axe */
        char lbl[16];
        snprintf(lbl, sizeof(lbl), "%u", t);
        color_text(cr);
        cairo_move_to(cr, x - 3.0, (double)GANTT_MARGIN_T - 5.0);
        cairo_show_text(cr, lbl);
    }

    /* ---- Séparateurs horizontaux (lignes entre processus) ---- */
    cairo_set_line_width(cr, 0.3);
    color_grid_line(cr);
    for (int r = 0; r <= n_rows; r++) {
        double y = GANTT_MARGIN_T + r * row_h;
        cairo_move_to(cr, (double)GANTT_MARGIN_L, y);
        cairo_line_to(cr, (double)(width - 8), y);
        cairo_stroke(cr);
    }

    /* ---- Noms des processus (axe Y) ---- */
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10.0);
    color_text(cr);
    for (size_t i = 0; i < n_procs; i++) {
        double y = GANTT_MARGIN_T + (double)i * row_h + row_h / 2.0 + 4.0;
        cairo_move_to(cr, 4.0, y);
        cairo_show_text(cr, report->per_process[i].name);
    }
    /* Étiquette IDLE */
    {
        double y = GANTT_MARGIN_T + (double)n_procs * row_h + row_h / 2.0 + 4.0;
        cairo_move_to(cr, 4.0, y);
        cairo_show_text(cr, "IDLE");
    }

    /* ---- Rectangles Gantt ---- */
    cairo_set_line_width(cr, 0.6);
    for (size_t e = 0; e < sim->gantt_count; e++) {
        const GanttEntry *g = &sim->gantt[e];

        double x1 = GANTT_MARGIN_L + (double)g->start_ms * scale;
        double x2 = GANTT_MARGIN_L + (double)g->end_ms   * scale;
        double bw  = x2 - x1;
        if (bw < 0.5) bw = 0.5;

        /* Trouver la ligne du processus */
        int row = -1;
        if (g->pid == 0) {
            row = (int)n_procs; /* Ligne IDLE = dernière */
        } else {
            for (size_t i = 0; i < n_procs; i++) {
                if (report->per_process[i].pid == g->pid) {
                    row = (int)i;
                    break;
                }
            }
        }
        if (row < 0) continue;

        double y1 = GANTT_MARGIN_T + (double)row * row_h + 2.0;
        double bh  = row_h - 4.0;
        if (bh < 2.0) bh = 2.0;

        /* Couleur selon état */
        if (g->pid == 0) {
            color_idle(cr);
        } else if (g->is_io) {
            color_io(cr);
        } else {
            color_cpu(cr);
        }
        cairo_rectangle(cr, x1, y1, bw, bh);
        cairo_fill_preserve(cr);

        /* Bordure légère */
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.12);
        cairo_stroke(cr);
    }

    /* ---- Légende ---- */
    double lx = (double)GANTT_MARGIN_L;
    double ly  = GANTT_MARGIN_T + n_rows * row_h + 6.0;
    cairo_set_font_size(cr, 9.5);

    color_cpu(cr);
    cairo_rectangle(cr, lx, ly, 11.0, 11.0);
    cairo_fill(cr);
    color_text(cr);
    cairo_move_to(cr, lx + 14.0, ly + 9.5);
    cairo_show_text(cr, "CPU actif");

    lx += 85.0;
    color_io(cr);
    cairo_rectangle(cr, lx, ly, 11.0, 11.0);
    cairo_fill(cr);
    color_text(cr);
    cairo_move_to(cr, lx + 14.0, ly + 9.5);
    cairo_show_text(cr, "E/S (bloqué)");

    lx += 90.0;
    color_idle(cr);
    cairo_rectangle(cr, lx, ly, 11.0, 11.0);
    cairo_fill(cr);
    color_text(cr);
    cairo_move_to(cr, lx + 14.0, ly + 9.5);
    cairo_show_text(cr, "CPU inactif");
}

/* ------------------------------------------------------------------ */
/*  Construction du tableau de métriques (GtkGrid)                    */
/* ------------------------------------------------------------------ */

/**
 * @brief Construit un GtkScrolledWindow contenant un GtkGrid de métriques.
 *
 * @param report  Rapport de métriques.
 * @return        Widget GtkScrolledWindow peuplé.
 */
static GtkWidget *build_metrics_widget(const MetricsReport *report)
{
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 16);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 3);
    gtk_widget_set_margin_start(grid, 12);
    gtk_widget_set_margin_end(grid, 12);
    gtk_widget_set_margin_top(grid, 8);
    gtk_widget_set_margin_bottom(grid, 8);

    /* En-têtes de colonnes */
    static const char *headers[] = {
        "PID", "Nom", "Arrivée (ms)", "Fin (ms)",
        "Restitution (ms)", "Attente (ms)", "Réponse (ms)",
        "CPU (ms)", "E/S (ms)"
    };
    const int n_cols = 9;

    for (int c = 0; c < n_cols; c++) {
        char markup[128];
        snprintf(markup, sizeof(markup), "<b>%s</b>", headers[c]);
        GtkWidget *lbl = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(lbl), markup);
        gtk_label_set_xalign(GTK_LABEL(lbl), 0.5f);
        gtk_widget_set_hexpand(lbl, TRUE);
        gtk_grid_attach(GTK_GRID(grid), lbl, c, 0, 1, 1);
    }

    /* Séparateur sous les en-têtes */
    GtkWidget *sep1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_grid_attach(GTK_GRID(grid), sep1, 0, 1, n_cols, 1);

    /* Lignes de données */
    for (size_t i = 0; i < report->num_processes; i++) {
        const ProcessMetrics *pm  = &report->per_process[i];
        int                   row = (int)i + 2;
        char                  bufs[9][32];

        snprintf(bufs[0], 32, "%u",  pm->pid);
        snprintf(bufs[1], 32, "%s",  pm->name);
        snprintf(bufs[2], 32, "%u",  pm->arrival_time_ms);
        snprintf(bufs[3], 32, "%u",  pm->finish_time_ms);
        snprintf(bufs[4], 32, "%u",  pm->turnaround_time_ms);
        snprintf(bufs[5], 32, "%u",  pm->waiting_time_ms);
        snprintf(bufs[6], 32, "%u",  pm->response_time_ms);
        snprintf(bufs[7], 32, "%u",  pm->total_cpu_ms);
        snprintf(bufs[8], 32, "%u",  pm->total_io_ms);

        for (int c = 0; c < n_cols; c++) {
            GtkWidget *lbl = gtk_label_new(bufs[c]);
            gtk_label_set_xalign(GTK_LABEL(lbl), 0.5f);
            gtk_grid_attach(GTK_GRID(grid), lbl, c, row, 1, 1);
        }
    }

    /* Séparateur avant la ligne des moyennes */
    int avg_row = (int)report->num_processes + 2;
    GtkWidget *sep2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_grid_attach(GTK_GRID(grid), sep2, 0, avg_row, n_cols, 1);
    avg_row++;

    /* Ligne des moyennes */
    char avg_bufs[9][32];
    snprintf(avg_bufs[0], 32, "—");
    snprintf(avg_bufs[1], 32, "MOYENNE");
    snprintf(avg_bufs[2], 32, "—");
    snprintf(avg_bufs[3], 32, "—");
    snprintf(avg_bufs[4], 32, "%.1f", report->avg_turnaround_ms);
    snprintf(avg_bufs[5], 32, "%.1f", report->avg_waiting_ms);
    snprintf(avg_bufs[6], 32, "%.1f", report->avg_response_ms);
    snprintf(avg_bufs[7], 32, "—");
    snprintf(avg_bufs[8], 32, "—");

    for (int c = 0; c < n_cols; c++) {
        char markup[64];
        snprintf(markup, sizeof(markup), "<i>%s</i>", avg_bufs[c]);
        GtkWidget *lbl = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(lbl), markup);
        gtk_label_set_xalign(GTK_LABEL(lbl), 0.5f);
        gtk_grid_attach(GTK_GRID(grid), lbl, c, avg_row, 1, 1);
    }

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), grid);
    return scroll;
}

/* ------------------------------------------------------------------ */
/*  Construction du panneau gauche (informations de simulation)       */
/* ------------------------------------------------------------------ */

/**
 * @brief Construit le panneau latéral gauche avec les résumés de métriques.
 *
 * @param report  Rapport de métriques.
 * @return        Widget GtkBox peuplé.
 */
static GtkWidget *build_info_panel(const MetricsReport *report)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_widget_set_margin_top(box, 16);
    gtk_widget_set_margin_bottom(box, 12);
    gtk_widget_set_size_request(box, 175, -1);

    /* Titre du panneau */
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<b><big>Résultats</big></b>");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_box_append(GTK_BOX(box), title);

    gtk_box_append(GTK_BOX(box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    /* Helper macro-like inline function to add a label */
    #define ADD_LABEL(markup_str) do {                              \
        GtkWidget *_l = gtk_label_new(NULL);                       \
        gtk_label_set_markup(GTK_LABEL(_l), (markup_str));        \
        gtk_label_set_xalign(GTK_LABEL(_l), 0.0f);                \
        gtk_label_set_wrap(GTK_LABEL(_l), TRUE);                  \
        gtk_box_append(GTK_BOX(box), _l);                         \
    } while(0)

    char buf[256];

    /* Algorithme */
    snprintf(buf, sizeof(buf), "<b>Algorithme</b>\n%s",
             report->algorithm_name ? report->algorithm_name : "—");
    ADD_LABEL(buf);

    /* Quantum (si applicable) */
    if (report->quantum_ms > 0) {
        snprintf(buf, sizeof(buf), "<b>Quantum</b>\n%u ms", report->quantum_ms);
        ADD_LABEL(buf);
    }

    /* Nombre de processus */
    snprintf(buf, sizeof(buf), "<b>Processus</b>\n%zu", report->num_processes);
    ADD_LABEL(buf);

    gtk_box_append(GTK_BOX(box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    /* Taux CPU */
    snprintf(buf, sizeof(buf), "<b>Taux CPU</b>\n%.1f %%",
             report->cpu_utilization_pct);
    ADD_LABEL(buf);

    /* Durée totale */
    snprintf(buf, sizeof(buf), "<b>Durée totale</b>\n%u ms",
             report->total_simulation_ms);
    ADD_LABEL(buf);

    gtk_box_append(GTK_BOX(box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    /* Moyennes */
    snprintf(buf, sizeof(buf), "<b>Moy. restitution</b>\n%.1f ms",
             report->avg_turnaround_ms);
    ADD_LABEL(buf);

    snprintf(buf, sizeof(buf), "<b>Moy. attente</b>\n%.1f ms",
             report->avg_waiting_ms);
    ADD_LABEL(buf);

    snprintf(buf, sizeof(buf), "<b>Moy. réponse</b>\n%.1f ms",
             report->avg_response_ms);
    ADD_LABEL(buf);

    #undef ADD_LABEL

    return box;
}

/* ------------------------------------------------------------------ */
/*  Callback d'activation GTK4 (crée la fenêtre principale)          */
/* ------------------------------------------------------------------ */

/**
 * @brief Callback appelé quand l'application GTK4 est activée.
 *
 * Construit et affiche la fenêtre principale avec ses trois zones :
 * panneau gauche, Gantt, et tableau de métriques.
 *
 * @param app       Application GTK4.
 * @param user_data Pointeur vers AppData.
 */
static void on_activate(GtkApplication *app, gpointer user_data)
{
    AppData *data = (AppData *)user_data;

    /* ---- Fenêtre principale ---- */
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window),
                         "Process Scheduler Simulator — Résultats");
    gtk_window_set_default_size(GTK_WINDOW(window), 1280, 720);

    /* ---- Paned horizontal racine (gauche | droite) ---- */
    GtkWidget *root_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);

    /* Panneau gauche : infos dans un scrolled window */
    GtkWidget *info_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(info_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(info_scroll),
                                  build_info_panel(data->report));
    gtk_paned_set_start_child(GTK_PANED(root_paned), info_scroll);
    gtk_paned_set_resize_start_child(GTK_PANED(root_paned), FALSE);
    gtk_paned_set_shrink_start_child(GTK_PANED(root_paned), FALSE);
    gtk_paned_set_position(GTK_PANED(root_paned), 190);

    /* ---- Paned vertical à droite (Gantt | Métriques) ---- */
    GtkWidget *right_paned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);

    /* Zone Gantt : DrawingArea dans ScrolledWindow */
    GtkWidget *gantt_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(gantt_scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    GtkWidget *gantt_area = gtk_drawing_area_new();
    /* Taille minimale proportionnelle au nombre de processus */
    int min_gantt_h = GANTT_MARGIN_T
                    + (int)(data->report->num_processes + 1) * (GANTT_ROW_H_MIN + 4)
                    + GANTT_MARGIN_B + 20;
    gtk_widget_set_size_request(gantt_area, 600, min_gantt_h);
    gtk_widget_set_hexpand(gantt_area, TRUE);
    gtk_widget_set_vexpand(gantt_area, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(gantt_area),
                                   draw_gantt_cb, data, NULL);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(gantt_scroll),
                                  gantt_area);

    gtk_paned_set_start_child(GTK_PANED(right_paned), gantt_scroll);
    gtk_paned_set_resize_start_child(GTK_PANED(right_paned), TRUE);
    gtk_paned_set_shrink_start_child(GTK_PANED(right_paned), FALSE);

    /* Zone métriques : GtkGrid dans ScrolledWindow */
    GtkWidget *metrics_w = build_metrics_widget(data->report);
    gtk_widget_set_vexpand(metrics_w, TRUE);
    gtk_paned_set_end_child(GTK_PANED(right_paned), metrics_w);
    gtk_paned_set_resize_end_child(GTK_PANED(right_paned), TRUE);
    gtk_paned_set_shrink_end_child(GTK_PANED(right_paned), FALSE);
    /* Position du séparateur vertical (Gantt prend ~60% de la hauteur) */
    gtk_paned_set_position(GTK_PANED(right_paned), 430);

    gtk_paned_set_end_child(GTK_PANED(root_paned), right_paned);

    /* ---- Affichage ---- */
    gtk_window_set_child(GTK_WINDOW(window), root_paned);
    gtk_window_present(GTK_WINDOW(window));
}

/* ------------------------------------------------------------------ */
/*  Point d'entrée public                                              */
/* ------------------------------------------------------------------ */

/**
 * @brief Lance la fenêtre GTK4 de visualisation.
 *
 * Crée un GtkApplication, lui connecte le callback on_activate,
 * et exécute la boucle d'événements GTK jusqu'à la fermeture
 * de la fenêtre. Retourne après fermeture.
 *
 * @param sim     Simulation terminée (doit rester valide pendant l'appel).
 * @param report  Rapport de métriques (doit rester valide pendant l'appel).
 */
void output_gtk_render(const SimulationState *sim,
                       const MetricsReport   *report)
{
    AppData data = { .sim = sim, .report = report };

    GtkApplication *app = gtk_application_new(
        "fr.univ.scheduler",
        G_APPLICATION_DEFAULT_FLAGS
    );

    g_signal_connect(app, "activate", G_CALLBACK(on_activate), &data);
    g_application_run(G_APPLICATION(app), 0, NULL);
    g_object_unref(app);
}

#endif /* HAVE_GTK4 */
