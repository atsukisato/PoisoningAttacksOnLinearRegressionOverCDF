import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import os
from load_time_results import load_injection_time_results, load_upper_bound_time_results
from plot_config import (
    TICK_SIZE, LEGEND_SIZE, XLABEL_SIZE, FONT_SIZE, DATASET_NAMES,
    BOXPLOT, ERROR_BARS, MAX_N, MIN_N
)

VERBOSE = False
ROW_HEIGHT = 5.7
COLUMN_WIDTH = 5

def plot_n_time(df_injection_time, df_upper_bound_time, fig_path, percentage_value=None, show_error_bars=None, approaches=None):
    """
    Plot n on the x-axis and computation time on the y-axis.
    Display data for each dataset, distribution, and R separately.
    If experiments are conducted with multiple seeds, take the average.
    Plot for the same poison percentage (lambda/n).
    Display standard deviation as error bars (if show_error_bars=True).
    
    Args:
        df_injection_time: DataFrame containing injection time results
        df_upper_bound_time: DataFrame containing upper bound time results
        fig_path: Path to save the figure
        percentage_value: Specific percentage value to filter by (if None, uses all percentage values)
        show_error_bars: Whether to show error bars (if None, uses ERROR_BARS setting)
        approaches: List of approaches to plot (if None, plots all available approaches)
    """
    # Determine show_error_bars based on settings
    if show_error_bars is None:
        show_error_bars = ERROR_BARS

    # Filter by specified approaches
    if approaches is not None:
        df_injection_time = df_injection_time[df_injection_time['approach'].isin(approaches)]
    
    # Calculate poison percentage
    df_injection_time = df_injection_time.copy()
    df_injection_time['percentage'] = df_injection_time['lambda'] / df_injection_time['n'] * 100
    df_upper_bound_time = df_upper_bound_time.copy()
    df_upper_bound_time['percentage'] = df_upper_bound_time['lambda'] / df_upper_bound_time['n'] * 100
    
    # Filter by specified percentage value (considering tolerance)
    if percentage_value is not None:
        tolerance = 0.1  # Tolerance for percentage
        df_injection_time = df_injection_time[abs(df_injection_time['percentage'] - percentage_value) < tolerance]
        df_upper_bound_time = df_upper_bound_time[abs(df_upper_bound_time['percentage'] - percentage_value) < tolerance]
    
    # Calculate average and standard deviation for each seed
    df_injection_stats = df_injection_time.groupby(['dataset_name', 'data_type', 'R', 'n', 'percentage', 'approach'])['time'].agg(['mean', 'std', 'count']).reset_index()
    df_injection_stats.columns = ['dataset_name', 'data_type', 'R', 'n', 'percentage', 'approach', 'time_mean', 'time_std', 'time_count']
    
    # Group upper bound statistics by algorithm
    if 'algorithm' in df_upper_bound_time.columns:
        df_upper_bound_stats = df_upper_bound_time.groupby(['dataset_name', 'data_type', 'R', 'n', 'percentage', 'algorithm'])['time'].agg(['mean', 'std', 'count']).reset_index()
        df_upper_bound_stats.columns = ['dataset_name', 'data_type', 'R', 'n', 'percentage', 'algorithm', 'time_mean', 'time_std', 'time_count']
    else:
        df_upper_bound_stats = df_upper_bound_time.groupby(['dataset_name', 'data_type', 'R', 'n', 'percentage'])['time'].agg(['mean', 'std', 'count']).reset_index()
        df_upper_bound_stats.columns = ['dataset_name', 'data_type', 'R', 'n', 'percentage', 'time_mean', 'time_std', 'time_count']
    
    # Replace NaN with 0 for standard deviation (for single data point cases)
    df_injection_stats['time_std'] = df_injection_stats['time_std'].fillna(0)
    df_upper_bound_stats['time_std'] = df_upper_bound_stats['time_std'].fillna(0)

    # Get available combinations
    available_combinations = [
        (name, dtype, R) for name, dtype, R in df_injection_stats[['dataset_name', 'data_type', 'R']].drop_duplicates().itertuples(index=False)
        if (name, dtype, R) in DATASET_NAMES
    ]
    distributions = sorted(available_combinations, key=lambda x: DATASET_NAMES[x][0])
    
    # Fixed distribution order (blank if exponential is not present)
    fixed_distributions = [
        key for key in sorted(DATASET_NAMES.keys(), key=lambda x: DATASET_NAMES[x][0])
    ]
    distributions = fixed_distributions
    percentage_values = sorted(df_injection_time['percentage'].unique())
    
    if not distributions:
        print(f"No data available for percentage = {percentage_value}%")
        return
    
    # Plot settings
    plt.rcParams['figure.figsize'] = [COLUMN_WIDTH * len(distributions) + 0.5, ROW_HEIGHT * len(percentage_values)]
    fig, axes = plt.subplots(len(percentage_values), len(distributions))
    
    # Process if axes is one-dimensional
    if len(percentage_values) == 1:
        if len(distributions) == 1:
            axes = [[axes]]
        else:
            axes = axes.reshape(1, -1)
    elif len(distributions) == 1:
        axes = axes.reshape(-1, 1)
    
    # Calculate y-axis range
    min_time = min(df_injection_stats['time_mean'].min(), df_upper_bound_stats['time_mean'].min())
    max_time = max(df_injection_stats['time_mean'].max(), df_upper_bound_stats['time_mean'].max())
    
    for i, percentage_val in enumerate(percentage_values):
        for j, dist in enumerate(distributions):
            ax = axes[i][j]
            dataset_name, data_type, R = dist
            
            # Filter by specific percentage value
            tolerance = 0.1
            df_injection_stats_percentage = df_injection_stats[
                (df_injection_stats['dataset_name'] == dataset_name) & 
                (df_injection_stats['data_type'] == data_type) & 
                (df_injection_stats['R'] == R) &
                (df_injection_stats['percentage'] == percentage_val)
            ].sort_values(by='n')
            
            df_upper_bound_stats_percentage = df_upper_bound_stats[
                (df_upper_bound_stats['dataset_name'] == dataset_name) & 
                (df_upper_bound_stats['data_type'] == data_type) & 
                (df_upper_bound_stats['R'] == R) &
                (df_upper_bound_stats['percentage'] == percentage_val)
            ]
            
            # Create empty plot if no data exists
            if df_injection_stats_percentage.empty and df_upper_bound_stats_percentage.empty:
                ax.set_xlim(100, 10000)  # Default x-axis range
                ax.set_ylim(min_time * 0.5, max_time * 2.0)
                ax.set_xscale('log')
                ax.set_yscale('log')
                ax.grid(True, which='both', linestyle='--', linewidth=0.8)
                ax.tick_params(axis='both', labelsize=TICK_SIZE)
                ax.set_xlabel('n', fontsize=FONT_SIZE)
                if i == 0:
                    ax.set_title(DATASET_NAMES[dist][1], fontsize=FONT_SIZE)
                if j == 0:
                    if len(percentage_values) == 1:
                        ax.set_ylabel('Time [s]', fontsize=FONT_SIZE)
                    else:
                        ax.set_ylabel(rf'$\mathbf{{Poisoning:~{percentage_val:.1f}\%}}$' + '\nTime [s]', fontsize=FONT_SIZE)
                continue
            
            # Plot each approach separately
            if not df_injection_stats_percentage.empty:
                # Define colors and markers for different approaches
                approach_colors = {
                    'inject_poison': 'blue',
                    'consecutive_w_endpoints': 'green',
                    'consecutive_w_endpoints_duplicate_allowed': 'purple',
                    'consecutive_w_endpoints_using_relaxed_solution': 'magenta',
                    'duplicate_allowed': 'orange'
                }
                approach_markers = {
                    'inject_poison': 'o',
                    'consecutive_w_endpoints': 's',
                    'consecutive_w_endpoints_duplicate_allowed': '^',
                    'consecutive_w_endpoints_using_relaxed_solution': 'p',
                    'duplicate_allowed': 'd'
                }
                approach_labels = {
                    'inject_poison': 'Greedy Poisoning',
                    'consecutive_w_endpoints': 'Seg+E',
                    'consecutive_w_endpoints_duplicate_allowed': 'Seg+E (Relaxed)',
                    'consecutive_w_endpoints_using_relaxed_solution': 'Seg+E (Heuristic)',
                    'duplicate_allowed': 'Duplicate Allowed'
                }
                
                # Plot each approach separately
                for approach in df_injection_stats_percentage['approach'].unique():
                    approach_data = df_injection_stats_percentage[df_injection_stats_percentage['approach'] == approach].sort_values(by='n')
                    if not approach_data.empty:
                        color = approach_colors.get(approach, 'blue')
                        marker = approach_markers.get(approach, 'o')
                        label = approach_labels.get(approach, approach)
                        
                        if show_error_bars:
                            ax.errorbar(approach_data['n'], approach_data['time_mean'], 
                                       yerr=approach_data['time_std'], 
                                       fmt=marker + '-', label=label, color=color, 
                                       linewidth=2, markersize=8, capsize=5, capthick=2)
                        else:
                            ax.plot(approach_data['n'], approach_data['time_mean'], 
                                   marker + '-', label=label, color=color, 
                                   linewidth=2, markersize=8)
                        
                        # Output the number of samples for each point
                        if VERBOSE:
                            for _, row in approach_data.iterrows():
                                print(f"{label} - {DATASET_NAMES[dist][1]}, n={row['n']}, percentage={row['percentage']:.1f}%: {row['time_count']} samples")
            
            if not df_upper_bound_stats_percentage.empty:
                # Handle different algorithms
                if 'algorithm' in df_upper_bound_stats_percentage.columns:
                    algorithms = df_upper_bound_stats_percentage['algorithm'].unique()
                    colors = {'binary_search': 'red', 'golden_section': 'orange', 'strict': 'brown', 'legacy': 'red'}
                    markers = {'binary_search': 's', 'golden_section': '^', 'strict': 'd', 'legacy': 's'}
                    labels = {'binary_search': 'Upper Bound (Binary Search)', 'golden_section': 'Upper Bound (Golden Section)', 'strict': 'Upper Bound (Strict)', 'legacy': 'Upper Bound'}
                    
                    for alg in algorithms:
                        alg_data = df_upper_bound_stats_percentage[df_upper_bound_stats_percentage['algorithm'] == alg].sort_values(by='n')
                        if not alg_data.empty:
                            if show_error_bars:
                                ax.errorbar(alg_data['n'], alg_data['time_mean'], 
                                           yerr=alg_data['time_std'], 
                                           fmt=markers.get(alg, 's') + '-', 
                                           label=labels.get(alg, f'Upper Bound ({alg})'), 
                                           color=colors.get(alg, 'red'), 
                                           linewidth=2, markersize=8, capsize=5, capthick=2)
                            else:
                                ax.plot(alg_data['n'], alg_data['time_mean'], 
                                       markers.get(alg, 's') + '-', 
                                       label=labels.get(alg, f'Upper Bound ({alg})'), 
                                       color=colors.get(alg, 'red'), 
                                       linewidth=2, markersize=8)
                            
                            # Output the number of samples for each point
                            if VERBOSE:
                                for _, row in alg_data.iterrows():
                                    print(f"Upper Bound ({alg}) - {DATASET_NAMES[dist][1]}, n={row['n']}, percentage={row['percentage']:.1f}%: {row['time_count']} samples")
                else:
                    # Legacy format
                    df_upper_bound_stats_percentage = df_upper_bound_stats_percentage.sort_values(by='n')
                    if show_error_bars:
                        ax.errorbar(df_upper_bound_stats_percentage['n'], df_upper_bound_stats_percentage['time_mean'], 
                                   yerr=df_upper_bound_stats_percentage['time_std'], 
                                   fmt='s-', label='Upper Bound', color='red', 
                                   linewidth=2, markersize=8, capsize=5, capthick=2)
                    else:
                        ax.plot(df_upper_bound_stats_percentage['n'], df_upper_bound_stats_percentage['time_mean'], 
                               's-', label='Upper Bound', color='red', 
                               linewidth=2, markersize=8)
                    
                    # Output the number of samples for each point
                    if VERBOSE:
                        for _, row in df_upper_bound_stats_percentage.iterrows():
                            print(f"Upper Bound - {DATASET_NAMES[dist][1]}, n={row['n']}, percentage={row['percentage']:.1f}%: {row['time_count']} samples")
            
            ax.set_xscale('log')
            ax.set_yscale('log')
            ax.grid(True, which='both', linestyle='--', linewidth=0.8)
            ax.tick_params(axis='both', labelsize=TICK_SIZE)
            ax.set_xlabel('n', fontsize=FONT_SIZE)
            if i == 0:
                ax.set_title(DATASET_NAMES[dist][1], fontsize=FONT_SIZE)
            
            if j == 0:
                if len(percentage_values) == 1:
                    ax.set_ylabel('Time [s]', fontsize=FONT_SIZE)
                else:
                    ax.set_ylabel(rf'$\mathbf{{Poisoning:~{percentage_val:.1f}\%}}$' + '\nTime [s]', fontsize=FONT_SIZE)
            
            # Set y-axis range
            ax.set_ylim(min_time * 0.5, max_time * 2.0)
    
    # Place legend below the graph (horizontally)
    # Collect legend information from all subplots
    all_handles = []
    all_labels = []
    for i in range(len(percentage_values)):
        for j in range(len(distributions)):
            handles, labels = axes[i][j].get_legend_handles_labels()
            for handle, label in zip(handles, labels):
                if label not in all_labels:
                    all_handles.append(handle)
                    all_labels.append(label)
    
    # Display legend only if it exists
    if all_handles:
        desired_order = [
            'Greedy Poisoning',
            'Seg{+}E',
            'Seg{+}E (Relaxed)',
            'Seg{+}E (Heuristic)',
            'Duplicate Allowed',
            'Upper Bound (Golden Section)',
            'Upper Bound (Binary Search)', 
            'Upper Bound (Strict)'
        ]
        ordered_handles = []
        ordered_labels = []
        for desired_label in desired_order:
            for handle, label in zip(all_handles, all_labels):
                if label == desired_label:
                    ordered_handles.append(handle)
                    ordered_labels.append(label)
                    break
        # Add labels that are not in the specified order (just in case)
        for handle, label in zip(all_handles, all_labels):
            if label not in ordered_labels:
                ordered_handles.append(handle)
                ordered_labels.append(label)
        fig.legend(ordered_handles, ordered_labels, 
                   bbox_to_anchor=(0.5, 0.0), 
                   loc='upper center',
                   ncol=(len(ordered_handles)+1)//2,
                   frameon=True,
                   facecolor='white',
                   edgecolor='black',
                   fontsize=LEGEND_SIZE)
    
    plt.tight_layout()
    plt.savefig(fig_path, bbox_inches='tight')
    plt.close()
    
    print(f"Saved {fig_path}")

if __name__ == '__main__':
    # Set output directory to results/figures
    output_dir = "../results/figures/n_time"
    os.makedirs(output_dir, exist_ok=True)
    
    # Data directory (poisoning folder)
    data_dir = ".."
    
    # Define which approaches to plot (can be customized)
    approaches_to_plot = ['inject_poison', 'consecutive_w_endpoints', 'consecutive_w_endpoints_duplicate_allowed', 'consecutive_w_endpoints_using_relaxed_solution']
    
    df_injection_time = load_injection_time_results(data_dir, approaches=approaches_to_plot)
    df_upper_bound_time = load_upper_bound_time_results(data_dir)

    df_injection_time = df_injection_time[(MIN_N <= df_injection_time['n']) & (df_injection_time['n'] <= MAX_N)]
    df_upper_bound_time = df_upper_bound_time[(MIN_N <= df_upper_bound_time['n']) & (df_upper_bound_time['n'] <= MAX_N)]

    percentages = [10]
    df_injection_time['percentage'] = df_injection_time['lambda'] / df_injection_time['n'] * 100
    df_upper_bound_time['percentage'] = df_upper_bound_time['lambda'] / df_upper_bound_time['n'] * 100
    df_injection_time = df_injection_time[df_injection_time['percentage'].isin(percentages)]
    df_upper_bound_time = df_upper_bound_time[df_upper_bound_time['percentage'].isin(percentages)]
    df_injection_time = df_injection_time.drop(columns=['percentage'])
    df_upper_bound_time = df_upper_bound_time.drop(columns=['percentage'])
    
    # Plot based on settings
    plot_n_time(df_injection_time, df_upper_bound_time, f'{output_dir}/n_time_all.pdf', 
               percentage_value=None, show_error_bars=None, approaches=approaches_to_plot)
