#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Script to plot Modbus test results from log file
Displays test cycles with success/failure counts and test duration
"""

import re
import matplotlib.pyplot as plt
from datetime import datetime
import numpy as np
import os
import sys


def parse_log_file(log_file_path):
    """
    Parse the Modbus statistics log file and extract cycle data
    
    Returns:
        dict: Parsed data with cycle numbers, success/failed counts, and times
    """
    data = {
        'cycle': [],
        'success': [],
        'failed': [],
        'disconnected': [],
        'time_seconds': [],
        'timestamp': []
    }
    
    # Pattern to match CYCLE SUMMARY lines
    pattern = r'(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) \| CYCLE #(\d+) SUMMARY \| Success: (\d+) \| Failed: (\d+) \| Disconnected: (\d+) \| Time: ([\d.]+)s'
    
    try:
        with open(log_file_path, 'r', encoding='utf-8') as f:
            for line in f:
                match = re.search(pattern, line)
                if match:
                    timestamp_str = match.group(1)
                    cycle_num = int(match.group(2))
                    success = int(match.group(3))
                    failed = int(match.group(4))
                    disconnected = int(match.group(5))
                    time_sec = float(match.group(6))
                    
                    # Parse timestamp
                    timestamp = datetime.strptime(timestamp_str, '%Y-%m-%d %H:%M:%S')
                    
                    data['cycle'].append(cycle_num)
                    data['success'].append(success)
                    data['failed'].append(failed)
                    data['disconnected'].append(disconnected)
                    data['time_seconds'].append(time_sec)
                    data['timestamp'].append(timestamp)
    
    except FileNotFoundError:
        print(f"Error: File not found - {log_file_path}")
        sys.exit(1)
    except Exception as e:
        print(f"Error reading file: {e}")
        sys.exit(1)
    
    if not data['cycle']:
        print("Warning: No CYCLE SUMMARY data found in file")
    
    return data


def plot_test_results(data, output_dir='logs'):
    """
    Create plots showing test results
    
    Args:
        data: Dictionary with parsed test data
        output_dir: Directory to save plot images
    """
    if not data['cycle']:
        print("No data to display")
        return
    
    # Create figure
    fig, ax = plt.subplots(figsize=(10, 8))
    
    cycles = np.array(data['cycle'])
    success = np.array(data['success'])
    failed = np.array(data['failed'])
    time_sec = np.array(data['time_seconds'])
    timestamps = data['timestamp']
    
    # Calculate overall statistics
    total_success = np.sum(success)
    total_failed = np.sum(failed)
    total_cycles = len(cycles)
    total_tests = total_success + total_failed
    overall_success_rate = (total_success / total_tests * 100) if total_tests > 0 else 0
    avg_time = np.mean(time_sec)
    
    # ==================== Overall Summary Bar Chart ====================
    categories = ['Total']
    x_pos = np.array([0])
    width = 0.5
    
    # Create stacked bar
    p1 = ax.bar(x_pos, [total_success], width, label=f'Success ({total_success:,})', 
                color='#2ecc71', alpha=0.85, edgecolor='white', linewidth=2)
    p2 = ax.bar(x_pos, [total_failed], width, bottom=[total_success], 
                label=f'Failed ({total_failed:,})', color='#e74c3c', alpha=0.85, 
                edgecolor='white', linewidth=2)
    
    # Customize axes
    ax.set_ylabel('Number of Tests', fontsize=14, fontweight='bold')
    ax.set_title('Modbus TCP Test Results - Overall Summary', fontsize=17, fontweight='bold', pad=20)
    ax.set_xticks(x_pos)
    ax.set_xticklabels(categories, fontsize=13, fontweight='bold')
    ax.legend(loc='upper right', fontsize=12, framealpha=0.95)
    ax.grid(axis='y', alpha=0.3, linestyle='--', linewidth=0.8)
    
    # Add value labels on bar
    if total_success > 0:
        ax.text(0, total_success/2, f'{total_success:,}', ha='center', va='center', 
               fontsize=16, fontweight='bold', color='white')
    if total_failed > 0:
        ax.text(0, total_success + total_failed/2, f'{total_failed:,}', ha='center', va='center', 
               fontsize=16, fontweight='bold', color='white')
    
    # Total label on top
    ax.text(0, total_tests + total_tests*0.02, f'{total_tests:,}', ha='center', va='bottom', 
           fontsize=13, fontweight='bold', color='#2c3e50')
    
    # Success rate label
    ax.text(0.5, 0.5, f'{overall_success_rate:.2f}%\nSuccess Rate', 
           transform=ax.transAxes, ha='center', va='center',
           fontsize=24, fontweight='bold', color='#27ae60',
           bbox=dict(boxstyle='round,pad=0.8', facecolor='white', 
                    edgecolor='#27ae60', linewidth=3, alpha=0.9))
    
    # ==================== Statistics Table ====================
    stats_text = f"""Test Statistics
─────────────────────────────────────
• Total Cycles: {total_cycles}
• Total Tests: {total_tests:,}
• Success: {total_success:,} ({overall_success_rate:.2f}%)
• Failed: {total_failed:,} ({100-overall_success_rate:.2f}%)
• Avg Time/Cycle: {avg_time:.2f}s ({avg_time/60:.2f} min)
• Max Time: {np.max(time_sec):.2f}s ({np.max(time_sec)/60:.2f} min)
• Min Time: {np.min(time_sec):.2f}s ({np.min(time_sec)/60:.2f} min)
• Total Time: {np.sum(time_sec)/3600:.2f} hours
"""
    
    # Add textbox with statistics
    props = dict(boxstyle='round', facecolor='lightblue', alpha=0.3, edgecolor='#3498db', linewidth=2)
    ax.text(0.02, 0.98, stats_text, transform=ax.transAxes, 
            fontsize=11, verticalalignment='top', family='monospace',
            bbox=props)
    
    # Set y-axis limit with some padding
    ax.set_ylim([0, total_tests * 1.15])
    
    plt.tight_layout()
    
    # Save plot
    os.makedirs(output_dir, exist_ok=True)
    output_file = os.path.join(output_dir, 'test_results_plot.png')
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"\n✓ Plot saved to: {output_file}")
    
    # Show plot
    plt.show()
    
    return output_file


def main():
    """Main function"""
    # Default log file path
    default_log = r"c:\Users\mteer\OneDrive\VS Code Workspace\LGS-Standard-Module\logs\modbus_stats_2026-02-13_135915.log"
    
    # Check if log file provided as argument
    if len(sys.argv) > 1:
        log_file = sys.argv[1]
    else:
        log_file = default_log
    
    print("=" * 60)
    print("  Modbus Test Results Plotter")
    print("=" * 60)
    print(f"\nReading file: {log_file}")
    
    # Parse log file
    data = parse_log_file(log_file)
    
    if data['cycle']:
        print(f"Found {len(data['cycle'])} test cycles")
        print("\nGenerating plot...")
        
        # Create plots
        output_file = plot_test_results(data)
        
        print("\n" + "=" * 60)
        print("  Completed!")
        print("=" * 60)
    else:
        print("No test data found in file")


if __name__ == "__main__":
    main()
