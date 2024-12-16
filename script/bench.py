import csv
import os
from pathlib import Path
import subprocess
import datetime
import re
# import matplotlib.pyplot as plt
# import scienceplots
import plotly.graph_objects as go
from plotly.subplots import make_subplots
import numpy as np
from tqdm import tqdm

ceph_path = Path(os.path.dirname(__file__)) / '..'
# plt.style.use(['science', 'grid', 'no-latex'])

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

def rados_bench():
    bsize_values = [64, 512, 1024, 4096, 65536, 1048576, 4194304]
    bsize_values = [64, 512, 1024]
    configs = range(1, 3)
    runs_per_config = 5
    
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
        for bsize in bsize_values:
            print(f"\nBSIZE: {bsize}")
            for run in tqdm(range(runs_per_config)):
                result = run_single_benchmark(bsize, config, run)
                if result:
                    results[config][bsize]['bandwidth'].append(result['bandwidth'])
                    results[config][bsize]['iops'].append(result['iops'])
    
    # Create plots using Plotly
    fig_dir = ceph_path / 'bench/fig'
    os.makedirs(fig_dir, exist_ok=True)
    current_date = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    
    # Create subplot with bandwidth and IOPS
    fig = make_subplots(rows=2, cols=1,
                       subplot_titles=('Bandwidth Distribution by Configuration and Block Size',
                                     'IOPS Distribution by Configuration and Block Size'))
    
    colors = ['rgb(31, 119, 180)', 'rgb(255, 127, 14)', 
             'rgb(44, 160, 44)', 'rgb(214, 39, 40)', 'rgb(148, 103, 189)']
    
    # Add bandwidth traces
    for idx, config in enumerate(configs):
        for bsize_idx, bsize in enumerate(bsize_values):
            fig.add_trace(
                go.Box(
                    y=results[config][bsize]['bandwidth'],
                    name=f'{CONFIG_NAMES[config]}',
                    legendgroup=f'config_{config}',
                    showlegend=bsize_idx == 0,
                    marker_color=colors[idx],
                    x=[bsize] * len(results[config][bsize]['bandwidth']),
                    boxpoints='all',
                    jitter=0.3,
                    pointpos=-1.8
                ),
                row=1, col=1
            )
    
    # Add IOPS traces
    for idx, config in enumerate(configs):
        for bsize_idx, bsize in enumerate(bsize_values):
            fig.add_trace(
                go.Box(
                    y=results[config][bsize]['iops'],
                    name=f'{CONFIG_NAMES[config]}',
                    legendgroup=f'config_{config}',
                    showlegend=False,
                    marker_color=colors[idx],
                    x=[bsize] * len(results[config][bsize]['iops']),
                    boxpoints='all',
                    jitter=0.3,
                    pointpos=-1.8
                ),
                row=2, col=1
            )
    
    # Update layout
    fig.update_layout(
        height=1000,
        width=1200,
        title_text="Ceph Benchmark Results",
        showlegend=True,
        template="plotly_white",
        boxmode='group'
    )
    
    # Update x and y axes
    fig.update_xaxes(type="log", title_text="Block Size (Bytes)", row=1, col=1)
    fig.update_xaxes(type="log", title_text="Block Size (Bytes)", row=2, col=1)
    fig.update_yaxes(title_text="Bandwidth (MB/sec)", row=1, col=1)
    fig.update_yaxes(title_text="IOPS", row=2, col=1)
    
    # Save plots
    fig.write_html(os.path.join(fig_dir, f'benchmark-results-{current_date}.html'))
    fig.write_image(os.path.join(fig_dir, f'benchmark-results-{current_date}.png'))
    
    # Save raw results
    with open(os.path.join(fig_dir, f'results-{current_date}.csv'), 'w', newline='') as f:
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

if __name__ == '__main__':
    rados_bench()
