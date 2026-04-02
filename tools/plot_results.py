#!/usr/bin/env python3
"""
plot_results.py — Génération de graphiques depuis le CSV du simulateur.

Usage : python3 tools/plot_results.py <fichier.csv>

Produit deux fichiers PNG dans le même répertoire que le CSV :
  <algo>_metrics.png  — diagramme en barres groupées (turnaround/attente/réponse)
  <algo>_gantt.png    — diagramme de Gantt horizontal (matplotlib barh)

Dépendances : Python 3 + matplotlib (pip install matplotlib)
"""

import sys
import csv
import os

# ---------------------------------------------------------------------------
# Lecture du CSV
# ---------------------------------------------------------------------------

def load_csv(path):
    """
    Lit le fichier CSV et retourne :
      - algo (str) : nom de l'algorithme
      - quantum (int)
      - processes (list of dict) : lignes de données par processus
      - averages (dict) : ligne MOY
      - cpu_util (float) : taux occupation CPU (%)
    """
    processes = []
    averages  = {}
    cpu_util  = 0.0
    algo      = "?"
    quantum   = 0

    with open(path, newline='', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            pid = row.get('pid', '')
            if pid == 'AVG':
                averages = row
                algo    = row.get('algorithm', algo)
                quantum = int(row.get('quantum_ms', 0) or 0)
            elif pid == 'CPU_UTIL':
                util_str = row.get('cpu_ms', '0%').replace('%', '')
                try:
                    cpu_util = float(util_str)
                except ValueError:
                    cpu_util = 0.0
            elif pid.isdigit():
                processes.append({
                    'pid':         int(row['pid']),
                    'name':        row.get('name', f"P{pid}"),
                    'arrival':     int(row.get('arrival_ms', 0) or 0),
                    'finish':      int(row.get('finish_ms', 0) or 0),
                    'turnaround':  int(row.get('turnaround_ms', 0) or 0),
                    'waiting':     int(row.get('waiting_ms', 0) or 0),
                    'response':    int(row.get('response_ms', 0) or 0),
                    'cpu_ms':      int(row.get('cpu_ms', 0) or 0),
                    'io_ms':       int(row.get('io_ms', 0) or 0),
                })
                algo    = row.get('algorithm', algo)
                quantum = int(row.get('quantum_ms', 0) or 0)

    return algo, quantum, processes, averages, cpu_util


# ---------------------------------------------------------------------------
# Graphique 1 : barres groupées (métriques par processus)
# ---------------------------------------------------------------------------

def plot_metrics(algo, quantum, processes, averages, cpu_util, out_path):
    import matplotlib.pyplot as plt
    import matplotlib.patches as mpatches
    import numpy as np

    if not processes:
        print("Aucun processus trouvé dans le CSV.")
        return

    labels     = [p['name'] for p in processes]
    turnaround = [p['turnaround'] for p in processes]
    waiting    = [p['waiting']    for p in processes]
    response   = [p['response']   for p in processes]

    x     = np.arange(len(labels))
    width = 0.25

    fig, ax = plt.subplots(figsize=(max(8, len(labels) * 1.5), 6))

    bars1 = ax.bar(x - width,     turnaround, width, label='Restitution', color='steelblue')
    bars2 = ax.bar(x,             waiting,    width, label='Attente',     color='coral')
    bars3 = ax.bar(x + width,     response,   width, label='Réponse',     color='mediumseagreen')

    # Valeurs sur les barres
    for bar in [*bars1, *bars2, *bars3]:
        h = bar.get_height()
        if h > 0:
            ax.annotate(f'{h}',
                        xy=(bar.get_x() + bar.get_width() / 2, h),
                        xytext=(0, 3), textcoords="offset points",
                        ha='center', va='bottom', fontsize=8)

    # Moyennes en lignes pointillées
    try:
        avg_t = float(averages.get('turnaround_ms', 0) or 0)
        avg_w = float(averages.get('waiting_ms',    0) or 0)
        avg_r = float(averages.get('response_ms',   0) or 0)
        if avg_t > 0:
            ax.axhline(avg_t, color='steelblue',     linestyle='--', alpha=0.5,
                       label=f'Moy restitution={avg_t:.1f}')
        if avg_w > 0:
            ax.axhline(avg_w, color='coral',         linestyle='--', alpha=0.5,
                       label=f'Moy attente={avg_w:.1f}')
        if avg_r > 0:
            ax.axhline(avg_r, color='mediumseagreen', linestyle='--', alpha=0.5,
                       label=f'Moy réponse={avg_r:.1f}')
    except (ValueError, TypeError):
        pass

    title = f"Métriques d'ordonnancement — {algo.upper()}"
    if quantum > 0:
        title += f" (q={quantum} ms)"
    ax.set_title(title)
    ax.set_xlabel("Processus")
    ax.set_ylabel("Temps (ms)")
    ax.set_xticks(x)
    ax.set_xticklabels(labels, rotation=15, ha='right')
    ax.legend(loc='upper right', fontsize=8)
    ax.set_ylim(bottom=0)

    # CPU util en bas
    fig.text(0.5, 0.01,
             f"Taux d'occupation CPU : {cpu_util:.1f}%",
             ha='center', fontsize=9, style='italic')

    plt.tight_layout(rect=[0, 0.04, 1, 1])
    plt.savefig(out_path, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"Graphique métriques : {out_path}")


# ---------------------------------------------------------------------------
# Graphique 2 : Gantt horizontal (barh)
# ---------------------------------------------------------------------------

def plot_gantt(algo, quantum, processes, out_path):
    import matplotlib.pyplot as plt
    import matplotlib.patches as mpatches

    if not processes:
        return

    fig, ax = plt.subplots(figsize=(max(10, len(processes) * 1.2), max(4, len(processes) * 0.8)))

    colors = plt.cm.tab10.colors
    legend_patches = []

    for idx, p in enumerate(processes):
        color = colors[idx % len(colors)]
        # Barre CPU (arrivée → fin, en simplifiant : on montre la durée CPU active)
        # Pour un Gantt exact il faudrait les segments bruts — on utilise finish-waiting
        cpu_start = p['arrival'] + p['response']
        cpu_end   = p['finish']
        if cpu_end > cpu_start:
            ax.barh(p['name'], cpu_end - cpu_start,
                    left=cpu_start, height=0.5,
                    color=color, edgecolor='white', linewidth=0.5)
        # Barre attente (depuis arrivée jusqu'au premier run)
        if p['response'] > 0:
            ax.barh(p['name'], p['response'],
                    left=p['arrival'], height=0.5,
                    color=color, alpha=0.3, edgecolor='white', linewidth=0.5)
        legend_patches.append(
            mpatches.Patch(color=color, label=f"P{p['pid']} {p['name']}"))

    title = f"Gantt simplifié — {algo.upper()}"
    if quantum > 0:
        title += f" (q={quantum} ms)"
    ax.set_title(title)
    ax.set_xlabel("Temps (ms)")
    ax.set_xlim(left=0)
    ax.legend(handles=legend_patches, loc='lower right', fontsize=8)
    ax.invert_yaxis()
    ax.grid(axis='x', linestyle='--', alpha=0.4)

    plt.tight_layout()
    plt.savefig(out_path, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"Graphique Gantt     : {out_path}")


# ---------------------------------------------------------------------------
# Point d'entrée
# ---------------------------------------------------------------------------

def main():
    if len(sys.argv) < 2:
        print(f"Usage : python3 {sys.argv[0]} <fichier.csv>")
        sys.exit(1)

    csv_path = sys.argv[1]
    if not os.path.isfile(csv_path):
        print(f"Erreur : fichier introuvable : {csv_path}")
        sys.exit(1)

    try:
        import matplotlib
        matplotlib.use('Agg')  # rendu sans fenêtre (compatible serveur)
    except ImportError:
        print("Erreur : matplotlib n'est pas installé.")
        print("  Installez-le avec : pip install matplotlib")
        sys.exit(1)

    algo, quantum, processes, averages, cpu_util = load_csv(csv_path)

    base = os.path.splitext(csv_path)[0]
    plot_metrics(algo, quantum, processes, averages, cpu_util,
                 base + "_metrics.png")
    plot_gantt(algo, quantum, processes,
               base + "_gantt.png")


if __name__ == "__main__":
    main()
