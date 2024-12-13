import csv
import os
from pathlib import Path
import subprocess
import datetime
import re
import matplotlib.pyplot as plt
import scienceplots

ceph_path = Path(os.path.dirname(__file__)) / '..'
plt.style.use(['science', 'grid', 'no-latex'])


def rados_bench():
    script_path = ceph_path / 'bench/bench.sh'
    log_dir = ceph_path / 'bench/log'
    os.makedirs(log_dir, exist_ok=True)
    current_date = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')

    bthread_values = [1, 2, 4, 8, 16, 32, 64]
    # bsize_values = [64, 512, 1024, 4096, 65536, 1048576, 4194304]
    bsize_values = [64, 512, 1024]
    results = {}
    for bsize in bsize_values:
        logfile = os.path.join(
            log_dir, f'radosbench-{current_date}-[{bsize}].log')
        resultfile = os.path.join(
            log_dir, f'radosbench-{current_date}-[{bsize}].csv')
        print(f'Running bench.sh with BSIZE={bsize}')

        env = os.environ.copy()
        env['BSIZE'] = str(bsize)

        with open(logfile, 'w') as log_file:
            subprocess.run(['bash', script_path], stdout=log_file,
                           stderr=subprocess.STDOUT, env=env)

        with open(logfile, 'r') as log_file:
            log_content = log_file.read()
            bandwidth_match = re.search(
                r'Bandwidth \(MB/sec\):\s+([\d\.]+)', log_content)
            iops_match = re.search(r'Average IOPS:\s+([\d\.]+)', log_content)
            if bandwidth_match and iops_match:
                bandwidth = float(bandwidth_match.group(1))
                iops = float(iops_match.group(1))
                results[bsize] = {'bandwidth': bandwidth, 'iops': iops}
            else:
                print(f'Failed to parse results for BSIZE={bsize}')

        with open(resultfile, 'w', newline='') as csvfile:
            csv_writer = csv.writer(csvfile)
            csv_writer.writerow(['bsize', 'bandwidth', 'iops'])
            for bsize, result in results.items():
                csv_writer.writerow(
                    [bsize, result['bandwidth'], result['iops']])

    sorted_bsize = sorted(results.keys())
    bandwidths = [results[b]['bandwidth'] for b in sorted_bsize]
    iops_values = [results[b]['iops'] for b in sorted_bsize]
    print('Results:')
    for bsize, result in results.items():
        print(
            f'BSIZE={bsize}: Bandwidth={result["bandwidth"]}, IOPS={result["iops"]}')

    fig_dir = ceph_path / 'bench/fig'
    os.makedirs(fig_dir, exist_ok=True)
    plot_filename = os.path.join(fig_dir, f'radosbench-{current_date}.png')
    
    fig, axs = plt.subplots(1, 2, figsize=(12, 5))
    # Plot Bandwidth vs BSIZE
    axs[0].plot(sorted_bsize, bandwidths, marker='o')
    axs[0].set_title('Bandwidth vs BSIZE')
    axs[0].set_xlabel('BSIZE (Bytes)')
    axs[0].set_ylabel('Bandwidth (MB/sec)')
    axs[0].set_xscale('log')
    axs[0].grid(True)
    # Plot IOPS vs BSIZE
    axs[1].plot(sorted_bsize, iops_values, marker='o')
    axs[1].set_title('IOPS vs BSIZE')
    axs[1].set_xlabel('BSIZE (Bytes)')
    axs[1].set_ylabel('Average IOPS')
    axs[1].set_xscale('log')
    axs[1].grid(True)
    plt.tight_layout()
    plt.savefig(plot_filename)
    plt.close()
    print(f'Plot saved to {plot_filename}')


if __name__ == '__main__':
    rados_bench()
