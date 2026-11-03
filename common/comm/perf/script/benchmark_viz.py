#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import re
import os
import argparse
from typing import List, Dict, Any, Optional, Tuple, Union

# Function to parse benchmark files
def parse_benchmark_file(file_path: str) -> Tuple[pd.DataFrame, str]:
    """
    Parse benchmark file and extract data for various metrics.
    
    Args:
        file_path: Path to the benchmark output file
    
    Returns:
        Tuple of (DataFrame with benchmark results, GPU name)
    """
    if not os.path.exists(file_path):
        print(f"Warning: Benchmark file {file_path} not found.")
        return pd.DataFrame(), "Unknown"
    
    with open(file_path, 'r') as f:
        content = f.read()
    
    # Extract GPU name
    gpu_match = re.search(r'Running benchmark on (.*)', content)
    gpu_name = gpu_match.group(1) if gpu_match else "Unknown GPU"
    print(f"Parsing benchmark data for {gpu_name}")
    
    # Regular expression patterns to extract comprehensive data
    pattern = (
        r'Testing autobatch (v\d+) with '
        r'N_THREADS=(\d+), '
        r'N_REQUESTS=(\d+), '
        r'N_BUFFERS=(\d+), '
        r'BLOCK_SIZE=(\d+), '
        r'N_PUSHES\(per thread\)=(\d+)'
        r'(?:, UseCg=(\d+))?'
        r'\.\.\.[\s\S]*?'
        r'Total pushes per second: ([0-9.e+-]+)[\s\S]*?'
        r'Min: (\d+), Max: (\d+), Mean: ([0-9.e+-]+), Median: (\d+), Stddev: ([0-9.e+-]+)[\s\S]*?'
        r'System time taken: ([0-9.]+) seconds'
    )
    
    results = []
    for match in re.finditer(pattern, content):
        groups = match.groups()
        
        # Handle optional UseCg parameter which might be None
        if groups[6] is None:  # UseCg was not found in the pattern
            version, threads, requests, buffers, block_size, pushes, use_cg, throughput, min_val, max_val, mean, median, stddev, sys_time = (
                groups[0], groups[1], groups[2], groups[3], groups[4], groups[5], "0", 
                groups[7], groups[8], groups[9], groups[10], groups[11], groups[12], groups[13]
            )
        else:
            version, threads, requests, buffers, block_size, pushes, use_cg, throughput, min_val, max_val, mean, median, stddev, sys_time = groups
        
        # Convert values to appropriate types
        threads_val = int(threads)
        pushes_val = int(pushes)
        sys_time_val = float(sys_time)
        
        # Calculate pushes per second if not provided or verify the reported value
        total_pushes = threads_val * pushes_val
        computed_pushes_per_sec = total_pushes / sys_time_val if sys_time_val > 0 else 0
        reported_throughput = float(throughput)
        
        # Use reported throughput but keep computed value for verification
        results.append({
            'GPU': gpu_name,
            'Version': version,
            'Threads': threads_val,
            'Requests': int(requests),
            'Buffers': int(buffers),
            'BlockSize': int(block_size),
            'Pushes': pushes_val,
            'UseCg': int(use_cg) if use_cg else 0,
            'Throughput': reported_throughput,
            'ComputedThroughput': computed_pushes_per_sec,
            'Min': int(min_val),
            'Max': int(max_val),
            'Mean': float(mean),
            'Median': int(median),
            'Stddev': float(stddev),
            'SystemTime': sys_time_val,
            'ConfigKey': f"{threads}_{requests}_{buffers}_{pushes}_{use_cg if use_cg else 0}"
        })
    
    # Convert to DataFrame for easier manipulation
    df = pd.DataFrame(results)
    
    # Print summary
    versions = df['Version'].unique()
    print(f"Found {len(df)} benchmark results across {len(versions)} versions")
    
    return df, gpu_name

# Process all benchmark files in a directory
def process_benchmark_files(directory: str = 'perf/tmp/') -> Dict[str, pd.DataFrame]:
    """
    Process all benchmark files in a directory.
    
    Args:
        directory: Directory containing benchmark files
    
    Returns:
        Dictionary mapping GPU names to their benchmark DataFrames
    """
    gpu_data_dict = {}
    
    if not os.path.exists(directory):
        print(f"Warning: Directory {directory} not found.")
        return gpu_data_dict
    
    for filename in os.listdir(directory):
        if filename.startswith('bench.out-'):
            file_path = os.path.join(directory, filename)
            df, gpu_name = parse_benchmark_file(file_path)
            if not df.empty:
                gpu_data_dict[gpu_name] = df
    
    return gpu_data_dict

# Function to create nice formatted labels
def format_labels(configs: List[Dict[str, Any]]) -> List[str]:
    """
    Create formatted labels for test configurations.
    
    Args:
        configs: List of configuration dictionaries
    
    Returns:
        List of formatted string labels
    """
    labels = []
    for cfg in configs:
        t = cfg['Threads']
        r = cfg['Requests']
        b = cfg['Buffers']
        p = cfg['Pushes']
        cg = cfg['UseCg']
        
        # Format threads with K or M suffix for readability
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

# Single GPU visualization
def plot_benchmark_comparison(df: pd.DataFrame, gpu_name: str, output_file: str) -> None:
    """
    Create a comprehensive visualization comparing v2 and v3 for a single GPU.
    
    Args:
        df: DataFrame with benchmark results
        gpu_name: Name of the GPU
        output_file: Path to save the output image
    """
    # Filter data into v2 and v3 results
    v2_df = df[df['Version'] == 'v2'].copy()
    v3_df = df[df['Version'] == 'v3'].copy()
    
    # Find common configurations between v2 and v3
    merged_df = pd.merge(
        v2_df, v3_df, 
        on=['Threads', 'Requests', 'Buffers', 'Pushes', 'UseCg'],
        suffixes=('_v2', '_v3')
    )
    
    if merged_df.empty:
        print("Error: No matching configurations found between v2 and v3")
        return
    
    # Extract configs for labeling
    configs = []
    for _, row in merged_df.iterrows():
        configs.append({
            'Threads': row['Threads'],
            'Requests': row['Requests'],
            'Buffers': row['Buffers'],
            'Pushes': row['Pushes'],
            'UseCg': row['UseCg']
        })
    
    # Create formatted labels
    formatted_labels = format_labels(configs)
    
    # Set up the figure with a 2x3 grid of subplots (removing system time)
    plt.figure(figsize=(20, 14))
    plt.style.use('ggplot')
    plt.suptitle(f'Autobatch Performance Comparison on {gpu_name}', fontsize=18, fontweight='bold')
    
    # 1. Mean Latency (log scale)
    plt.subplot(2, 3, 1)
    x = np.arange(len(formatted_labels))
    width = 0.35
    
    plt.bar(x - width/2, merged_df['Mean_v2'], width, label='v2', color='#ff7f0e', alpha=0.8)
    plt.bar(x + width/2, merged_df['Mean_v3'], width, label='v3', color='#1f77b4', alpha=0.8)
    
    plt.ylabel('Mean Latency (lower is better)', fontsize=12)
    plt.title('Mean Latency Comparison (Log Scale)', fontsize=14, fontweight='bold')
    plt.xticks(x, formatted_labels, rotation=45, ha='right')
    plt.yscale('log')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    # Add improvement percentage labels
    for i in range(len(merged_df)):
        v2_val = merged_df.iloc[i]['Mean_v2']
        v3_val = merged_df.iloc[i]['Mean_v3']
        improvement = (1 - v3_val/v2_val) * 100
        if improvement > 0:
            plt.text(i, max(v2_val, v3_val) * 1.1, 
                    f"{improvement:.1f}% better", ha='center', fontsize=9, 
                    color='green', fontweight='bold')
        else:
            plt.text(i, max(v2_val, v3_val) * 1.1, 
                    f"{-improvement:.1f}% worse", ha='center', fontsize=9, 
                    color='red', fontweight='bold')
    
    # 2. Median Latency (log scale)
    plt.subplot(2, 3, 2)
    plt.bar(x - width/2, merged_df['Median_v2'], width, label='v2', color='#ff7f0e', alpha=0.8)
    plt.bar(x + width/2, merged_df['Median_v3'], width, label='v3', color='#1f77b4', alpha=0.8)
    
    plt.ylabel('Median Latency (lower is better)', fontsize=12)
    plt.title('Median Latency Comparison (Log Scale)', fontsize=14, fontweight='bold')
    plt.xticks(x, formatted_labels, rotation=45, ha='right')
    plt.yscale('log')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    # Add improvement percentage labels
    for i in range(len(merged_df)):
        v2_val = merged_df.iloc[i]['Median_v2']
        v3_val = merged_df.iloc[i]['Median_v3']
        improvement = (1 - v3_val/v2_val) * 100
        if improvement > 0:
            plt.text(i, max(v2_val, v3_val) * 1.1, 
                    f"{improvement:.1f}% better", ha='center', fontsize=9, 
                    color='green', fontweight='bold')
        else:
            plt.text(i, max(v2_val, v3_val) * 1.1, 
                    f"{-improvement:.1f}% worse", ha='center', fontsize=9, 
                    color='red', fontweight='bold')
    
    # 3. Min-Max Latency Range (log scale)
    plt.subplot(2, 3, 3)
    
    # v2 min-max bars
    plt.bar(x - width/2, merged_df['Max_v2'] - merged_df['Min_v2'], width, bottom=merged_df['Min_v2'],
            label='v2 range', color='#ff7f0e', alpha=0.4)
    plt.bar(x - width/2, [0] * len(merged_df), width, bottom=merged_df['Min_v2'],
            color='#ff7f0e', alpha=0.8)
    
    # v3 min-max bars
    plt.bar(x + width/2, merged_df['Max_v3'] - merged_df['Min_v3'], width, bottom=merged_df['Min_v3'],
            label='v3 range', color='#1f77b4', alpha=0.4)
    plt.bar(x + width/2, [0] * len(merged_df), width, bottom=merged_df['Min_v3'],
            color='#1f77b4', alpha=0.8)
    
    plt.ylabel('Latency Range (lower is better)', fontsize=12)
    plt.title('Min-Max Latency Range (Log Scale)', fontsize=14, fontweight='bold')
    plt.xticks(x, formatted_labels, rotation=45, ha='right')
    plt.yscale('log')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    # 4. Standard Deviation (log scale)
    plt.subplot(2, 3, 4)
    plt.bar(x - width/2, merged_df['Stddev_v2'], width, label='v2', color='#ff7f0e', alpha=0.8)
    plt.bar(x + width/2, merged_df['Stddev_v3'], width, label='v3', color='#1f77b4', alpha=0.8)
    
    plt.ylabel('Standard Deviation (lower is better)', fontsize=12)
    plt.title('Latency Standard Deviation (Log Scale)', fontsize=14, fontweight='bold')
    plt.xticks(x, formatted_labels, rotation=45, ha='right')
    plt.yscale('log')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    # Add improvement percentage labels
    for i in range(len(merged_df)):
        v2_val = merged_df.iloc[i]['Stddev_v2']
        v3_val = merged_df.iloc[i]['Stddev_v3']
        improvement = (1 - v3_val/v2_val) * 100
        if improvement > 0:
            plt.text(i, max(v2_val, v3_val) * 1.1, 
                    f"{improvement:.1f}% better", ha='center', fontsize=9, 
                    color='green', fontweight='bold')
        else:
            plt.text(i, max(v2_val, v3_val) * 1.1, 
                    f"{-improvement:.1f}% worse", ha='center', fontsize=9, 
                    color='red', fontweight='bold')
    
    # 5. Throughput (Pushes per second) (log scale)
    plt.subplot(2, 3, 5)
    plt.bar(x - width/2, merged_df['Throughput_v2'], width, label='v2', color='#ff7f0e', alpha=0.8)
    plt.bar(x + width/2, merged_df['Throughput_v3'], width, label='v3', color='#1f77b4', alpha=0.8)
    
    plt.ylabel('Pushes per Second (higher is better)', fontsize=12)
    plt.title('Throughput Comparison (Log Scale)', fontsize=14, fontweight='bold')
    plt.xticks(x, formatted_labels, rotation=45, ha='right')
    plt.yscale('log')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    # Add improvement percentage labels
    for i in range(len(merged_df)):
        v2_val = merged_df.iloc[i]['Throughput_v2']
        v3_val = merged_df.iloc[i]['Throughput_v3']
        improvement = (v3_val/v2_val - 1) * 100  # For throughput, higher is better
        if improvement > 0:
            plt.text(i, max(v2_val, v3_val) * 1.1, 
                    f"{improvement:.1f}% better", ha='center', fontsize=9, 
                    color='green', fontweight='bold')
        else:
            plt.text(i, max(v2_val, v3_val) * 1.1, 
                    f"{-improvement:.1f}% worse", ha='center', fontsize=9, 
                    color='red', fontweight='bold')
    
    # 6. Speedup Ratio (v3/v2) for Throughput
    plt.subplot(2, 3, 6)
    speedup_throughput = merged_df['Throughput_v3'] / merged_df['Throughput_v2']
    plt.bar(x, speedup_throughput, width, color='#2ca02c', alpha=0.8)
    
    plt.ylabel('Speedup Ratio (higher is better)', fontsize=12)
    plt.title('v3/v2 Speedup Ratio for Throughput (Log Scale)', fontsize=14, fontweight='bold')
    plt.xticks(x, formatted_labels, rotation=45, ha='right')
    plt.yscale('log')  # Set log scale for speedup ratio
    plt.grid(True, alpha=0.3)
    
    # Add ratio labels
    for i, ratio in enumerate(speedup_throughput):
        plt.text(i, ratio * 1.05, f"{ratio:.2f}x", ha='center', fontsize=9, fontweight='bold')
    
    # Add a common x-axis label at the bottom of the figure
    plt.figtext(0.5, 0.01, 'Parameter Configurations', ha='center', fontsize=14)
    
    plt.tight_layout(rect=[0, 0.03, 1, 0.97])
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    
    print(f"Benchmark comparison chart saved to {output_file}")
    
    # Print summary table, removing system time
    print("\nPerformance Summary:")
    summary_df = pd.DataFrame({
        'Configuration': formatted_labels,
        'Mean Latency Improvement': ((1 - merged_df['Mean_v3']/merged_df['Mean_v2']) * 100).map('{:.1f}%'.format),
        'Throughput Improvement': ((merged_df['Throughput_v3']/merged_df['Throughput_v2'] - 1) * 100).map('{:.1f}%'.format)
    })
    pd.set_option('display.max_rows', None)
    pd.set_option('display.max_columns', None)
    pd.set_option('display.width', 1000)
    print(summary_df)

# Multi-GPU visualization
def plot_multi_gpu_comparison(gpu_data_dict: Dict[str, pd.DataFrame], output_dir: str) -> None:
    """
    Create visualizations comparing performance across multiple GPUs.
    
    Args:
        gpu_data_dict: Dictionary mapping GPU names to their benchmark DataFrames
        output_dir: Directory to save output images
    """
    if len(gpu_data_dict) < 2:
        print("Error: Need at least 2 GPUs for multi-GPU comparison")
        return
    
    # Combine all data
    all_gpu_data = []
    for gpu_name, df in gpu_data_dict.items():
        df_copy = df.copy()
        df_copy['GPU'] = gpu_name
        all_gpu_data.append(df_copy)
    
    combined_df = pd.concat(all_gpu_data)
    
    # Find common configurations across all GPUs
    config_columns = ['Threads', 'Requests', 'Buffers', 'Pushes', 'UseCg', 'Version']
    config_counts = combined_df.groupby(config_columns).size().reset_index(name='count')
    common_configs = config_counts[config_counts['count'] == len(gpu_data_dict)]
    
    if common_configs.empty:
        print("Error: No common configurations across all GPUs")
        return
    
    # Filter to common configurations
    common_df = pd.merge(combined_df, common_configs[config_columns], on=config_columns)
    
    # Plot throughput comparison across GPUs for each version
    for version in ['v2', 'v3']:
        version_df = common_df[common_df['Version'] == version]
        
        # Create a unique configuration identifier
        version_df['ConfigName'] = version_df.apply(
            lambda row: f"T={format_num(row['Threads'])}, R={row['Requests']}, "
                       f"B={row['Buffers']}, P={row['Pushes']}, "
                       f"CG={'Y' if row['UseCg'] == 1 else 'N'}",
            axis=1
        )
        
        plt.figure(figsize=(14, 10))
        configs = version_df['ConfigName'].unique()
        n_configs = len(configs)
        n_gpus = len(gpu_data_dict)
        
        # Set up the plot
        fig, ax = plt.subplots(figsize=(max(14, n_configs * 1.5), 10))
        
        # Plot grouped bar chart
        width = 0.8 / n_gpus
        x = np.arange(n_configs)
        
        for i, gpu in enumerate(gpu_data_dict.keys()):
            gpu_data = version_df[version_df['GPU'] == gpu]
            throughput_values = []
            
            for config in configs:
                config_data = gpu_data[gpu_data['ConfigName'] == config]
                if not config_data.empty:
                    throughput_values.append(config_data['Throughput'].values[0])
                else:
                    throughput_values.append(0)
            
            ax.bar(x + i * width - (n_gpus-1) * width/2, throughput_values, width, label=gpu)
        
        # Customize the plot
        ax.set_yscale('log')
        ax.set_xlabel('Test Configuration', fontsize=14)
        ax.set_ylabel('Throughput (Pushes per Second)', fontsize=14)
        ax.set_title(f'Throughput Comparison Across GPUs for {version}', fontsize=16, fontweight='bold')
        ax.set_xticks(x)
        ax.set_xticklabels(configs, rotation=45, ha='right')
        ax.legend()
        ax.grid(True, alpha=0.3)
        
        plt.tight_layout()
        output_file = os.path.join(output_dir, f'throughput_comparison_{version}.png')
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        plt.close()
        
        print(f"Throughput comparison chart for {version} saved to {output_file}")
    
    # Plot v3/v2 speedup ratio comparison across GPUs
    merged_dfs = {}
    for gpu, df in gpu_data_dict.items():
        v2_df = df[df['Version'] == 'v2'].copy()
        v3_df = df[df['Version'] == 'v3'].copy()
        
        merged = pd.merge(
            v2_df, v3_df,
            on=['Threads', 'Requests', 'Buffers', 'Pushes', 'UseCg'],
            suffixes=('_v2', '_v3')
        )
        
        if not merged.empty:
            merged['ThroughputSpeedup'] = merged['Throughput_v3'] / merged['Throughput_v2']
            merged['LatencySpeedup'] = merged['Mean_v2'] / merged['Mean_v3']
            merged['SystemTimeSpeedup'] = merged['SystemTime_v2'] / merged['SystemTime_v3']
            merged['ConfigName'] = merged.apply(
                lambda row: f"T={format_num(row['Threads'])}, R={row['Requests']}, "
                          f"B={row['Buffers']}, P={row['Pushes']}, "
                          f"CG={'Y' if row['UseCg'] == 1 else 'N'}",
                axis=1
            )
            merged_dfs[gpu] = merged
    
    # Find common configurations across all GPUs for speedup comparison
    common_configs = set()
    for gpu, df in merged_dfs.items():
        if len(common_configs) == 0:
            common_configs = set(df['ConfigName'].tolist())
        else:
            common_configs &= set(df['ConfigName'].tolist())
    
    if not common_configs:
        print("Error: No common configurations for speedup comparison")
        return
    
    # Convert to list and sort
    common_configs = sorted(list(common_configs))
    
    # Plot throughput speedup comparison
    plt.figure(figsize=(max(14, len(common_configs) * 1.5), 10))
    
    width = 0.8 / len(merged_dfs)
    x = np.arange(len(common_configs))
    
    for i, (gpu, df) in enumerate(merged_dfs.items()):
        throughput_speedups = []
        
        for config in common_configs:
            config_data = df[df['ConfigName'] == config]
            if not config_data.empty:
                throughput_speedups.append(config_data['ThroughputSpeedup'].values[0])
            else:
                throughput_speedups.append(0)
        
        plt.bar(x + i * width - (len(merged_dfs)-1) * width/2, throughput_speedups, width, label=gpu)
    
    plt.xlabel('Test Configuration', fontsize=14)
    plt.ylabel('Throughput Speedup Ratio (v3/v2)', fontsize=14)
    plt.title('v3/v2 Throughput Speedup Ratio Comparison Across GPUs (Log Scale)', fontsize=16, fontweight='bold')
    plt.xticks(x, common_configs, rotation=45, ha='right')
    plt.yscale('log')  # Set log scale for multi-GPU throughput speedup
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    # Add a horizontal line at y=1
    plt.axhline(y=1, color='black', linestyle='--', alpha=0.5)
    
    plt.tight_layout()
    output_file = os.path.join(output_dir, 'throughput_speedup_comparison.png')
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    
    print(f"Throughput speedup comparison chart saved to {output_file}")
    
    # Plot latency speedup comparison
    plt.figure(figsize=(max(14, len(common_configs) * 1.5), 10))
    
    for i, (gpu, df) in enumerate(merged_dfs.items()):
        latency_speedups = []
        
        for config in common_configs:
            config_data = df[df['ConfigName'] == config]
            if not config_data.empty:
                latency_speedups.append(config_data['LatencySpeedup'].values[0])
            else:
                latency_speedups.append(0)
        
        plt.bar(x + i * width - (len(merged_dfs)-1) * width/2, latency_speedups, width, label=gpu)
    
    plt.xlabel('Test Configuration', fontsize=14)
    plt.ylabel('Latency Speedup Ratio (v3/v2)', fontsize=14)
    plt.title('v3/v2 Latency Speedup Ratio Comparison Across GPUs (Log Scale)', fontsize=16, fontweight='bold')
    plt.xticks(x, common_configs, rotation=45, ha='right')
    plt.yscale('log')  # Set log scale for multi-GPU latency speedup
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    # Add a horizontal line at y=1
    plt.axhline(y=1, color='black', linestyle='--', alpha=0.5)
    
    plt.tight_layout()
    output_file = os.path.join(output_dir, 'latency_speedup_comparison.png')
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    
    print(f"Latency speedup comparison chart saved to {output_file}")

# Helper function to format large numbers with K or M suffix
def format_num(num: int) -> str:
    """Format a number with K or M suffix for readability."""
    if num >= 1000000:
        return f"{num/1000000:.0f}M"
    elif num >= 1000:
        return f"{num/1000:.0f}K"
    else:
        return str(num)

def main():
    parser = argparse.ArgumentParser(description="Benchmark visualization tool for autobatch performance comparison")
    
    # Command-line arguments
    parser.add_argument('--input', '-i', type=str, help='Input benchmark file or directory', required=True)
    parser.add_argument('--output-dir', '-o', type=str, default='doc/docs/images', help='Output directory for generated images')
    parser.add_argument('--mode', '-m', choices=['single', 'multi', 'both'], default='both', 
                        help='Mode of operation: single GPU, multi-GPU, or both')
    
    args = parser.parse_args()
    
    # Create output directory if it doesn't exist
    os.makedirs(args.output_dir, exist_ok=True)
    
    # Process input based on whether it's a file or directory
    if os.path.isfile(args.input):
        # Single file mode
        df, gpu_name = parse_benchmark_file(args.input)
        if not df.empty and (args.mode == 'single' or args.mode == 'both'):
            output_file = os.path.join(args.output_dir, f'autobatch_comparison_{gpu_name.replace(" ", "_")}.png')
            plot_benchmark_comparison(df, gpu_name, output_file)
        else:
            print(f"Error: No valid data found in {args.input}")
    elif os.path.isdir(args.input):
        # Directory mode
        gpu_data_dict = process_benchmark_files(args.input)
        
        if not gpu_data_dict:
            print(f"Error: No valid benchmark files found in {args.input}")
            return
        
        # Single GPU plots
        if args.mode == 'single' or args.mode == 'both':
            for gpu_name, df in gpu_data_dict.items():
                output_file = os.path.join(args.output_dir, f'autobatch_comparison_{gpu_name.replace(" ", "_")}.png')
                plot_benchmark_comparison(df, gpu_name, output_file)
        
        # Multi-GPU comparison
        if (args.mode == 'multi' or args.mode == 'both') and len(gpu_data_dict) >= 2:
            plot_multi_gpu_comparison(gpu_data_dict, args.output_dir)
    else:
        print(f"Error: Input {args.input} is not a valid file or directory")

if __name__ == "__main__":
    main() 