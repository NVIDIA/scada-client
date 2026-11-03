#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

import re
import numpy as np
import matplotlib.pyplot as plt
import os
import pandas as pd

# Function to parse benchmark output files
def parse_benchmark_file(file_path):
    with open(file_path, 'r') as f:
        content = f.read()
    
    # Extract GPU name
    gpu_match = re.search(r'Running benchmark on (.*)', content)
    gpu_name = gpu_match.group(1) if gpu_match else "Unknown GPU"
    
    # Parse all benchmark test results
    pattern = r'Testing autobatch (v\d+) with N_THREADS=(\d+), N_REQUESTS=(\d+), N_BUFFERS=(\d+), N_PUSHES\(per thread\)=(\d+), UseCg=(\d+)\.\.\.[\s\S]*?Total pushes per second: ([0-9.e+]+)[\s\S]*?System time taken: ([0-9.]+) seconds'
    
    results = []
    for match in re.finditer(pattern, content):
        version, threads, requests, buffers, pushes, use_cg, throughput, system_time = match.groups()
        results.append({
            'GPU': gpu_name,
            'Version': version,
            'Threads': int(threads),
            'Requests': int(requests),
            'Buffers': int(buffers),
            'Pushes': int(pushes),
            'UseCg': int(use_cg),
            'Throughput': float(throughput),
            'SystemTime': float(system_time)
        })
    
    return results

# Process all benchmark files in a directory
def process_benchmark_files(directory='perf/tmp/'):
    all_results = []
    
    for filename in os.listdir(directory):
        if filename.startswith('bench.out-'):
            file_path = os.path.join(directory, filename)
            results = parse_benchmark_file(file_path)
            all_results.extend(results)
    
    return pd.DataFrame(all_results)

# Plot functions
def plot_throughput_comparison(data, output_file='throughput_comparison.png'):
    # Create a unique configuration identifier
    data['Config'] = data.apply(
        lambda row: f"T{row['Threads']}_R{row['Requests']}_B{row['Buffers']}_P{row['Pushes']}_C{row['UseCg']}", 
        axis=1
    )
    
    # Get unique configurations and GPUs
    configs = data['Config'].unique()
    gpus = data['GPU'].unique()
    
    # Filter configs that have both v2 and v3 results for all GPUs
    valid_configs = []
    for config in configs:
        valid = True
        for gpu in gpus:
            gpu_data = data[(data['Config'] == config) & (data['GPU'] == gpu)]
            if len(gpu_data) < 2 or not all(version in gpu_data['Version'].values for version in ['v2', 'v3']):
                valid = False
                break
        if valid:
            valid_configs.append(config)
    
    if not valid_configs:
        print("No valid configurations found for comparison")
        return
    
    # Limit to manageable number of configs for visualization
    if len(valid_configs) > 6:
        valid_configs = valid_configs[:6]
    
    # Set up plot dimensions
    plt.figure(figsize=(14, 8))
    
    # Number of groups (configs)
    n_configs = len(valid_configs)
    # Width of each bar
    bar_width = 0.15
    # Positions for each group
    indices = np.arange(n_configs)
    
    # Plot bars for each GPU and version
    for i, gpu in enumerate(gpus):
        v2_throughputs = []
        v3_throughputs = []
        
        for config in valid_configs:
            gpu_data = data[(data['Config'] == config) & (data['GPU'] == gpu)]
            
            v2_data = gpu_data[gpu_data['Version'] == 'v2']
            v3_data = gpu_data[gpu_data['Version'] == 'v3']
            
            v2_throughputs.append(v2_data['Throughput'].values[0] if not v2_data.empty else 0)
            v3_throughputs.append(v3_data['Throughput'].values[0] if not v3_data.empty else 0)
        
        # Plot v2 bars
        plt.bar(indices - bar_width/2 + i*bar_width, v2_throughputs, bar_width, 
                label=f'{gpu} (v2)', alpha=0.7)
        
        # Plot v3 bars
        plt.bar(indices + bar_width/2 + i*bar_width, v3_throughputs, bar_width, 
                label=f'{gpu} (v3)', alpha=0.7)
    
    # Add labels and legend
    plt.xlabel('Configuration')
    plt.ylabel('Throughput (pushes per second)')
    plt.title('Autobatch Performance Comparison Across GPUs')
    plt.yscale('log')  # Log scale for wide range of values
    plt.xticks(indices, valid_configs, rotation=45, ha='right')
    plt.legend()
    plt.tight_layout()
    
    # Save figure
    plt.savefig(output_file)
    plt.close()
    print(f"Saved throughput comparison to {output_file}")
    
    # Generate simplified config descriptions for the report
    config_descriptions = []
    for config in valid_configs:
        match = re.match(r'T(\d+)_R(\d+)_B(\d+)_P(\d+)_C(\d+)', config)
        if match:
            threads, requests, buffers, pushes, use_cg = match.groups()
            desc = f"Threads: {threads}, Requests: {requests}, Buffers: {buffers}, Pushes: {pushes}, UseCg: {use_cg}"
            config_descriptions.append(desc)
    
    return valid_configs, config_descriptions

def plot_speedup_ratios(data, output_file='speedup_ratios.png'):
    # Create a unique configuration identifier
    data['Config'] = data.apply(
        lambda row: f"T{row['Threads']}_R{row['Requests']}_B{row['Buffers']}_P{row['Pushes']}_C{row['UseCg']}", 
        axis=1
    )
    
    # Get unique configurations and GPUs
    configs = data['Config'].unique()
    gpus = data['GPU'].unique()
    
    # Calculate speedup ratios for each GPU and configuration
    speedup_data = []
    
    for gpu in gpus:
        for config in configs:
            gpu_config_data = data[(data['Config'] == config) & (data['GPU'] == gpu)]
            
            if len(gpu_config_data) >= 2:
                v2_data = gpu_config_data[gpu_config_data['Version'] == 'v2']
                v3_data = gpu_config_data[gpu_config_data['Version'] == 'v3']
                
                if not v2_data.empty and not v3_data.empty:
                    v2_throughput = v2_data['Throughput'].values[0]
                    v3_throughput = v3_data['Throughput'].values[0]
                    
                    speedup = v3_throughput / v2_throughput
                    
                    speedup_data.append({
                        'GPU': gpu,
                        'Config': config,
                        'Speedup': speedup
                    })
    
    if not speedup_data:
        print("No valid data for speedup ratio calculation")
        return
    
    speedup_df = pd.DataFrame(speedup_data)
    
    # Limit to manageable number of configs for visualization
    top_configs = speedup_df.groupby('Config')['Speedup'].mean().nlargest(6).index.tolist()
    speedup_df = speedup_df[speedup_df['Config'].isin(top_configs)]
    
    # Set up plot
    plt.figure(figsize=(12, 8))
    
    # Create bar plot of speedup ratios
    for i, gpu in enumerate(gpus):
        gpu_speedup = speedup_df[speedup_df['GPU'] == gpu]
        
        if not gpu_speedup.empty:
            plt.scatter(gpu_speedup['Config'], gpu_speedup['Speedup'], label=gpu, s=100, alpha=0.7)
            plt.plot(gpu_speedup['Config'], gpu_speedup['Speedup'], alpha=0.5)
    
    # Add labels and legend
    plt.xlabel('Configuration')
    plt.ylabel('Speedup Ratio (v3/v2)')
    plt.title('Autobatch v3 vs v2 Speedup Ratios Across GPUs')
    plt.yscale('log')  # Log scale for wide range of values
    plt.axhline(y=1, color='r', linestyle='--', alpha=0.5)  # Reference line at y=1
    plt.xticks(rotation=45, ha='right')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    
    # Save figure
    plt.savefig(output_file)
    plt.close()
    print(f"Saved speedup ratio plot to {output_file}")

def plot_performance_characteristics(data, output_file='performance_characteristics.png'):
    """Plot key performance characteristics comparing v2 and v3 across configurations"""
    # Create a unique configuration identifier
    data['Config'] = data.apply(
        lambda row: f"T{row['Threads']}_R{row['Requests']}_B{row['Buffers']}_P{row['Pushes']}_C{row['UseCg']}", 
        axis=1
    )
    
    # Get configs with significant differences
    config_diffs = {}
    for config in data['Config'].unique():
        config_data = data[data['Config'] == config]
        if len(config_data) >= 2:
            v2_data = config_data[config_data['Version'] == 'v2']
            v3_data = config_data[config_data['Version'] == 'v3']
            
            if not v2_data.empty and not v3_data.empty:
                v2_throughput = v2_data['Throughput'].mean()
                v3_throughput = v3_data['Throughput'].mean()
                
                config_diffs[config] = v3_throughput / v2_throughput
    
    # Select top configs by performance difference
    top_configs = sorted(config_diffs.items(), key=lambda x: x[1], reverse=True)[:4]
    top_config_names = [x[0] for x in top_configs]
    
    # Set up the plot
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    axes = axes.flatten()
    
    # For each configuration, plot throughput and system time
    for i, config in enumerate(top_config_names):
        if i >= len(axes):
            break
            
        config_data = data[data['Config'] == config]
        
        # Group by GPU and version
        grouped = config_data.groupby(['GPU', 'Version'])
        
        # Extract configuration details
        config_match = re.match(r'T(\d+)_R(\d+)_B(\d+)_P(\d+)_C(\d+)', config)
        if config_match:
            threads, requests, buffers, pushes, use_cg = config_match.groups()
            config_title = f"Threads: {threads}, Requests: {requests}, Buffers: {buffers}\nPushes: {pushes}, UseCg: {use_cg}"
        else:
            config_title = config
        
        # Bar positions
        gpu_labels = []
        throughputs = []
        system_times = []
        versions = []
        
        for (gpu, version), group in grouped:
            gpu_labels.append(gpu)
            throughputs.append(group['Throughput'].values[0])
            system_times.append(group['SystemTime'].values[0])
            versions.append(version)
        
        # Create a DataFrame for easier plotting
        plot_data = pd.DataFrame({
            'GPU': gpu_labels,
            'Version': versions,
            'Throughput': throughputs,
            'SystemTime': system_times
        })
        
        # Calculate positions for grouped bars
        x = np.arange(len(plot_data['GPU'].unique()))
        width = 0.35
        
        # Throughput bars (log scale)
        ax1 = axes[i]
        ax1.set_title(config_title)
        
        for j, version in enumerate(['v2', 'v3']):
            version_data = plot_data[plot_data['Version'] == version]
            ax1.bar(x - width/2 + j*width, version_data['Throughput'], width, label=f'{version} Throughput')
        
        ax1.set_yscale('log')
        ax1.set_ylabel('Throughput (pushes/sec)')
        ax1.set_xlabel('GPU')
        ax1.set_xticks(x)
        ax1.set_xticklabels(plot_data['GPU'].unique())
        ax1.legend()
        
        # Add system time values as text
        for j, gpu in enumerate(plot_data['GPU'].unique()):
            gpu_data = plot_data[plot_data['GPU'] == gpu]
            
            for k, version in enumerate(['v2', 'v3']):
                version_gpu_data = gpu_data[gpu_data['Version'] == version]
                if not version_gpu_data.empty:
                    time = version_gpu_data['SystemTime'].values[0]
                    ax1.text(j - width/2 + k*width, version_gpu_data['Throughput'].values[0]*1.1, 
                             f"{time:.4f}s", ha='center', va='bottom', rotation=45)
    
    plt.tight_layout()
    plt.savefig(output_file)
    plt.close()
    print(f"Saved performance characteristics to {output_file}")

def main():
    print("Processing benchmark data...")
    data = process_benchmark_files()
    
    if data.empty:
        print("No benchmark data found!")
        return
    
    print(f"Found {len(data)} benchmark results across {data['GPU'].nunique()} GPUs.")
    
    # Create output directory if it doesn't exist
    os.makedirs('doc/docs/images', exist_ok=True)
    
    # Generate plots
    valid_configs, config_descriptions = plot_throughput_comparison(data, 'doc/docs/images/gpu_throughput_comparison.png')
    plot_speedup_ratios(data, 'doc/docs/images/gpu_speedup_ratios.png')
    plot_performance_characteristics(data, 'doc/docs/images/gpu_performance_characteristics.png')
    
    # Print out configuration details for reference
    print("\nConfiguration Details:")
    for i, (config, desc) in enumerate(zip(valid_configs, config_descriptions)):
        print(f"Config {i+1}: {config} - {desc}")
    
    print("\nPlots have been generated in the doc/docs/images directory.")
    print("You can reference these images in your documentation.")

if __name__ == "__main__":
    main() 