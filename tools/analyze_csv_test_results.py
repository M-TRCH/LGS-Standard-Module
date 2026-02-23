#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Script to analyze and visualize Modbus TCP test results from CSV logs
Generates comprehensive analysis charts for test data

Command to run:
& "$env:USERPROFILE\miniconda3\python.exe" tools/analyze_csv_test_results.py "logs/random_test_2026-02-17_021503.csv"
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from datetime import datetime
import re
import os
import sys


def load_csv_data(csv_file_path):
    """
    Load CSV test data
    
    Args:
        csv_file_path: Path to the CSV log file
        
    Returns:
        DataFrame: Loaded test data
    """
    try:
        df = pd.read_csv(csv_file_path)
        df['Timestamp'] = pd.to_datetime(df['Timestamp'])
        return df
    except Exception as e:
        print(f"Error loading CSV file: {e}")
        sys.exit(1)


def plot_response_time_analysis(df, output_dir):
    """
    Plot response time analysis for different event types
    """
    # Filter for events with response times
    df_response = df[df['Response_Time_ms'].notna()].copy()
    
    if df_response.empty:
        print("No response time data available")
        return
    
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    fig.suptitle('Response Time Analysis', fontsize=16, fontweight='bold')
    
    # 1. Response time distribution by event type
    event_types = df_response['Event_Type'].unique()
    ax1 = axes[0, 0]
    for event_type in event_types:
        data = df_response[df_response['Event_Type'] == event_type]['Response_Time_ms']
        if len(data) > 0:
            ax1.hist(data, alpha=0.6, label=event_type, bins=30)
    ax1.set_xlabel('Response Time (ms)')
    ax1.set_ylabel('Frequency')
    ax1.set_title('Response Time Distribution by Event Type')
    ax1.legend()
    ax1.grid(True, alpha=0.3)
    
    # 2. Response time over cycles
    ax2 = axes[0, 1]
    for event_type in event_types:
        data = df_response[df_response['Event_Type'] == event_type]
        if len(data) > 0:
            ax2.scatter(data['Cycle'], data['Response_Time_ms'], 
                       alpha=0.5, label=event_type, s=10)
    ax2.set_xlabel('Cycle Number')
    ax2.set_ylabel('Response Time (ms)')
    ax2.set_title('Response Time vs Cycle Number')
    ax2.legend()
    ax2.grid(True, alpha=0.3)
    
    # 3. Box plot by event type
    ax3 = axes[1, 0]
    response_data = [df_response[df_response['Event_Type'] == et]['Response_Time_ms'].values 
                     for et in event_types if len(df_response[df_response['Event_Type'] == et]) > 0]
    labels = [et for et in event_types if len(df_response[df_response['Event_Type'] == et]) > 0]
    ax3.boxplot(response_data, tick_labels=labels)
    ax3.set_ylabel('Response Time (ms)')
    ax3.set_title('Response Time Statistics by Event Type')
    ax3.tick_params(axis='x', rotation=45)
    ax3.grid(True, alpha=0.3)
    
    # 4. Response time statistics table
    ax4 = axes[1, 1]
    ax4.axis('off')
    
    stats_data = []
    for event_type in event_types:
        data = df_response[df_response['Event_Type'] == event_type]['Response_Time_ms']
        if len(data) > 0:
            stats_data.append([
                event_type,
                f"{data.mean():.2f}",
                f"{data.median():.2f}",
                f"{data.std():.2f}",
                f"{data.min():.2f}",
                f"{data.max():.2f}"
            ])
    
    if stats_data:
        table = ax4.table(cellText=stats_data,
                         colLabels=['Event Type', 'Mean', 'Median', 'Std Dev', 'Min', 'Max'],
                         cellLoc='center',
                         loc='center',
                         colWidths=[0.25, 0.15, 0.15, 0.15, 0.15, 0.15])
        table.auto_set_font_size(False)
        table.set_fontsize(9)
        table.scale(1, 2)
        ax4.set_title('Response Time Statistics (ms)', pad=20)
    
    plt.tight_layout()
    output_file = os.path.join(output_dir, 'response_time_analysis.png')
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Saved: {output_file}")
    plt.close()


def plot_success_rate_analysis(df, output_dir):
    """
    Plot success rate analysis by cabinet and event type
    """
    # Filter operational events only
    event_types = ['WRITE_SINGLE_COIL', 'READ_REGISTER', 'CYCLE_END']
    df_ops = df[df['Event_Type'].isin(event_types)].copy()
    
    if df_ops.empty:
        print("No operational event data available")
        return
    
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    fig.suptitle('Success Rate Analysis', fontsize=16, fontweight='bold')
    
    # 1. Overall success rate by event type
    ax1 = axes[0, 0]
    success_by_event = df_ops.groupby(['Event_Type', 'Status']).size().unstack(fill_value=0)
    if 'SUCCESS' in success_by_event.columns:
        total = success_by_event.sum(axis=1)
        success_rate = (success_by_event['SUCCESS'] / total * 100)
        success_rate.plot(kind='bar', ax=ax1, color='green', alpha=0.7)
    ax1.set_ylabel('Success Rate (%)')
    ax1.set_title('Success Rate by Event Type')
    ax1.set_ylim([0, 105])
    ax1.grid(True, alpha=0.3, axis='y')
    ax1.tick_params(axis='x', rotation=45)
    
    # 2. Success rate by cabinet
    ax2 = axes[0, 1]
    df_cabinet = df_ops[df_ops['Cabinet'].notna()].copy()
    if not df_cabinet.empty:
        success_by_cabinet = df_cabinet.groupby(['Cabinet', 'Status']).size().unstack(fill_value=0)
        if 'SUCCESS' in success_by_cabinet.columns:
            total = success_by_cabinet.sum(axis=1)
            success_rate = (success_by_cabinet['SUCCESS'] / total * 100)
            success_rate.plot(kind='bar', ax=ax2, color='blue', alpha=0.7)
    ax2.set_ylabel('Success Rate (%)')
    ax2.set_title('Success Rate by Cabinet')
    ax2.set_ylim([0, 105])
    ax2.grid(True, alpha=0.3, axis='y')
    ax2.tick_params(axis='x', rotation=45)
    
    # 3. Success/Failure counts over time
    ax3 = axes[1, 0]
    df_ops['Hour'] = df_ops['Timestamp'].dt.floor('h')
    hourly_status = df_ops.groupby(['Hour', 'Status']).size().unstack(fill_value=0)
    if 'SUCCESS' in hourly_status.columns:
        hourly_status['SUCCESS'].plot(ax=ax3, label='Success', marker='o', color='green')
    if 'FAILURE' in hourly_status.columns:
        hourly_status['FAILURE'].plot(ax=ax3, label='Failure', marker='x', color='red')
    ax3.set_xlabel('Time')
    ax3.set_ylabel('Event Count')
    ax3.set_title('Success/Failure Events Over Time')
    ax3.legend()
    ax3.grid(True, alpha=0.3)
    ax3.tick_params(axis='x', rotation=45)
    
    # 4. Event type distribution
    ax4 = axes[1, 1]
    event_counts = df_ops['Event_Type'].value_counts()
    colors = plt.cm.Set3(range(len(event_counts)))
    ax4.pie(event_counts.values, labels=event_counts.index, autopct='%1.1f%%',
            colors=colors, startangle=90)
    ax4.set_title('Event Type Distribution')
    
    plt.tight_layout()
    output_file = os.path.join(output_dir, 'success_rate_analysis.png')
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Saved: {output_file}")
    plt.close()


def plot_cycle_performance(df, output_dir):
    """
    Plot cycle performance metrics
    """
    # Get cycle-level data
    df_cycles = df[df['Event_Type'] == 'CYCLE_END'].copy()
    
    if df_cycles.empty:
        print("No cycle end data available")
        return
    
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    fig.suptitle('Cycle Performance Analysis', fontsize=16, fontweight='bold')
    
    # 1. Cycle completion time
    ax1 = axes[0, 0]
    ax1.plot(df_cycles['Cycle'], df_cycles['Response_Time_ms'], 
             marker='o', markersize=3, linestyle='-', linewidth=0.5)
    ax1.set_xlabel('Cycle Number')
    ax1.set_ylabel('Cycle Time (ms)')
    ax1.set_title('Cycle Completion Time')
    ax1.grid(True, alpha=0.3)
    
    # 2. Cycle time distribution
    ax2 = axes[0, 1]
    ax2.hist(df_cycles['Response_Time_ms'], bins=50, color='skyblue', edgecolor='black')
    ax2.axvline(df_cycles['Response_Time_ms'].mean(), color='red', 
                linestyle='--', label=f"Mean: {df_cycles['Response_Time_ms'].mean():.2f} ms")
    ax2.axvline(df_cycles['Response_Time_ms'].median(), color='green', 
                linestyle='--', label=f"Median: {df_cycles['Response_Time_ms'].median():.2f} ms")
    ax2.set_xlabel('Cycle Time (ms)')
    ax2.set_ylabel('Frequency')
    ax2.set_title('Cycle Time Distribution')
    ax2.legend()
    ax2.grid(True, alpha=0.3)
    
    # 3. Cycles per cabinet
    ax3 = axes[1, 0]
    cabinet_counts = df_cycles['Cabinet'].value_counts().sort_index()
    cabinet_counts.plot(kind='bar', ax=ax3, color='orange', alpha=0.7)
    ax3.set_xlabel('Cabinet')
    ax3.set_ylabel('Number of Cycles')
    ax3.set_title('Cycles per Cabinet')
    ax3.grid(True, alpha=0.3, axis='y')
    ax3.tick_params(axis='x', rotation=45)
    
    # 4. Cycle success vs failure
    ax4 = axes[1, 1]
    status_counts = df_cycles['Status'].value_counts()
    colors = ['green' if status == 'SUCCESS' else 'red' for status in status_counts.index]
    ax4.pie(status_counts.values, labels=status_counts.index, autopct='%1.1f%%',
            colors=colors, startangle=90)
    ax4.set_title('Cycle Completion Status')
    
    plt.tight_layout()
    output_file = os.path.join(output_dir, 'cycle_performance.png')
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Saved: {output_file}")
    plt.close()


def plot_cabinet_unit_analysis(df, output_dir):
    """
    Plot cabinet and unit analysis
    """
    df_units = df[df['Unit_ID'].notna()].copy()
    
    if df_units.empty:
        print("No unit data available")
        return
    
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    fig.suptitle('Cabinet and Unit Analysis', fontsize=16, fontweight='bold')
    
    # 1. Unit activity heatmap data
    ax1 = axes[0, 0]
    unit_cabinet = df_units.groupby(['Cabinet', 'Unit_ID']).size().reset_index(name='Count')
    top_units = unit_cabinet.nlargest(20, 'Count')
    ax1.barh(range(len(top_units)), top_units['Count'], color='steelblue')
    ax1.set_yticks(range(len(top_units)))
    ax1.set_yticklabels([f"{row['Cabinet']}-{int(row['Unit_ID'])}" 
                          for _, row in top_units.iterrows()])
    ax1.set_xlabel('Number of Events')
    ax1.set_title('Top 20 Active Cabinet-Unit Combinations')
    ax1.grid(True, alpha=0.3, axis='x')
    
    # 2. Address usage frequency
    ax2 = axes[0, 1]
    df_addr = df[df['Address'].notna()].copy()
    if not df_addr.empty:
        addr_counts = df_addr['Address'].value_counts().head(15)
        addr_counts.plot(kind='bar', ax=ax2, color='teal', alpha=0.7)
        ax2.set_xlabel('Address')
        ax2.set_ylabel('Frequency')
        ax2.set_title('Top 15 Most Used Addresses')
        ax2.grid(True, alpha=0.3, axis='y')
        ax2.tick_params(axis='x', rotation=45)
    
    # 3. Events timeline
    ax3 = axes[1, 0]
    df_units['Minute'] = df_units['Timestamp'].dt.floor('min')
    events_per_minute = df_units.groupby('Minute').size()
    ax3.plot(events_per_minute.index, events_per_minute.values, 
             marker='o', markersize=2, linestyle='-', linewidth=1)
    ax3.set_xlabel('Time')
    ax3.set_ylabel('Events per Minute')
    ax3.set_title('Event Rate Over Time')
    ax3.grid(True, alpha=0.3)
    ax3.tick_params(axis='x', rotation=45)
    
    # 4. Statistics summary
    ax4 = axes[1, 1]
    ax4.axis('off')
    
    total_cycles = df['Cycle'].max() if 'Cycle' in df.columns else 0
    total_events = len(df)
    unique_cabinets = df_units['Cabinet'].nunique()
    unique_units = df_units['Unit_ID'].nunique()
    
    df_response = df[df['Response_Time_ms'].notna()]
    avg_response = df_response['Response_Time_ms'].mean() if not df_response.empty else 0
    
    summary_text = f"""
    Test Summary Statistics
    
    Total Cycles: {total_cycles:,}
    Total Events: {total_events:,}
    Unique Cabinets: {unique_cabinets}
    Unique Units: {unique_units}
    
    Average Response Time: {avg_response:.2f} ms
    Test Duration: {(df['Timestamp'].max() - df['Timestamp'].min()).total_seconds():.0f} seconds
    
    Event Types:
    """
    
    event_summary = df['Event_Type'].value_counts().head(5)
    for event_type, count in event_summary.items():
        summary_text += f"\n    {event_type}: {count:,}"
    
    ax4.text(0.1, 0.9, summary_text, transform=ax4.transAxes,
             fontsize=11, verticalalignment='top', fontfamily='monospace',
             bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
    
    plt.tight_layout()
    output_file = os.path.join(output_dir, 'cabinet_unit_analysis.png')
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Saved: {output_file}")
    plt.close()


def plot_overall_summary(df, output_dir, title_date_str=None):
    """
    Plot overall summary with key metrics
    """
    # If a title date was provided (from filename), use it; otherwise fall back to data timestamps
    if title_date_str is None:
        if 'Timestamp' in df.columns and not df['Timestamp'].empty:
            title_date = df['Timestamp'].max().date()
            title_date_str = title_date.strftime('%d/%m/%Y')
        else:
            title_date_str = datetime.now().strftime('%d/%m/%Y')

    fig, axes = plt.subplots(2, 2, figsize=(18, 14))
    fig.suptitle(f'LGS Test Summary - {title_date_str}', fontsize=18, fontweight='bold')
    
    # 1. Response time distribution for WRITE_SINGLE_COIL and READ_REGISTER
    ax1 = axes[0, 0]
    target_events = ['WRITE_SINGLE_COIL', 'READ_REGISTER']
    df_target = df[df['Event_Type'].isin(target_events) & df['Response_Time_ms'].notna()].copy()
    
    if not df_target.empty:
        colors = {'WRITE_SINGLE_COIL': '#FF6B6B', 'READ_REGISTER': '#4ECDC4'}
        for event_type in target_events:
            data = df_target[df_target['Event_Type'] == event_type]['Response_Time_ms']
            if len(data) > 0:
                color = colors[event_type]
                # Draw filled histogram
                ax1.hist(data, alpha=0.5, bins=50, color=color, label=event_type)
                # Draw outline for clarity
                ax1.hist(data, bins=50, histtype='step', 
                        linewidth=2.5, edgecolor=color)
        ax1.set_xlabel('Response Time (ms)', fontsize=11)
        ax1.set_ylabel('Frequency', fontsize=11)
        ax1.set_title('Response Time Distribution', fontsize=12, fontweight='bold')
        ax1.legend(fontsize=10, loc='upper right')
        ax1.grid(True, alpha=0.3)
    
    # 2. Success rate and cycle count by cabinet (dual y-axis)
    ax2 = axes[0, 1]
    ax2_twin = ax2.twinx()
    
    # Get cycle data per cabinet
    df_cycles = df[df['Event_Type'] == 'CYCLE_END'].copy()
    if not df_cycles.empty:
        # Extract cabinet numbers and sort
        df_cycles['Cabinet_Num'] = df_cycles['Cabinet'].str.extract(r'(\d+)').astype(int)
        
        # Success rate
        success_by_cabinet = df_cycles.groupby('Cabinet_Num')['Status'].apply(
            lambda x: (x == 'SUCCESS').sum() / len(x) * 100
        ).sort_index()
        
        # Cycle count
        cycle_count = df_cycles.groupby('Cabinet_Num').size().sort_index()
        
        # Plot
        x_pos = np.arange(len(success_by_cabinet))
        bar_width = 0.35
        
        bars1 = ax2.bar(x_pos - bar_width/2, success_by_cabinet.values, bar_width, 
                        label='Success Rate (%)', color='green', alpha=0.7)
        bars2 = ax2_twin.bar(x_pos + bar_width/2, cycle_count.values, bar_width, 
                            label='Number of Cycles', color='blue', alpha=0.7)
        
        ax2.set_xlabel('Cabinet', fontsize=11)
        ax2.set_ylabel('Success Rate (%)', fontsize=11, color='green')
        ax2_twin.set_ylabel('Number of Cycles', fontsize=11, color='blue')
        ax2.set_title('Success Rate & Cycle Count by Cabinet', fontsize=12, fontweight='bold')
        ax2.set_xticks(x_pos)
        ax2.set_xticklabels(success_by_cabinet.index)
        ax2.tick_params(axis='y', labelcolor='green')
        ax2_twin.tick_params(axis='y', labelcolor='blue')
        ax2.set_ylim([0, 105])
        ax2.grid(True, alpha=0.3, axis='y')
        
        # Legend (disabled)
        # lines1, labels1 = ax2.get_legend_handles_labels()
        # lines2, labels2 = ax2_twin.get_legend_handles_labels()
        # ax2.legend(lines1 + lines2, labels1 + labels2, loc='lower left', fontsize=9)
    
    # 3. Event Rate Over Time (events per minute)
    ax3 = axes[1, 0]
    df_events = df[df['Event_Type'].isin(['READ_REGISTER', 'WRITE_SINGLE_COIL'])].copy()
    if not df_events.empty:
        df_events['Minute'] = df_events['Timestamp'].dt.floor('min')
        events_per_minute = df_events.groupby('Minute').size()
        
        ax3.plot(events_per_minute.index, events_per_minute.values, 
                color='steelblue', linewidth=1.5, marker='o', markersize=3)
        ax3.fill_between(events_per_minute.index, events_per_minute.values, 
                         alpha=0.3, color='steelblue')
        ax3.set_xlabel('Time', fontsize=11)
        ax3.set_ylabel('Events per Minute', fontsize=11)
        ax3.set_title('Event Rate Over Time', fontsize=12, fontweight='bold')
        ax3.grid(True, alpha=0.3)
        ax3.tick_params(axis='x', rotation=45)
        
        # Add statistics
        avg_rate = events_per_minute.mean()
        max_rate = events_per_minute.max()
        ax3.axhline(avg_rate, color='red', linestyle='--', linewidth=1.5, 
                   label=f'Average: {avg_rate:.1f} events/min')
        ax3.legend(fontsize=9)
    
    # 4. Summary table
    ax4 = axes[1, 1]
    ax4.axis('off')
    
    # Calculate summary statistics
    # Calculate total cycles from CYCLE_START events
    df_cycle_start = df[df['Event_Type'] == 'CYCLE_START']
    if not df_cycle_start.empty and 'Cycle' in df_cycle_start.columns:
        total_cycles = df_cycle_start['Cycle'].max() - df_cycle_start['Cycle'].min()
    else:
        total_cycles = 0
    
    # Calculate success/failed events from WRITE_SINGLE_COIL and READ_REGISTER only
    target_event_types = ['WRITE_SINGLE_COIL', 'READ_REGISTER']
    df_target_events = df[df['Event_Type'].isin(target_event_types)]
    total_events = len(df_target_events)
    total_success = len(df_target_events[df_target_events['Status'] == 'SUCCESS'])
    total_failed = len(df_target_events[df_target_events['Status'] != 'SUCCESS'])
    success_rate = (total_success / total_events * 100) if total_events > 0 else 0
    
    df_response = df[df['Response_Time_ms'].notna()]
    avg_response_all = df_response['Response_Time_ms'].mean() if not df_response.empty else 0
    min_response_all = df_response['Response_Time_ms'].min() if not df_response.empty else 0
    max_response_all = df_response['Response_Time_ms'].max() if not df_response.empty else 0
    
    df_write = df[(df['Event_Type'] == 'WRITE_SINGLE_COIL') & df['Response_Time_ms'].notna()]
    avg_write = df_write['Response_Time_ms'].mean() if not df_write.empty else 0
    min_write = df_write['Response_Time_ms'].min() if not df_write.empty else 0
    max_write = df_write['Response_Time_ms'].max() if not df_write.empty else 0
    
    df_read = df[(df['Event_Type'] == 'READ_REGISTER') & df['Response_Time_ms'].notna()]
    avg_read = df_read['Response_Time_ms'].mean() if not df_read.empty else 0
    min_read = df_read['Response_Time_ms'].min() if not df_read.empty else 0
    max_read = df_read['Response_Time_ms'].max() if not df_read.empty else 0
    
    unique_cabinets = df[df['Cabinet'].notna()]['Cabinet'].nunique()
    
    test_duration = (df['Timestamp'].max() - df['Timestamp'].min()).total_seconds()
    test_duration_str = f"{int(test_duration // 3600):02d}:{int((test_duration % 3600) // 60):02d}:{int(test_duration % 60):02d}"
    
    # Create table data
    table_data = [
        ['Test Duration (HH:MM:SS)', test_duration_str],
        ['Total Cycles', f'{total_cycles:,}'],
        ['Total Events', f'{total_events:,}'],
        ['Successful Events', f'{total_success:,}'],
        ['Failed Events', f'{total_failed:,}'],
        ['Success Rate', f'{success_rate:.2f}%'],
        ['Number of Cabinet Test', f'{unique_cabinets}'],
        ['Avg Response Time (Write)', f'{avg_write:.2f} ms'],
        ['Avg Response Time (Read)', f'{avg_read:.2f} ms'],
        ['Min Response Time (Write)', f'{min_write:.2f} ms'],
        ['Min Response Time (Read)', f'{min_read:.2f} ms'],
        ['Max Response Time (Write)', f'{max_write:.2f} ms'],
        ['Max Response Time (Read)', f'{max_read:.2f} ms'],
    ]
    
    # Create table
    table = ax4.table(cellText=table_data,
                     colLabels=['Metric', 'Value'],
                     cellLoc='left',
                     loc='center',
                     colWidths=[0.65, 0.35])
    table.auto_set_font_size(False)
    table.set_fontsize(10)
    table.scale(1, 2.0)
    
    # Style header
    for i in range(2):
        table[(0, i)].set_facecolor('#4CAF50')
        table[(0, i)].set_text_props(weight='bold', color='white')
    
    # Alternate row colors
    for i in range(1, len(table_data) + 1):
        if i % 2 == 0:
            table[(i, 0)].set_facecolor('#f9f9f9')
            table[(i, 1)].set_facecolor('#f9f9f9')
        else:
            table[(i, 0)].set_facecolor('#ffffff')
            table[(i, 1)].set_facecolor('#ffffff')
    
    ax4.set_title('Test Summary Statistics', fontsize=12, fontweight='bold', pad=20)
    
    plt.tight_layout()
    output_file = os.path.join(output_dir, 'overall_summary.png')
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Saved: {output_file}")
    plt.show()
    plt.close()


def generate_analysis_report(csv_file_path):
    """
    Generate comprehensive analysis report with visualizations
    
    Args:
        csv_file_path: Path to the CSV log file
    """
    print(f"Loading data from: {csv_file_path}")
    df = load_csv_data(csv_file_path)
    
    print(f"Loaded {len(df)} records from {df['Timestamp'].min()} to {df['Timestamp'].max()}")
    
    # Create output directory
    output_dir = os.path.join(os.path.dirname(csv_file_path), 'analysis_plots')
    os.makedirs(output_dir, exist_ok=True)
    print(f"Output directory: {output_dir}")
    
    # Generate all plots
    print("\nGenerating analysis charts...")
    # Derive date from CSV filename (expecting YYYY-MM-DD or YYYY_MM_DD in name)
    basename = os.path.basename(csv_file_path)
    m = re.search(r"(\d{4}[-_]\d{2}[-_]\d{2})", basename)
    if m:
        date_part = m.group(1).replace('_', '-')
        try:
            file_date = datetime.strptime(date_part, '%Y-%m-%d').date()
            title_date_str = file_date.strftime('%d/%m/%Y')
        except Exception:
            title_date_str = None
    else:
        title_date_str = None

    plot_overall_summary(df, output_dir, title_date_str)
    plot_response_time_analysis(df, output_dir)
    plot_success_rate_analysis(df, output_dir)
    plot_cycle_performance(df, output_dir)
    plot_cabinet_unit_analysis(df, output_dir)
    
    print("\nAnalysis complete! All charts saved to:", output_dir)


def main():
    """
    Main function
    """
    if len(sys.argv) < 2:
        print("Usage: python analyze_csv_test_results.py <csv_file_path>")
        print("\nExample:")
        print("  python analyze_csv_test_results.py logs/random_test_2026-02-17_021503.csv")
        sys.exit(1)
    
    csv_file_path = sys.argv[1]
    
    if not os.path.exists(csv_file_path):
        print(f"Error: File not found: {csv_file_path}")
        sys.exit(1)
    
    generate_analysis_report(csv_file_path)


if __name__ == "__main__":
    main()
