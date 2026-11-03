#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import os
import re

def parse_benchmark_file(file_path):
    """Parse benchmark file to extract test configurations and results."""
    if not os.path.exists(file_path):
        print(f"Warning: Benchmark file {file_path} not found.")
        return None
    
    with open(file_path, 'r') as f:
        content = f.read()
    
    # Extract GPU name if available
    gpu_match = re.search(r'Running benchmark on (.*)', content)
    gpu_name = gpu_match.group(1) if gpu_match else "A100"
    print(f"Parsing benchmark data for {gpu_name}")
    
    # Dictionary to store all results
    results = {
        'v2': {'configs': [], 'min': [], 'max': [], 'mean': [], 'median': [], 'stddev': [], 'pushes_per_sec': []},
        'v3': {'configs': [], 'min': [], 'max': [], 'mean': [], 'median': [], 'stddev': [], 'pushes_per_sec': []}
    }
    
    # Extract test configurations and results
    v2_pattern = r'Testing autobatch v2 with N_THREADS=(\d+), N_REQUESTS=(\d+), N_BUFFERS=(\d+), N_PUSHES\(per thread\)=(\d+)(?:, UseCg=(\d))?\.\.\.[\s\S]*?Min: (\d+), Max: (\d+), Mean: ([0-9.e+-]+), Median: (\d+), Stddev: ([0-9.e+-]+)[\s\S]*?System time taken: ([0-9.]+) seconds'
    v3_pattern = r'Testing autobatch v3 with N_THREADS=(\d+), N_REQUESTS=(\d+), N_BUFFERS=(\d+), N_PUSHES\(per thread\)=(\d+)(?:, UseCg=(\d))?\.\.\.[\s\S]*?Min: (\d+), Max: (\d+), Mean: ([0-9.e+-]+), Median: (\d+), Stddev: ([0-9.e+-]+)[\s\S]*?System time taken: ([0-9.]+) seconds'
    
    v2_matches = re.finditer(v2_pattern, content)
    v3_matches = re.finditer(v3_pattern, content)
    
    # Process v2 results
    for match in v2_matches:
        groups = match.groups()
        # Check if UseCg is included in the pattern
        if groups[4] is not None:
            threads, requests, buffers, pushes, use_cg, min_val, max_val, mean, median, stddev, sys_time = groups
        else:
            threads, requests, buffers, pushes, use_cg, min_val, max_val, mean, median, stddev, sys_time = groups[0], groups[1], groups[2], groups[3], "0", groups[5], groups[6], groups[7], groups[8], groups[9], groups[10]
        
        threads = int(threads)
        pushes = int(pushes)
        sys_time = float(sys_time)
        
        # Calculate pushes per second based on thread count, pushes per thread, and system time
        total_pushes = threads * pushes
        pushes_per_sec = total_pushes / sys_time if sys_time > 0 else 0
        
        config = {
            'threads': threads,
            'requests': int(requests),
            'buffers': int(buffers),
            'pushes': pushes,
            'use_cg': int(use_cg) if use_cg else 0
        }
        
        results['v2']['configs'].append(config)
        results['v2']['min'].append(int(min_val))
        results['v2']['max'].append(int(max_val))
        results['v2']['mean'].append(float(mean))
        results['v2']['median'].append(int(median))
        results['v2']['stddev'].append(float(stddev))
        results['v2']['pushes_per_sec'].append(pushes_per_sec)
    
    # Process v3 results
    for match in re.finditer(v3_pattern, content):
        groups = match.groups()
        # Check if UseCg is included in the pattern
        if groups[4] is not None:
            threads, requests, buffers, pushes, use_cg, min_val, max_val, mean, median, stddev, sys_time = groups
        else:
            threads, requests, buffers, pushes, use_cg, min_val, max_val, mean, median, stddev, sys_time = groups[0], groups[1], groups[2], groups[3], "0", groups[5], groups[6], groups[7], groups[8], groups[9], groups[10]
        
        threads = int(threads)
        pushes = int(pushes)
        sys_time = float(sys_time)
        
        # Calculate pushes per second based on thread count, pushes per thread, and system time
        total_pushes = threads * pushes
        pushes_per_sec = total_pushes / sys_time if sys_time > 0 else 0
        
        config = {
            'threads': threads,
            'requests': int(requests),
            'buffers': int(buffers),
            'pushes': pushes,
            'use_cg': int(use_cg) if use_cg else 0
        }
        
        results['v3']['configs'].append(config)
        results['v3']['min'].append(int(min_val))
        results['v3']['max'].append(int(max_val))
        results['v3']['mean'].append(float(mean))
        results['v3']['median'].append(int(median))
        results['v3']['stddev'].append(float(stddev))
        results['v3']['pushes_per_sec'].append(pushes_per_sec)
    
    # Find matching test configurations for v2 and v3
    matched_configs = []
    matched_indices_v2 = []
    matched_indices_v3 = []
    
    for i, config_v2 in enumerate(results['v2']['configs']):
        for j, config_v3 in enumerate(results['v3']['configs']):
            if (config_v2['threads'] == config_v3['threads'] and
                config_v2['requests'] == config_v3['requests'] and
                config_v2['buffers'] == config_v3['buffers'] and
                config_v2['pushes'] == config_v3['pushes'] and
                config_v2['use_cg'] == config_v3['use_cg']):
                
                matched_configs.append(config_v2)
                matched_indices_v2.append(i)
                matched_indices_v3.append(j)
                break
    
    # Create final results with only matched configurations
    final_results = {
        'configs': matched_configs,
        'v2': {
            'min': [results['v2']['min'][i] for i in matched_indices_v2],
            'max': [results['v2']['max'][i] for i in matched_indices_v2],
            'mean': [results['v2']['mean'][i] for i in matched_indices_v2],
            'median': [results['v2']['median'][i] for i in matched_indices_v2],
            'stddev': [results['v2']['stddev'][i] for i in matched_indices_v2],
            'pushes_per_sec': [results['v2']['pushes_per_sec'][i] for i in matched_indices_v2]
        },
        'v3': {
            'min': [results['v3']['min'][i] for i in matched_indices_v3],
            'max': [results['v3']['max'][i] for i in matched_indices_v3],
            'mean': [results['v3']['mean'][i] for i in matched_indices_v3],
            'median': [results['v3']['median'][i] for i in matched_indices_v3],
            'stddev': [results['v3']['stddev'][i] for i in matched_indices_v3],
            'pushes_per_sec': [results['v3']['pushes_per_sec'][i] for i in matched_indices_v3]
        }
    }
    
    print(f"Found {len(matched_configs)} matched test configurations for v2 and v3")
    
    return final_results, gpu_name

# Function to create nice formatted labels with scientific notation for large numbers
def format_labels(configs):
    labels = []
    for cfg in configs:
        t = cfg['threads']
        r = cfg['requests']
        b = cfg['buffers']
        p = cfg['pushes']
        cg = cfg['use_cg']
        
        if t >= 1000000:
            t_str = f"{t/1000000:.0f}M"
        elif t >= 1000:
            t_str = f"{t/1000:.0f}K"
        else:
            t_str = str(t)
        
        # Add pushes to label only if it's not 1
        if p > 1:
            if cg == 1:
                labels.append(f"T={t_str}, R={r}, B={b}, P={p}, CG=Y")
            else:
                labels.append(f"T={t_str}, R={r}, B={b}, P={p}")
        else:
            if cg == 1:
                labels.append(f"T={t_str}, R={r}, B={b}, CG=Y")
            else:
                labels.append(f"T={t_str}, R={r}, B={b}")
            
    return labels

def main():
    # Parse benchmark data
    benchmark_file = "perf-times"  # Updated to use perf-times
    results, gpu_name = parse_benchmark_file(benchmark_file)
    
    if not results:
        print(f"Error: Could not parse benchmark data from {benchmark_file}")
        return
    
    # Extract data from results
    test_configs = results['configs']
    v2_min = results['v2']['min']
    v2_max = results['v2']['max']
    v2_mean = results['v2']['mean']
    v2_median = results['v2']['median']
    v2_stddev = results['v2']['stddev']
    v2_pushes_per_sec = results['v2']['pushes_per_sec']
    v3_min = results['v3']['min']
    v3_max = results['v3']['max']
    v3_mean = results['v3']['mean']
    v3_median = results['v3']['median']
    v3_stddev = results['v3']['stddev']
    v3_pushes_per_sec = results['v3']['pushes_per_sec']
    
    formatted_labels = format_labels(test_configs)

    # Set up the figure with a 3x3 grid of subplots
    plt.figure(figsize=(22, 18))
    plt.style.use('ggplot')

    # 1. Mean Latency (log scale)
    plt.subplot(3, 3, 1)
    x = np.arange(len(formatted_labels))
    width = 0.35

    plt.bar(x - width/2, v2_mean, width, label='v2', color='#ff7f0e', alpha=0.8)
    plt.bar(x + width/2, v3_mean, width, label='v3', color='#1f77b4', alpha=0.8)

    plt.xlabel('Test Configuration', fontsize=12)
    plt.ylabel('Mean Latency (lower is better)', fontsize=12)
    plt.title('Mean Latency Comparison (Log Scale)', fontsize=14, fontweight='bold')
    plt.xticks(x, formatted_labels, rotation=45, ha='right')
    plt.yscale('log')
    plt.legend()
    plt.grid(True, alpha=0.3)

    # Add improvement percentage labels
    for i in range(len(v2_mean)):
        improvement = (1 - v3_mean[i]/v2_mean[i]) * 100
        if improvement > 0:
            plt.text(i, max(v2_mean[i], v3_mean[i]) * 1.1, 
                     f"{improvement:.1f}% better", ha='center', fontsize=9, 
                     color='green', fontweight='bold')
        else:
            plt.text(i, max(v2_mean[i], v3_mean[i]) * 1.1, 
                     f"{-improvement:.1f}% worse", ha='center', fontsize=9, 
                     color='red', fontweight='bold')

    # 2. Median Latency (log scale)
    plt.subplot(3, 3, 2)
    plt.bar(x - width/2, v2_median, width, label='v2', color='#ff7f0e', alpha=0.8)
    plt.bar(x + width/2, v3_median, width, label='v3', color='#1f77b4', alpha=0.8)

    plt.xlabel('Test Configuration', fontsize=12)
    plt.ylabel('Median Latency (lower is better)', fontsize=12)
    plt.title('Median Latency Comparison (Log Scale)', fontsize=14, fontweight='bold')
    plt.xticks(x, formatted_labels, rotation=45, ha='right')
    plt.yscale('log')
    plt.legend()
    plt.grid(True, alpha=0.3)

    # Add improvement percentage labels
    for i in range(len(v2_median)):
        improvement = (1 - v3_median[i]/v2_median[i]) * 100
        if improvement > 0:
            plt.text(i, max(v2_median[i], v3_median[i]) * 1.1, 
                     f"{improvement:.1f}% better", ha='center', fontsize=9, 
                     color='green', fontweight='bold')
        else:
            plt.text(i, max(v2_median[i], v3_median[i]) * 1.1, 
                     f"{-improvement:.1f}% worse", ha='center', fontsize=9, 
                     color='red', fontweight='bold')

    # 3. Min Latency (log scale)
    plt.subplot(3, 3, 3)
    plt.bar(x - width/2, v2_min, width, label='v2', color='#ff7f0e', alpha=0.8)
    plt.bar(x + width/2, v3_min, width, label='v3', color='#1f77b4', alpha=0.8)

    plt.xlabel('Test Configuration', fontsize=12)
    plt.ylabel('Min Latency (lower is better)', fontsize=12)
    plt.title('Min Latency Comparison (Log Scale)', fontsize=14, fontweight='bold')
    plt.xticks(x, formatted_labels, rotation=45, ha='right')
    plt.yscale('log')
    plt.legend()
    plt.grid(True, alpha=0.3)

    # Add improvement percentage labels
    for i in range(len(v2_min)):
        improvement = (1 - v3_min[i]/v2_min[i]) * 100
        if improvement > 0:
            plt.text(i, max(v2_min[i], v3_min[i]) * 1.1, 
                     f"{improvement:.1f}% better", ha='center', fontsize=9, 
                     color='green', fontweight='bold')
        else:
            plt.text(i, max(v2_min[i], v3_min[i]) * 1.1, 
                     f"{-improvement:.1f}% worse", ha='center', fontsize=9, 
                     color='red', fontweight='bold')

    # 4. Max Latency (log scale)
    plt.subplot(3, 3, 4)
    plt.bar(x - width/2, v2_max, width, label='v2', color='#ff7f0e', alpha=0.8)
    plt.bar(x + width/2, v3_max, width, label='v3', color='#1f77b4', alpha=0.8)

    plt.xlabel('Test Configuration', fontsize=12)
    plt.ylabel('Max Latency (lower is better)', fontsize=12)
    plt.title('Max Latency Comparison (Log Scale)', fontsize=14, fontweight='bold')
    plt.xticks(x, formatted_labels, rotation=45, ha='right')
    plt.yscale('log')
    plt.legend()
    plt.grid(True, alpha=0.3)

    # Add improvement percentage labels
    for i in range(len(v2_max)):
        improvement = (1 - v3_max[i]/v2_max[i]) * 100
        if improvement > 0:
            plt.text(i, max(v2_max[i], v3_max[i]) * 1.1, 
                     f"{improvement:.1f}% better", ha='center', fontsize=9, 
                     color='green', fontweight='bold')
        else:
            plt.text(i, max(v2_max[i], v3_max[i]) * 1.1, 
                     f"{-improvement:.1f}% worse", ha='center', fontsize=9, 
                     color='red', fontweight='bold')

    # 5. Std Deviation (log scale)
    plt.subplot(3, 3, 5)
    plt.bar(x - width/2, v2_stddev, width, label='v2', color='#ff7f0e', alpha=0.8)
    plt.bar(x + width/2, v3_stddev, width, label='v3', color='#1f77b4', alpha=0.8)

    plt.xlabel('Test Configuration', fontsize=12)
    plt.ylabel('Standard Deviation (lower is better)', fontsize=12)
    plt.title('Standard Deviation Comparison (Log Scale)', fontsize=14, fontweight='bold')
    plt.xticks(x, formatted_labels, rotation=45, ha='right')
    plt.yscale('log')
    plt.legend()
    plt.grid(True, alpha=0.3)

    # Add improvement percentage labels
    for i in range(len(v2_stddev)):
        improvement = (1 - v3_stddev[i]/v2_stddev[i]) * 100
        if improvement > 0:
            plt.text(i, max(v2_stddev[i], v3_stddev[i]) * 1.1, 
                     f"{improvement:.1f}% better", ha='center', fontsize=9, 
                     color='green', fontweight='bold')
        else:
            plt.text(i, max(v2_stddev[i], v3_stddev[i]) * 1.1, 
                     f"{-improvement:.1f}% worse", ha='center', fontsize=9, 
                     color='red', fontweight='bold')

    # 6. Pushes per second (log scale)
    plt.subplot(3, 3, 6)
    plt.bar(x - width/2, v2_pushes_per_sec, width, label='v2', color='#ff7f0e', alpha=0.8)
    plt.bar(x + width/2, v3_pushes_per_sec, width, label='v3', color='#1f77b4', alpha=0.8)

    plt.xlabel('Test Configuration', fontsize=12)
    plt.ylabel('Pushes per Second (higher is better)', fontsize=12)
    plt.title('Throughput Comparison (Log Scale)', fontsize=14, fontweight='bold')
    plt.xticks(x, formatted_labels, rotation=45, ha='right')
    plt.yscale('log')
    plt.legend()
    plt.grid(True, alpha=0.3)

    # Add improvement percentage labels
    for i in range(len(v2_pushes_per_sec)):
        improvement = (v3_pushes_per_sec[i]/v2_pushes_per_sec[i] - 1) * 100
        if improvement > 0:
            plt.text(i, max(v2_pushes_per_sec[i], v3_pushes_per_sec[i]) * 1.1, 
                     f"{improvement:.1f}% better", ha='center', fontsize=9, 
                     color='green', fontweight='bold')
        else:
            plt.text(i, max(v2_pushes_per_sec[i], v3_pushes_per_sec[i]) * 1.1, 
                     f"{-improvement:.1f}% worse", ha='center', fontsize=9, 
                     color='red', fontweight='bold')

    # 7. Speedup Ratio for Latency (v2 / v3)
    plt.subplot(3, 3, 7)
    mean_ratio = [v2 / v3 for v2, v3 in zip(v2_mean, v3_mean)]
    median_ratio = [v2 / v3 for v2, v3 in zip(v2_median, v3_median)]
    min_ratio = [v2 / v3 for v2, v3 in zip(v2_min, v3_min)]
    max_ratio = [v2 / v3 for v2, v3 in zip(v2_max, v3_max)]
    stddev_ratio = [v2 / v3 for v2, v3 in zip(v2_stddev, v3_stddev)]

    width = 0.15
    plt.bar(x - width*2, mean_ratio, width, label='Mean', color='#2ca02c', alpha=0.8)
    plt.bar(x - width, median_ratio, width, label='Median', color='#d62728', alpha=0.8)
    plt.bar(x, min_ratio, width, label='Min', color='#9467bd', alpha=0.8)
    plt.bar(x + width, max_ratio, width, label='Max', color='#8c564b', alpha=0.8)
    plt.bar(x + width*2, stddev_ratio, width, label='StdDev', color='#e377c2', alpha=0.8)

    plt.xlabel('Test Configuration', fontsize=12)
    plt.ylabel('Latency Speedup Ratio (v2/v3, higher is better)', fontsize=12)
    plt.title('v3 Latency Improvement Factor over v2', fontsize=14, fontweight='bold')
    plt.xticks(x, formatted_labels, rotation=45, ha='right')
    plt.yscale('log')
    plt.axhline(y=1, color='k', linestyle='--', alpha=0.3)
    plt.legend()
    plt.grid(True, alpha=0.3)

    # 8. Throughput Ratio for Pushes per Second (v3 / v2)
    plt.subplot(3, 3, 8)
    pushes_ratio = [v3 / v2 for v3, v2 in zip(v3_pushes_per_sec, v2_pushes_per_sec)]

    plt.bar(x, pushes_ratio, width=0.6, color='#17becf', alpha=0.8)

    plt.xlabel('Test Configuration', fontsize=12)
    plt.ylabel('Throughput Speedup Ratio (v3/v2, higher is better)', fontsize=12)
    plt.title('v3 Throughput Improvement Factor over v2', fontsize=14, fontweight='bold')
    plt.xticks(x, formatted_labels, rotation=45, ha='right')
    plt.yscale('log')
    plt.axhline(y=1, color='k', linestyle='--', alpha=0.3)
    plt.grid(True, alpha=0.3)

    # Add ratio labels
    for i in range(len(pushes_ratio)):
        plt.text(i, pushes_ratio[i] * 1.1, f"{pushes_ratio[i]:.1f}x", 
                 ha='center', fontsize=9, fontweight='bold')

    # 9. CG vs Non-CG comparison (for the last config only)
    if len(v2_mean) >= 7:
        plt.subplot(3, 3, 9)
        
        # Find the indices of the CG and non-CG tests with the same configuration
        cg_idx = None
        non_cg_idx = None
        
        for i, config in enumerate(test_configs):
            if config['pushes'] > 1:  # Only consider tests with multiple pushes
                if config['use_cg'] == 1 and cg_idx is None:
                    cg_idx = i
                elif config['use_cg'] == 0 and non_cg_idx is None:
                    non_cg_idx = i
        
        if cg_idx is not None and non_cg_idx is not None:
            # For v2
            v2_improvement = (v2_mean[non_cg_idx] - v2_mean[cg_idx]) / v2_mean[non_cg_idx] * 100
            v2_throughput_improvement = (v2_pushes_per_sec[cg_idx] / v2_pushes_per_sec[non_cg_idx] - 1) * 100
            
            # For v3
            v3_improvement = (v3_mean[non_cg_idx] - v3_mean[cg_idx]) / v3_mean[non_cg_idx] * 100
            v3_throughput_improvement = (v3_pushes_per_sec[cg_idx] / v3_pushes_per_sec[non_cg_idx] - 1) * 100
            
            metrics = ['Latency Improvement', 'Throughput Improvement']
            v2_values = [v2_improvement, v2_throughput_improvement]
            v3_values = [v3_improvement, v3_throughput_improvement]
            
            x_pos = np.arange(len(metrics))
            width = 0.35
            
            plt.bar(x_pos - width/2, v2_values, width, label='v2', color='#ff7f0e', alpha=0.8)
            plt.bar(x_pos + width/2, v3_values, width, label='v3', color='#1f77b4', alpha=0.8)
            
            plt.xlabel('Metric', fontsize=12)
            plt.ylabel('Improvement Percentage (%)', fontsize=12)
            plt.title('Cooperative Groups Impact (CG vs Non-CG)', fontsize=14, fontweight='bold')
            plt.xticks(x_pos, metrics)
            plt.legend()
            plt.grid(True, alpha=0.3)
            
            # Add percentage labels
            for i, v in enumerate(v2_values):
                plt.text(i - width/2, v + (5 if v >= 0 else -15), f"{v:.1f}%", 
                         ha='center', va='center', fontsize=9, fontweight='bold')
            for i, v in enumerate(v3_values):
                plt.text(i + width/2, v + (5 if v >= 0 else -15), f"{v:.1f}%", 
                         ha='center', va='center', fontsize=9, fontweight='bold')

    plt.tight_layout()
    plt.suptitle(f'Autobatch v2 vs v3 Performance Comparison ({gpu_name})', fontsize=16, fontweight='bold', y=1.02)

    # Create output directory if it doesn't exist
    os.makedirs('doc/docs/images', exist_ok=True)

    output_file = 'doc/docs/images/autobatch_a100_comparison.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()

    print(f"Generated A100 benchmark comparison chart at {output_file}")

if __name__ == "__main__":
    main() 