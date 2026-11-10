#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import re
import os

# Function to parse benchmark files
def parse_benchmark_file(file_path):
    with open(file_path, 'r') as f:
        content = f.read()
    
    # Extract GPU name
    gpu_match = re.search(r'Running benchmark on (.*)', content)
    gpu_name = gpu_match.group(1) if gpu_match else "Unknown GPU"
    
    # Parse benchmark data
    results = {
        'gpu': gpu_name,
        'configs': [],
        'v2_min': [],
        'v2_max': [],
        'v2_mean': [],
        'v2_median': [],
        'v2_stddev': [],
        'v2_pushes_per_sec': [],
        'v3_min': [],
        'v3_max': [],
        'v3_mean': [],
        'v3_median': [],
        'v3_stddev': [],
        'v3_pushes_per_sec': []
    }
    
    # Regular expression patterns to extract data
    config_pattern = r'Testing autobatch (v\d+) with N_THREADS=(\d+), N_REQUESTS=(\d+), N_BUFFERS=(\d+), N_PUSHES\(per thread\)=(\d+), UseCg=(\d+)'
    metrics_pattern = r'Min: (\d+), Max: (\d+), Mean: ([0-9.e+]+), Median: (\d+), Stddev: ([0-9.e+]+)'
    throughput_pattern = r'Total pushes per second: ([0-9.e+]+)'
    
    # Find all test configurations and results
    test_blocks = []
    v2_configs = []
    v3_configs = []
    
    # Extract test blocks
    matches = list(re.finditer(config_pattern, content))
    
    for i, match in enumerate(matches):
        version, threads, requests, buffers, pushes, use_cg = match.groups()
        config = {
            'version': version,
            'threads': int(threads),
            'requests': int(requests),
            'buffers': int(buffers),
            'pushes': int(pushes),
            'use_cg': int(use_cg)
        }
        
        # Extract the block of text for this test
        start_pos = match.start()
        end_pos = matches[i+1].start() if i < len(matches) - 1 else len(content)
        block = content[start_pos:end_pos]
        
        # Extract metrics from the block
        metrics_match = re.search(metrics_pattern, block)
        throughput_match = re.search(throughput_pattern, block)
        
        if metrics_match and throughput_match:
            metrics = {
                'min': int(metrics_match.group(1)),
                'max': int(metrics_match.group(2)),
                'mean': float(metrics_match.group(3)),
                'median': int(metrics_match.group(4)),
                'stddev': float(metrics_match.group(5)),
                'throughput': float(throughput_match.group(1))
            }
            
            # Store test info
            test_info = {
                'config': config,
                'metrics': metrics
            }
            
            if version == 'v2':
                v2_configs.append(test_info)
            else:
                v3_configs.append(test_info)
    
    # Match v2 and v3 tests with the same configuration
    for v2_test in v2_configs:
        v2_cfg = v2_test['config']
        
        # Look for corresponding v3 test
        for v3_test in v3_configs:
            v3_cfg = v3_test['config']
            
            # Check if configurations match
            if (v2_cfg['threads'] == v3_cfg['threads'] and 
                v2_cfg['requests'] == v3_cfg['requests'] and 
                v2_cfg['buffers'] == v3_cfg['buffers'] and 
                v2_cfg['pushes'] == v3_cfg['pushes'] and 
                v2_cfg['use_cg'] == v3_cfg['use_cg']):
                
                # Add this test pair to results
                results['configs'].append({
                    'threads': v2_cfg['threads'],
                    'requests': v2_cfg['requests'],
                    'buffers': v2_cfg['buffers'],
                    'pushes': v2_cfg['pushes'],
                    'use_cg': v2_cfg['use_cg']
                })
                
                v2_metrics = v2_test['metrics']
                v3_metrics = v3_test['metrics']
                
                results['v2_min'].append(v2_metrics['min'])
                results['v2_max'].append(v2_metrics['max'])
                results['v2_mean'].append(v2_metrics['mean'])
                results['v2_median'].append(v2_metrics['median'])
                results['v2_stddev'].append(v2_metrics['stddev'])
                results['v2_pushes_per_sec'].append(v2_metrics['throughput'])
                
                results['v3_min'].append(v3_metrics['min'])
                results['v3_max'].append(v3_metrics['max'])
                results['v3_mean'].append(v3_metrics['mean'])
                results['v3_median'].append(v3_metrics['median'])
                results['v3_stddev'].append(v3_metrics['stddev'])
                results['v3_pushes_per_sec'].append(v3_metrics['throughput'])
                
                break
    
    print(f"  Found {len(results['configs'])} matched v2/v3 test pairs")
    return results

# Function to create formatted labels
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

# Function to plot multi-GPU comparison
def plot_multi_gpu_comparison(gpu_data_list, output_file='multi_gpu_comparison.png'):
    # Filter to common test configurations across all GPUs
    common_configs = []
    
    # First, find configurations that exist in all GPU datasets
    if gpu_data_list:
        config_lists = [[(c['threads'], c['requests'], c['buffers'], c['pushes'], c['use_cg']) for c in data['configs']] for data in gpu_data_list]
        
        # Find configs in the first dataset
        for i, config_tuple in enumerate(config_lists[0]):
            is_common = True
            
            # Check if this config exists in all other datasets
            for other_configs in config_lists[1:]:
                if config_tuple not in other_configs:
                    is_common = False
                    break
            
            if is_common:
                common_configs.append(i)
    
    if not common_configs:
        print("No common test configurations found across GPUs")
        return

    # Select common configurations for each GPU
    gpu_data_filtered = []
    for gpu_data in gpu_data_list:
        filtered_data = {
            'gpu': gpu_data['gpu'],
            'configs': [gpu_data['configs'][i] for i in common_configs],
            'v2_min': [gpu_data['v2_min'][i] for i in common_configs],
            'v2_max': [gpu_data['v2_max'][i] for i in common_configs],
            'v2_mean': [gpu_data['v2_mean'][i] for i in common_configs],
            'v2_median': [gpu_data['v2_median'][i] for i in common_configs],
            'v2_stddev': [gpu_data['v2_stddev'][i] for i in common_configs],
            'v2_pushes_per_sec': [gpu_data['v2_pushes_per_sec'][i] for i in common_configs],
            'v3_min': [gpu_data['v3_min'][i] for i in common_configs],
            'v3_max': [gpu_data['v3_max'][i] for i in common_configs],
            'v3_mean': [gpu_data['v3_mean'][i] for i in common_configs],
            'v3_median': [gpu_data['v3_median'][i] for i in common_configs],
            'v3_stddev': [gpu_data['v3_stddev'][i] for i in common_configs],
            'v3_pushes_per_sec': [gpu_data['v3_pushes_per_sec'][i] for i in common_configs]
        }
        gpu_data_filtered.append(filtered_data)
    
    # Create formatted labels for test configurations
    formatted_labels = format_labels(gpu_data_filtered[0]['configs'])
    
    # Set up the figure with a 3x3 grid of subplots
    plt.figure(figsize=(22, 18))
    plt.style.use('ggplot')
    
    # Define a color palette for GPUs
    gpu_colors = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728', '#9467bd', '#8c564b']
    
    # 1. Mean Latency (log scale)
    plt.subplot(3, 3, 1)
    x = np.arange(len(formatted_labels))
    width = 0.15  # narrower width for more bars
    
    for i, gpu_data in enumerate(gpu_data_filtered):
        plt.bar(x - width*1.5 + i*width, gpu_data['v2_mean'], width, 
                label=f"{gpu_data['gpu']} (v2)", color=gpu_colors[i % len(gpu_colors)], alpha=0.7)
        plt.bar(x + width*0.5 + i*width, gpu_data['v3_mean'], width, 
                label=f"{gpu_data['gpu']} (v3)", color=gpu_colors[i % len(gpu_colors)], hatch='//', alpha=0.7)
    
    plt.xlabel('Test Configuration', fontsize=12)
    plt.ylabel('Mean Latency (lower is better)', fontsize=12)
    plt.title('Mean Latency Comparison (Log Scale)', fontsize=14, fontweight='bold')
    plt.xticks(x, formatted_labels, rotation=45, ha='right')
    plt.yscale('log')
    plt.legend(fontsize=9)
    plt.grid(True, alpha=0.3)
    
    # 2. Median Latency (log scale)
    plt.subplot(3, 3, 2)
    
    for i, gpu_data in enumerate(gpu_data_filtered):
        plt.bar(x - width*1.5 + i*width, gpu_data['v2_median'], width, 
                label=f"{gpu_data['gpu']} (v2)", color=gpu_colors[i % len(gpu_colors)], alpha=0.7)
        plt.bar(x + width*0.5 + i*width, gpu_data['v3_median'], width, 
                label=f"{gpu_data['gpu']} (v3)", color=gpu_colors[i % len(gpu_colors)], hatch='//', alpha=0.7)
    
    plt.xlabel('Test Configuration', fontsize=12)
    plt.ylabel('Median Latency (lower is better)', fontsize=12)
    plt.title('Median Latency Comparison (Log Scale)', fontsize=14, fontweight='bold')
    plt.xticks(x, formatted_labels, rotation=45, ha='right')
    plt.yscale('log')
    plt.legend(fontsize=9)
    plt.grid(True, alpha=0.3)
    
    # 3. Min Latency (log scale)
    plt.subplot(3, 3, 3)
    
    for i, gpu_data in enumerate(gpu_data_filtered):
        plt.bar(x - width*1.5 + i*width, gpu_data['v2_min'], width, 
                label=f"{gpu_data['gpu']} (v2)", color=gpu_colors[i % len(gpu_colors)], alpha=0.7)
        plt.bar(x + width*0.5 + i*width, gpu_data['v3_min'], width, 
                label=f"{gpu_data['gpu']} (v3)", color=gpu_colors[i % len(gpu_colors)], hatch='//', alpha=0.7)
    
    plt.xlabel('Test Configuration', fontsize=12)
    plt.ylabel('Min Latency (lower is better)', fontsize=12)
    plt.title('Min Latency Comparison (Log Scale)', fontsize=14, fontweight='bold')
    plt.xticks(x, formatted_labels, rotation=45, ha='right')
    plt.yscale('log')
    plt.legend(fontsize=9)
    plt.grid(True, alpha=0.3)
    
    # 4. Max Latency (log scale)
    plt.subplot(3, 3, 4)
    
    for i, gpu_data in enumerate(gpu_data_filtered):
        plt.bar(x - width*1.5 + i*width, gpu_data['v2_max'], width, 
                label=f"{gpu_data['gpu']} (v2)", color=gpu_colors[i % len(gpu_colors)], alpha=0.7)
        plt.bar(x + width*0.5 + i*width, gpu_data['v3_max'], width, 
                label=f"{gpu_data['gpu']} (v3)", color=gpu_colors[i % len(gpu_colors)], hatch='//', alpha=0.7)
    
    plt.xlabel('Test Configuration', fontsize=12)
    plt.ylabel('Max Latency (lower is better)', fontsize=12)
    plt.title('Max Latency Comparison (Log Scale)', fontsize=14, fontweight='bold')
    plt.xticks(x, formatted_labels, rotation=45, ha='right')
    plt.yscale('log')
    plt.legend(fontsize=9)
    plt.grid(True, alpha=0.3)
    
    # 5. Std Deviation (log scale)
    plt.subplot(3, 3, 5)
    
    for i, gpu_data in enumerate(gpu_data_filtered):
        plt.bar(x - width*1.5 + i*width, gpu_data['v2_stddev'], width, 
                label=f"{gpu_data['gpu']} (v2)", color=gpu_colors[i % len(gpu_colors)], alpha=0.7)
        plt.bar(x + width*0.5 + i*width, gpu_data['v3_stddev'], width, 
                label=f"{gpu_data['gpu']} (v3)", color=gpu_colors[i % len(gpu_colors)], hatch='//', alpha=0.7)
    
    plt.xlabel('Test Configuration', fontsize=12)
    plt.ylabel('Standard Deviation (lower is better)', fontsize=12)
    plt.title('Standard Deviation Comparison (Log Scale)', fontsize=14, fontweight='bold')
    plt.xticks(x, formatted_labels, rotation=45, ha='right')
    plt.yscale('log')
    plt.legend(fontsize=9)
    plt.grid(True, alpha=0.3)
    
    # 6. Pushes per second (log scale)
    plt.subplot(3, 3, 6)
    
    for i, gpu_data in enumerate(gpu_data_filtered):
        plt.bar(x - width*1.5 + i*width, gpu_data['v2_pushes_per_sec'], width, 
                label=f"{gpu_data['gpu']} (v2)", color=gpu_colors[i % len(gpu_colors)], alpha=0.7)
        plt.bar(x + width*0.5 + i*width, gpu_data['v3_pushes_per_sec'], width, 
                label=f"{gpu_data['gpu']} (v3)", color=gpu_colors[i % len(gpu_colors)], hatch='//', alpha=0.7)
    
    plt.xlabel('Test Configuration', fontsize=12)
    plt.ylabel('Pushes per Second (higher is better)', fontsize=12)
    plt.title('Throughput Comparison (Log Scale)', fontsize=14, fontweight='bold')
    plt.xticks(x, formatted_labels, rotation=45, ha='right')
    plt.yscale('log')
    plt.legend(fontsize=9)
    plt.grid(True, alpha=0.3)
    
    # 7. Speedup Ratio for Latency (v2 / v3)
    plt.subplot(3, 3, 7)
    
    markers = ['o', 's', '^', 'D', 'v', '<']
    
    for i, gpu_data in enumerate(gpu_data_filtered):
        mean_ratio = [v2 / v3 for v2, v3 in zip(gpu_data['v2_mean'], gpu_data['v3_mean'])]
        median_ratio = [v2 / v3 for v2, v3 in zip(gpu_data['v2_median'], gpu_data['v3_median'])]
        min_ratio = [v2 / v3 for v2, v3 in zip(gpu_data['v2_min'], gpu_data['v3_min'])]
        max_ratio = [v2 / v3 for v2, v3 in zip(gpu_data['v2_max'], gpu_data['v3_max'])]
        stddev_ratio = [v2 / v3 for v2, v3 in zip(gpu_data['v2_stddev'], gpu_data['v3_stddev'])]
        
        plt.plot(x, mean_ratio, marker=markers[i % len(markers)], linestyle='-', 
                 label=f"{gpu_data['gpu']} Mean", color=gpu_colors[i % len(gpu_colors)])
    
    plt.xlabel('Test Configuration', fontsize=12)
    plt.ylabel('Latency Improvement Factor (v2/v3)', fontsize=12)
    plt.title('Mean Latency Improvement Factor (higher is better)', fontsize=14, fontweight='bold')
    plt.xticks(x, formatted_labels, rotation=45, ha='right')
    plt.yscale('log')
    plt.axhline(y=1, color='k', linestyle='--', alpha=0.3)
    plt.legend(fontsize=9)
    plt.grid(True, alpha=0.3)
    
    # 8. Throughput Ratio for Pushes per Second (v3 / v2)
    plt.subplot(3, 3, 8)
    
    for i, gpu_data in enumerate(gpu_data_filtered):
        pushes_ratio = [v3 / v2 for v3, v2 in zip(gpu_data['v3_pushes_per_sec'], gpu_data['v2_pushes_per_sec'])]
        
        plt.plot(x, pushes_ratio, marker=markers[i % len(markers)], linestyle='-', 
                 label=f"{gpu_data['gpu']}", color=gpu_colors[i % len(gpu_colors)])
        
        # Add ratio labels for the first GPU only to avoid cluttering
        if i == 0:
            for j, ratio in enumerate(pushes_ratio):
                plt.text(x[j], ratio * 1.1, f"{ratio:.1f}x", 
                         ha='center', fontsize=9, fontweight='bold', color=gpu_colors[i % len(gpu_colors)])
    
    plt.xlabel('Test Configuration', fontsize=12)
    plt.ylabel('Throughput Speedup Ratio (v3/v2)', fontsize=12)
    plt.title('Throughput Improvement Factor (higher is better)', fontsize=14, fontweight='bold')
    plt.xticks(x, formatted_labels, rotation=45, ha='right')
    plt.yscale('log')
    plt.axhline(y=1, color='k', linestyle='--', alpha=0.3)
    plt.grid(True, alpha=0.3)
    plt.legend(fontsize=9)
    
    # 9. Summary - Version comparison across GPUs for a specific config
    plt.subplot(3, 3, 9)
    
    # Choose a specific configuration for comparison (e.g., the one with the highest v3/v2 ratio)
    best_config_idx = 0
    max_ratio = 0
    for i in range(len(formatted_labels)):
        avg_ratio = np.mean([gpu_data['v3_pushes_per_sec'][i] / gpu_data['v2_pushes_per_sec'][i] 
                             for gpu_data in gpu_data_filtered])
        if avg_ratio > max_ratio:
            max_ratio = avg_ratio
            best_config_idx = i
    
    # Bar positions
    n_gpus = len(gpu_data_filtered)
    summary_x = np.arange(n_gpus)
    summary_width = 0.35
    
    v2_throughputs = [gpu_data['v2_pushes_per_sec'][best_config_idx] for gpu_data in gpu_data_filtered]
    v3_throughputs = [gpu_data['v3_pushes_per_sec'][best_config_idx] for gpu_data in gpu_data_filtered]
    gpu_labels = [gpu_data['gpu'] for gpu_data in gpu_data_filtered]
    
    plt.bar(summary_x - summary_width/2, v2_throughputs, summary_width, label='v2', color='#ff7f0e', alpha=0.7)
    plt.bar(summary_x + summary_width/2, v3_throughputs, summary_width, label='v3', color='#1f77b4', alpha=0.7)
    
    plt.xlabel('GPU', fontsize=12)
    plt.ylabel('Pushes per Second', fontsize=12)
    plt.title(f'GPU Comparison for {formatted_labels[best_config_idx]}', fontsize=14, fontweight='bold')
    plt.xticks(summary_x, gpu_labels, rotation=45, ha='right')
    plt.yscale('log')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    # Add speedup ratio labels
    for i, (v2, v3) in enumerate(zip(v2_throughputs, v3_throughputs)):
        ratio = v3 / v2
        plt.text(i, v3 * 1.1, f"{ratio:.1f}x", ha='center', fontsize=10, fontweight='bold')
    
    plt.tight_layout()
    plt.suptitle('Autobatch v2 vs v3 Performance Comparison Across GPUs', fontsize=16, fontweight='bold', y=1.02)
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    
    print(f"Saved multi-GPU comparison chart to {output_file}")
    
    return formatted_labels

def main():
    # Process GPU benchmark files
    benchmark_files = []
    directory = 'perf/tmp/'
    
    for filename in os.listdir(directory):
        if filename.startswith('bench.out-'):
            benchmark_files.append(os.path.join(directory, filename))
    
    if not benchmark_files:
        print("No benchmark files found!")
        return
    
    # Parse each file
    gpu_data_list = []
    for file_path in benchmark_files:
        print(f"Processing {file_path}...")
        gpu_data = parse_benchmark_file(file_path)
        gpu_data_list.append(gpu_data)
        print(f"  Found GPU: {gpu_data['gpu']} with {len(gpu_data['configs'])} test configurations.")
    
    # Create output directory if it doesn't exist
    os.makedirs('doc/docs/images', exist_ok=True)
    
    # Generate multi-GPU comparison chart
    formatted_labels = plot_multi_gpu_comparison(gpu_data_list, 'doc/docs/images/autobatch_multi_gpu_comparison.png')
    
    print("All plots generated successfully!")

if __name__ == "__main__":
    main() 