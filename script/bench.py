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
    1: "rocks",
    2: "procks",
    3: "rocks+pmem",
    4: "procks+pmem",
    5: "kstore",
}

def run_single_benchmark(bsize, config, run_number):
    script_path = ceph_path / 'bench/bench.sh'
    reset_path = ceph_path / 'script/reset.sh'
    
    # Reset the system with the current configuration
    subprocess.run(['bash', str(reset_path), '1', str(config)], 
                  stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    
    # Run the benchmark
    env = os.environ.copy()
    env['BSIZE'] = str(bsize)
    
    result = subprocess.run(['bash', str(script_path)], 
                          capture_output=True, text=True, env=env)
    
    # Parse results
    bandwidth_match = re.search(r'Bandwidth \(MB/sec\):\s+([\d\.]+)', result.stdout)
    iops_match = re.search(r'Average IOPS:\s+([\d\.]+)', result.stdout)
    
    if bandwidth_match and iops_match:
        return {
            'bandwidth': float(bandwidth_match.group(1)),
            'iops': float(iops_match.group(1))
        }
    return None

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

    # Create figure with two subplots
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(7, 8))
    fig.subplots_adjust(hspace=0.3)

    # Systems paper color palette
    configs = df['config'].unique()
    n_configs = len(configs)
    colors = ['#4e79a7', '#f28e2b', '#e15759', '#76b7b2', '#59a14f', '#edc948'][:n_configs]

    def plot_bar_box(data, x, y, hue, ax, colors):
        # Draw bars with reduced width
        width = 0.35  # Define width here to use consistently
        bars = sns.barplot(data=data, x=x, y=y, hue=hue, ax=ax, 
                          alpha=0.5, width=width, palette=colors)
        
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
                group_data = data[(data[x] == bsize) & (data[hue] == config)][y]
                if not group_data.empty:
                    position = bar_positions[j, i]
                    # Scale the box plot data to fit within the bar height
                    bar_height = bar_heights[j, i]
                    scaled_data = group_data * (bar_height / group_data.mean())
                    box = ax.boxplot(scaled_data,
                                   positions=[position + width/4],
                                   widths=width/2,
                                   patch_artist=True,
                                   boxprops=dict(facecolor=colors[j], alpha=0.5, 
                                               linewidth=0.8),
                                   medianprops=dict(color='black', linewidth=0.8),
                                   whiskerprops=dict(linewidth=0.8),
                                   capprops=dict(linewidth=0.8),
                                   showfliers=True,
                                   flierprops=dict(marker='o', markersize=3, 
                                                 markerfacecolor='none', 
                                                 markeredgewidth=0.8))
        
        # Set x-axis ticks and labels using the actual data values
        ax.set_xticks(range(len(data[x].unique())))
        ax.set_xticklabels(data[x].unique())
        
        return bars

    # Bandwidth plot
    bars1 = plot_bar_box(df, 'bsize', 'bandwidth', 'config', ax1, colors)
    ax1.set_title('(a) Bandwidth Distribution', pad=10)
    ax1.set_xlabel('Block Size (Bytes)')
    ax1.set_ylabel('Bandwidth (MB/s)')
    ax1.legend(title='Configuration', bbox_to_anchor=(1.02, 1), loc='upper left')
    ax1.grid(True, linestyle='--', alpha=0.3)

    # IOPS plot
    bars2 = plot_bar_box(df, 'bsize', 'iops', 'config', ax2, colors)
    ax2.set_title('(b) IOPS Distribution', pad=10)
    ax2.set_xlabel('Block Size (Bytes)')
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

def rados_bench():
    bsize_values = [64, 512, 1024, 4096, 65536, 1048576, 4194304]
    # bsize_values = [64, 512, 1024]
    configs = range(1, 6)
    # configs = range(1, 3)
    runs_per_config = 20
    
    results = {}
    
    # Create results structure
    for config in configs:
        results[config] = {}
        for bsize in bsize_values:
            results[config][bsize] = {
                'bandwidth': [],
                'iops': []
            }
    
    # Run benchmarks
    for config in configs:
        print(f"\nRunning configuration {config} ({CONFIG_NAMES[config]})")
        for run in range(runs_per_config):
            print(f"\nRun {run + 1}/{runs_per_config}")
            # Reset once per run
            subprocess.run(['bash', str(ceph_path / 'script/reset.sh'), '1', str(config)],
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            
            # Run all block sizes
            for bsize in tqdm(bsize_values):
                env = os.environ.copy()
                env['BSIZE'] = str(bsize)
                result = subprocess.run(['bash', str(ceph_path / 'bench/bench.sh')],
                                     capture_output=True, text=True, env=env)
                
                bandwidth_match = re.search(r'Bandwidth \(MB/sec\):\s+([\d\.]+)', result.stdout)
                iops_match = re.search(r'Average IOPS:\s+([\d\.]+)', result.stdout)
                
                if bandwidth_match and iops_match:
                    results[config][bsize]['bandwidth'].append(float(bandwidth_match.group(1)))
                    results[config][bsize]['iops'].append(float(iops_match.group(1)))

    # Save raw results
    fig_dir = ceph_path / 'bench/fig'
    os.makedirs(fig_dir, exist_ok=True)
    current_date = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    
    csv_file = os.path.join(fig_dir, f'results-{current_date}.csv')
    with open(csv_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['config', 'bsize', 'run', 'bandwidth', 'iops'])
        for config in configs:
            for bsize in bsize_values:
                for run in range(runs_per_config):
                    writer.writerow([
                        CONFIG_NAMES[config],
                        bsize,
                        run,
                        results[config][bsize]['bandwidth'][run],
                        results[config][bsize]['iops'][run]
                    ])
    
    # Plot results
    plot_benchmark_results(results)

def main():
    parser = argparse.ArgumentParser(description='Run benchmarks or plot results')
    parser.add_argument('--plot', type=str, help='Plot results from CSV file', required=False)
    args = parser.parse_args()
    
    if args.plot:
        plot_benchmark_results(args.plot)
    else:
        rados_bench()

if __name__ == '__main__':
    main()
