import csv
import os
from pathlib import Path
import subprocess
import datetime
import re
import seaborn as sns
import matplotlib.pyplot as plt
import pandas as pd
from tqdm import tqdm
import numpy as np
import argparse

ceph_path = Path(os.path.dirname(__file__)) / '..'

CONFIG_NAMES = {
    1: "kstore",
    2: "rocks",
    3: "procks",
    4: "rocks+pmem",
    5: "procks+pmem",
}


def plot_benchmark_results(data, output_dir=None):
    """
    Plot benchmark results from either a DataFrame or results dictionary

    Args:
        data: Either a path to CSV file or the results dictionary from rados_bench
        output_dir: Optional directory for output files (if None, uses default bench/fig)
    """
    if isinstance(data, str):
        # Read from CSV file
        df = pd.read_csv(data)
    else:
        # Convert results dictionary to DataFrame
        df_data = []
        for config in data:
            for bsize in data[config]:
                for run in range(len(data[config][bsize]['bandwidth'])):
                    df_data.append({
                        'config': CONFIG_NAMES[config],
                        'bsize': str(bsize),
                        'bandwidth': data[config][bsize]['bandwidth'][run],
                        'iops': data[config][bsize]['iops'][run]
                    })
        df = pd.DataFrame(df_data)

    # Set the style for scientific plotting
    plt.style.use('default')
    plt.rcParams.update({
        'font.family': 'serif',
        'font.size': 10,
        'axes.labelsize': 10,
        'axes.titlesize': 10,
        'xtick.labelsize': 9,
        'ytick.labelsize': 9,
        'legend.fontsize': 9,
        'figure.dpi': 300,
        'axes.grid': True,
        'grid.alpha': 0.3,
        'axes.linewidth': 0.8,
        'axes.axisbelow': True,
    })

    # Systems paper color palette - high contrast colors
    configs = df['config'].unique()
    n_configs = len(configs)
    colors = ['#000000', '#E69F00', '#56B4E9', '#009E73', '#CC79A7'][:n_configs]  # High contrast palette

    def plot_bar_box(data, x, y, hue, ax, colors):
        # Draw bars with increased width
        width = 0.6  # Increased from 0.35
        bars = sns.barplot(data=data, x=x, y=y, hue=hue, ax=ax,
                          alpha=0.7, width=width, palette=colors)  # Increased alpha

        # Calculate positions for box plots to be exactly on top of bars
        n_groups = len(data[x].unique())
        n_hues = len(data[hue].unique())

        # Get the actual bar positions and heights
        bar_positions = []
        bar_heights = []
        for container in bars.containers:
            for patch in container:
                bar_positions.append(patch.get_x())
                bar_heights.append(patch.get_height())

        # Reshape positions and heights to match the data structure
        bar_positions = np.array(bar_positions).reshape(n_hues, -1)
        bar_heights = np.array(bar_heights).reshape(n_hues, -1)

        for i, bsize in enumerate(data[x].unique()):
            for j, config in enumerate(data[hue].unique()):
                group_data = data[(data[x] == bsize) &
                                  (data[hue] == config)][y]
                if not group_data.empty:
                    position = bar_positions[j, i]
                    # Scale the box plot data to fit within the bar height
                    bar_height = bar_heights[j, i]
                    scaled_data = group_data * (bar_height / group_data.mean())
                    # box = ax.boxplot(scaled_data,
                    #                  positions=[position + width/4],
                    #                  widths=width/2,
                    #                  patch_artist=True,
                    #                  boxprops=dict(facecolor=colors[j], alpha=0.5,
                    #                                linewidth=0.8),
                    #                  medianprops=dict(
                    #                      color='black', linewidth=0.8),
                    #                  whiskerprops=dict(linewidth=0.8),
                    #                  capprops=dict(linewidth=0.8),
                    #                  showfliers=True,
                    #                  flierprops=dict(marker='o', markersize=3,
                    #                                  markerfacecolor='none',
                    #                                  markeredgewidth=0.8))

        # Convert block size to readable format
        def format_block_size(size):
            size = int(size)
            if size >= 1048576:  # 1MB
                return f'{size//1048576}MB'
            elif size >= 1024:
                return f'{size//1024}KB'
            else:
                return f'{size}B'

        # Set x-axis ticks and labels using formatted values
        ax.set_xticks(range(len(data[x].unique())))
        ax.set_xticklabels([format_block_size(size) for size in data[x].unique()], rotation=45)

        return bars

    # Create figure with two subplots side by side with smaller margins
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))
    fig.subplots_adjust(wspace=0.2)  # Reduced from 0.3

    # Bandwidth plot
    bars1 = plot_bar_box(df, 'bsize', 'bandwidth', 'config', ax1, colors)
    ax1.set_title('(a) Write Bandwidth', pad=8)
    ax1.set_xlabel('Block Size')  # Removed "(Bytes)"
    ax1.set_ylabel('Bandwidth (MB/s)')
    ax1.grid(True, linestyle='--', alpha=0.3)
    ax1.get_legend().remove()

    # IOPS plot
    bars2 = plot_bar_box(df, 'bsize', 'iops', 'config', ax2, colors)
    ax2.set_title('(b) Write IOPS', pad=8)
    ax2.set_xlabel('Block Size')  # Removed "(Bytes)"
    ax2.set_ylabel('IOPS')
    ax2.legend(title='Configuration', bbox_to_anchor=(1.02, 1), loc='upper left')
    ax2.grid(True, linestyle='--', alpha=0.3)

    # Adjust layout
    plt.tight_layout()

    # Determine output directory
    if output_dir is None:
        if isinstance(data, str):
            output_dir = Path(data).parent
        else:
            output_dir = ceph_path / 'bench/fig'

    os.makedirs(output_dir, exist_ok=True)
    current_date = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')

    # Save plots
    plt.savefig(os.path.join(output_dir, f'benchmark-results-{current_date}.png'),
                bbox_inches='tight', dpi=300, format='png')
    plt.savefig(os.path.join(output_dir, f'benchmark-results-{current_date}.pdf'),
                bbox_inches='tight', format='pdf')


def reset_config(config, pmem_populate, cache,  log_file):
    process = subprocess.Popen(['bash', str(ceph_path / 'script/reset.sh'), '1', str(config), str(pmem_populate), str(cache)],
                               stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE,
                               universal_newlines=True)
    with open(log_file, 'a') as f:
        f.write(f"\n=== Reset Configuration {
                config} ({CONFIG_NAMES[config]}) ===\n")
        for line in process.stdout:
            f.write(line)
        for line in process.stderr:
            f.write(line)
    process.wait()


def rados_bench():
    btime = 10
    bthread = 16
    bsize_values = [64, 512, 1024, 4096, 65536, 1048576, 4194304]
    # bsize_values = [64, 512]
    configs = range(1, 6)
    # configs = range(5, 6)
    runs_per_config = 10
    # runs_per_config = 3
    pmem_populate = 1
    cache = 1
    results = {}

    # Create results structure
    for config in configs:
        results[config] = {}
        for bsize in bsize_values:
            results[config][bsize] = {
                'bandwidth': [],
                'iops': []
            }

    # Create/open CSV file at the start
    fig_dir = ceph_path / 'bench/fig'
    os.makedirs(fig_dir, exist_ok=True)
    current_date = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    csv_file = os.path.join(fig_dir, f'results-{current_date}.csv')

    # Write CSV header
    with open(csv_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['config', 'bsize', 'run', 'bandwidth', 'iops'])

    # Create log directory
    log_dir = ceph_path / 'bench/log'
    os.makedirs(log_dir, exist_ok=True)

    # Create main log file
    current_date = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    main_log = log_dir / f'bench-main-{current_date}.log'

    with open(main_log, 'w') as f:
        f.write(f"=== Benchmark Started at {datetime.datetime.now()} ===\n")
        f.write(f"Configurations:\n")
        f.write(f"- Block sizes: {bsize_values}\n")
        f.write(f"- Configs: {[CONFIG_NAMES[c] for c in configs]}\n")
        f.write(f"- Runs per config: {runs_per_config}\n\n")

    # Run benchmarks
    for config in configs:
        with open(main_log, 'a') as f:
            f.write(f"\nRunning configuration {
                    config} ({CONFIG_NAMES[config]})\n")

        print(f"\nRunning configuration {config} ({CONFIG_NAMES[config]})")
        for run in range(runs_per_config):
            with open(main_log, 'a') as f:
                f.write(f"\nRun {run + 1}/{runs_per_config}\n")

            print(f"\nRun {run + 1}/{runs_per_config}")

            # # Reset once per run - write to log but not console
            # reset_config(config, pmem_populate, cache, main_log)

            # Run all block sizes
            for bsize in tqdm(bsize_values):
                # Reset once per run - write to log but not console
                reset_config(config, pmem_populate, cache, main_log)

                env = os.environ.copy()
                env['BSIZE'] = str(bsize)
                env['BTIME'] = str(btime)
                env['BTHREAD'] = str(bthread)

                output = []  # Store output lines for parsing
                # Run benchmark and capture output while also printing
                process = subprocess.Popen(['bash', str(ceph_path / 'bench/bench.sh')],
                                           stdout=subprocess.PIPE,
                                           stderr=subprocess.PIPE,
                                           env=env,
                                           universal_newlines=True)

                # Read and write output in real-time
                with open(main_log, 'a') as log:
                    log.write(f"\nBlock size {bsize}:\n")
                    for line in process.stdout:
                        print(line, end='')  # Print to console
                        log.write(line)      # Write to log
                        output.append(line)  # Store for parsing

                    for line in process.stderr:
                        print(line, end='')  # Print to console
                        log.write(line)      # Write to log
                        output.append(line)  # Store for parsing

                process.wait()

                # Parse results from stored output
                output_text = ''.join(output)
                bandwidth_match = re.search(
                    r'Bandwidth \(MB/sec\):\s+([\d\.]+)', output_text)
                iops_match = re.search(
                    r'Average IOPS:\s+([\d\.]+)', output_text)

                if bandwidth_match and iops_match:
                    bandwidth = float(bandwidth_match.group(1))
                    iops = float(iops_match.group(1))
                    results[config][bsize]['bandwidth'].append(bandwidth)
                    results[config][bsize]['iops'].append(iops)

                    # Write result to CSV immediately
                    with open(csv_file, 'a', newline='') as f:
                        writer = csv.writer(f)
                        writer.writerow([
                            CONFIG_NAMES[config],
                            bsize,
                            run,
                            bandwidth,
                            iops
                        ])

                    # Log the result
                    with open(main_log, 'a') as f:
                        f.write(f"Block size {bsize}: Bandwidth={
                                bandwidth} MB/s, IOPS={iops}\n")

    # Plot results
    plot_benchmark_results(results)

    with open(main_log, 'a') as f:
        f.write(f"\n=== Benchmark Completed at {
                datetime.datetime.now()} ===\n")


def main():
    parser = argparse.ArgumentParser(
        description='Run benchmarks or plot results')
    parser.add_argument('--plot', type=str,
                        help='Plot results from CSV file', required=False)
    args = parser.parse_args()

    if args.plot:
        plot_benchmark_results(args.plot)
    else:
        rados_bench()


if __name__ == '__main__':
    main()
