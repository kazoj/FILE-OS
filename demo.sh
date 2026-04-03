#!/usr/bin/env bash
# =============================================================================
#  DEMO — file-OS Scheduler  (~3 min)
# =============================================================================

C='\033[1;36m'; G='\033[1;32m'; Y='\033[1;33m'; R='\033[0m'
SIM="tests/sample_inputs/io_heavy.sim"

banner() { echo -e "\n${C}━━━  $*  ━━━${R}\n"; sleep 1; }
run()    { echo -e "${G}▶  $*${R}"; eval "$@"; sleep 2; }

# ── 1. Découverte ────────────────────────────────────────────────────────────
banner "Algorithmes disponibles"
run ./scheduler list

banner "Options de la commande run"
run ./scheduler help run

# ── 2. Comparaison des 4 algos sur io_heavy.sim ──────────────────────────────
banner "FIFO  — avec Gantt et mode verbeux"
run ./scheduler run fifo  $SIM --gantt --verbose

banner "SJF   — Shortest Job First"
run ./scheduler run sjf   $SIM --gantt

banner "SRJF  — version préemptive de SJF"
run ./scheduler run srjf  $SIM --gantt

banner "Round Robin  (quantum = 50)"
run ./scheduler run rr    $SIM --quantum 50 --gantt

# ── 3. Options avancées de Round Robin ───────────────────────────────────────
banner "RR — sans Gantt  (métriques seules)"
run ./scheduler run rr $SIM --quantum 50

banner "RR — I/O séquentielles"
run ./scheduler run rr $SIM --quantum 50 --sequential-io

banner "RR — export CSV"
run ./scheduler run rr $SIM --quantum 50 --csv

banner "RR — visualisation graphique (GUI)"
run ./scheduler run rr $SIM --quantum 50 --gui

# ── 4. Processus définis en ligne de commande ─────────────────────────────────
banner "Processus inline  (sans fichier .sim)"
run ./scheduler run fifo \
  --process "WebServer:cpu=150,io=80,cpu=100" \
  --process "Daemon:cpu=200,io=50,cpu=80,io=30,cpu=60" \
  --process "Shell:cpu=90" \
  --gantt

# ── 5. Export graphique ───────────────────────────────────────────────────────
banner "SRJF sur basic.sim — export --plot"
run ./scheduler run srjf tests/sample_inputs/basic.sim --plot

echo -e "\n${Y}Demo terminée.${R}\n"
