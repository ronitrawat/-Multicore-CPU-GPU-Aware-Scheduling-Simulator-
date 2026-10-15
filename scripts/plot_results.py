#!/usr/bin/env python3
"""
CPU Scheduler Simulator - Phase 4 Visualizer
Plots Gantt charts and performance metrics from CSV / JSON simulator outputs.
Requires: matplotlib, pandas (optional, falls back to csv/json stdlib)
"""

import sys
import os
import argparse
import csv
import json

def plot_gantt_chart(csv_path, output_image="gantt_chart.png"):
    try:
        import matplotlib.pyplot as plt
        import matplotlib.patches as mpatches
    except ImportError:
        print("Error: matplotlib is required for plotting. Please install via: pip install matplotlib")
        return

    segments = []
    with open(csv_path, 'r', newline='') as f:
        reader = csv.DictReader(f)
        for row in reader:
            segments.append({
                'core_id': int(row['core_id']),
                'pid': int(row['pid']),
                'name': row['name'],
                'start_time': float(row['start_time']),
                'end_time': float(row['end_time']),
                'duration': float(row['duration'])
            })

    if not segments:
        print("No segments found in Gantt CSV.")
        return

    cores = sorted(list(set(s['core_id'] for s in segments)))
    unique_pids = sorted(list(set(s['pid'] for s in segments if s['pid'] not in (0, 4294967295))))

    # Colormap for processes
    cmap = plt.get_cmap('tab20')
    color_map = {}
    for idx, pid in enumerate(unique_pids):
        color_map[pid] = cmap(idx % 20)

    color_map[0] = '#E0E0E0'             # Idle: light gray
    color_map[4294967295] = '#D9534F'     # Context Switch: red/salmon

    fig, ax = plt.subplots(figsize=(14, max(4, len(cores) * 2)))

    for seg in segments:
        cid = seg['core_id']
        start = seg['start_time']
        dur = seg['duration']
        pid = seg['pid']
        color = color_map.get(pid, '#888888')

        y_pos = cid
        hatch = '///' if pid == 4294967295 else None
        edge_col = 'black' if dur > 0.5 else 'none'

        rect = ax.barh(y_pos, dur, left=start, height=0.6, align='center',
                       color=color, edgecolor=edge_col, linewidth=0.8, hatch=hatch)

        # Label inside block if wide enough
        if dur >= 1.5:
            label = "CS" if pid == 4294967295 else ("Idle" if pid == 0 else seg['name'])
            ax.text(start + dur / 2.0, y_pos, label, ha='center', va='center',
                    color='white' if pid == 4294967295 else 'black',
                    fontsize=8, fontweight='bold')

    ax.set_yticks(cores)
    ax.set_yticklabels([f"Core {cid}" for cid in cores], fontsize=11, fontweight='bold')
    ax.set_xlabel("Simulation Time (Ticks)", fontsize=11, fontweight='bold')
    ax.set_title("Multicore CPU Scheduling - Execution Gantt Chart", fontsize=14, fontweight='bold', pad=15)
    ax.grid(axis='x', linestyle='--', alpha=0.6)

    # Legend
    legend_handles = []
    for pid in unique_pids:
        name = next((s['name'] for s in segments if s['pid'] == pid), f"P{pid}")
        legend_handles.append(mpatches.Patch(color=color_map[pid], label=name))
    legend_handles.append(mpatches.Patch(color=color_map[0], label="Idle"))
    legend_handles.append(mpatches.Patch(color=color_map[4294967295], hatch='///', label="Context Switch"))

    ax.legend(handles=legend_handles, loc='upper center', bbox_to_anchor=(0.5, -0.15),
              ncol=min(8, len(legend_handles)), fontsize=9, frameon=True)

    plt.tight_layout()
    plt.savefig(output_image, dpi=200, bbox_inches='tight')
    plt.close()
    print(f"Gantt chart successfully saved to: {output_image}")

def plot_comparison_metrics(csv_path, output_image="algorithm_comparison.png"):
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("Error: matplotlib is required. Please install via: pip install matplotlib")
        return

    data = []
    with open(csv_path, 'r', newline='') as f:
        reader = csv.DictReader(f)
        for row in reader:
            data.append(row)

    if not data:
        print("No comparison data found.")
        return

    algos = [r['algorithm'].upper() for r in data]
    avg_tat = [float(r['avg_tat']) for r in data]
    p95_tat = [float(r['p95_tat']) for r in data]
    avg_wait = [float(r['avg_wait']) for r in data]
    p95_wait = [float(r['p95_wait']) for r in data]
    cpu_util = [float(r['cpu_util']) * 100.0 for r in data]
    fairness = [float(r['jains_fairness']) for r in data]
    ctx_sw = [int(r['context_switches']) for r in data]

    fig, axes = plt.subplots(2, 3, figsize=(16, 9))
    fig.suptitle("Scheduling Algorithms Comprehensive Comparison", fontsize=16, fontweight='bold', y=0.98)

    x = range(len(algos))
    bar_width = 0.35

    # 1. Turnaround Time (Avg & P95)
    ax1 = axes[0, 0]
    ax1.bar([i - bar_width/2 for i in x], avg_tat, width=bar_width, label='Avg TAT', color='#2b5c8f')
    ax1.bar([i + bar_width/2 for i in x], p95_tat, width=bar_width, label='P95 TAT', color='#4a90e2')
    ax1.set_xticks(x)
    ax1.set_xticklabels(algos, rotation=25, ha='right', fontsize=9)
    ax1.set_title("Turnaround Time (Ticks)", fontweight='bold')
    ax1.legend()
    ax1.grid(axis='y', linestyle='--', alpha=0.5)

    # 2. Waiting Time (Avg & P95)
    ax2 = axes[0, 1]
    ax2.bar([i - bar_width/2 for i in x], avg_wait, width=bar_width, label='Avg Wait', color='#2e7d32')
    ax2.bar([i + bar_width/2 for i in x], p95_wait, width=bar_width, label='P95 Wait', color='#66bb6a')
    ax2.set_xticks(x)
    ax2.set_xticklabels(algos, rotation=25, ha='right', fontsize=9)
    ax2.set_title("Waiting Time (Ticks)", fontweight='bold')
    ax2.legend()
    ax2.grid(axis='y', linestyle='--', alpha=0.5)

    # 3. CPU Utilization
    ax3 = axes[0, 2]
    ax3.bar(x, cpu_util, width=0.5, color='#d97706')
    ax3.set_xticks(x)
    ax3.set_xticklabels(algos, rotation=25, ha='right', fontsize=9)
    ax3.set_title("CPU Utilization (%)", fontweight='bold')
    ax3.set_ylim(0, 105)
    ax3.grid(axis='y', linestyle='--', alpha=0.5)

    # 4. Jain's Fairness Index
    ax4 = axes[1, 0]
    ax4.bar(x, fairness, width=0.5, color='#7c3aed')
    ax4.set_xticks(x)
    ax4.set_xticklabels(algos, rotation=25, ha='right', fontsize=9)
    ax4.set_title("Jain's Fairness Index (1.0 = Max)", fontweight='bold')
    ax4.set_ylim(0, 1.1)
    ax4.grid(axis='y', linestyle='--', alpha=0.5)

    # 5. Context Switches
    ax5 = axes[1, 1]
    ax5.bar(x, ctx_sw, width=0.5, color='#dc2626')
    ax5.set_xticks(x)
    ax5.set_xticklabels(algos, rotation=25, ha='right', fontsize=9)
    ax5.set_title("Context Switches Total", fontweight='bold')
    ax5.grid(axis='y', linestyle='--', alpha=0.5)

    # 6. Makespan (Total Completion Time)
    makespans = [float(r['makespan']) for r in data]
    ax6 = axes[1, 2]
    ax6.bar(x, makespans, width=0.5, color='#0891b2')
    ax6.set_xticks(x)
    ax6.set_xticklabels(algos, rotation=25, ha='right', fontsize=9)
    ax6.set_title("Total Makespan (Ticks)", fontweight='bold')
    ax6.grid(axis='y', linestyle='--', alpha=0.5)

    plt.tight_layout()
    plt.savefig(output_image, dpi=200, bbox_inches='tight')
    plt.close()
    print(f"Comparison metrics chart successfully saved to: {output_image}")

def main():
    parser = argparse.ArgumentParser(description="Plot CPU Scheduler Simulation Results")
    parser.add_argument("--gantt", help="Path to Gantt timeline CSV file")
    parser.add_argument("--comparison", help="Path to algorithm comparison CSV file")
    parser.add_argument("--output-dir", default=".", help="Directory to save output plots (default: .)")
    args = parser.parse_args()

    os.makedirs(args.output_dir, exist_ok=True)

    if args.gantt:
        out_gantt = os.path.join(args.output_dir, "gantt_chart.png")
        plot_gantt_chart(args.gantt, out_gantt)

    if args.comparison:
        out_comp = os.path.join(args.output_dir, "algorithm_comparison.png")
        plot_comparison_metrics(args.comparison, out_comp)

    if not args.gantt and not args.comparison:
        print("Usage examples:")
        print("  python scripts/plot_results.py --gantt gantt.csv")
        print("  python scripts/plot_results.py --comparison results.csv")

if __name__ == "__main__":
    main()
