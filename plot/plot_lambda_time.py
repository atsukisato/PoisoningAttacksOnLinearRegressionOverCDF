import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import os
from load_time_results import load_injection_time_results, load_upper_bound_time_results
from plot_config import (
    TICK_SIZE, LEGEND_SIZE, XLABEL_SIZE, FONT_SIZE, DATASET_NAMES,
    BOXPLOT, ERROR_BARS
)

VERBOSE = False
ROW_HEIGHT = 5.7
COLUMN_WIDTH = 5

def plot_lambda_time(df_injection_time, df_upper_bound_time, fig_path, n_value=None, show_error_bars=None, approaches=None):
    """
    Plot lambda on the x-axis and computation time on the y-axis.
    Display data for each dataset, distribution, and R separately.
    If experiments are conducted with multiple seeds, take the average.
    Display standard deviation as error bars (if show_error_bars=True).
    
    Args:
        df_injection_time: DataFrame containing injection time results
        df_upper_bound_time: DataFrame containing upper bound time results
        fig_path: Path to save the figure
        n_value: Specific n value to filter by (if None, uses all n values)
        show_error_bars: Whether to show error bars (if None, uses ERROR_BARS setting)
        approaches: List of approaches to plot (if None, plots all available approaches)
    """
    # Determine show_error_bars based on settings
    if show_error_bars is None:
        show_error_bars = ERROR_BARS

    # Filter by specified approaches
    if approaches is not None:
        df_injection_time = df_injection_time[df_injection_time['approach'].isin(approaches)]
    
    # Filter by specified n value
    if n_value is not None:
        df_injection_time = df_injection_time[df_injection_time['n'] == n_value]
        df_upper_bound_time = df_upper_bound_time[df_upper_bound_time['n'] == n_value]
    
    # Use only data where lambda > 0
    df_injection_time = df_injection_time[df_injection_time['lambda'] > 0]
    df_upper_bound_time = df_upper_bound_time[df_upper_bound_time['lambda'] > 0]
    
    # Calculate average, standard deviation, and data point count for each seed
    df_injection_stats = df_injection_time.groupby(['dataset_name', 'data_type', 'R', 'lambda', 'approach'])['time'].agg(['mean', 'std', 'count']).reset_index()
    df_injection_stats.columns = ['dataset_name', 'data_type', 'R', 'lambda', 'approach', 'time_mean', 'time_std', 'time_count']
    
    # Group upper bound statistics by algorithm
    if 'algorithm' in df_upper_bound_time.columns:
        df_upper_bound_stats = df_upper_bound_time.groupby(['dataset_name', 'data_type', 'R', 'lambda', 'algorithm'])['time'].agg(['mean', 'std', 'count']).reset_index()
        df_upper_bound_stats.columns = ['dataset_name', 'data_type', 'R', 'lambda', 'algorithm', 'time_mean', 'time_std', 'time_count']
    else:
        df_upper_bound_stats = df_upper_bound_time.groupby(['dataset_name', 'data_type', 'R', 'lambda'])['time'].agg(['mean', 'std', 'count']).reset_index()
        df_upper_bound_stats.columns = ['dataset_name', 'data_type', 'R', 'lambda', 'time_mean', 'time_std', 'time_count']

    # Replace NaN with 0 for standard deviation (for single data point cases)
    df_injection_stats['time_std'] = df_injection_stats['time_std'].fillna(0)
    df_upper_bound_stats['time_std'] = df_upper_bound_stats['time_std'].fillna(0)
    
    # Get available combinations
    available_combinations = [
        (name, dtype, R) for name, dtype, R in df_injection_stats[['dataset_name', 'data_type', 'R']].drop_duplicates().itertuples(index=False)
        if (name, dtype, R) in DATASET_NAMES
    ]
    distributions = sorted(available_combinations, key=lambda x: DATASET_NAMES[x][0])
    
    # Fixed distribution order (leave blank if exponential is not present)
    fixed_distributions = [
        key for key in sorted(DATASET_NAMES.keys(), key=lambda x: DATASET_NAMES[x][0])
    ]
    distributions = fixed_distributions
    n_values = sorted(df_injection_time['n'].unique())
    
    # Fixed n value list (include cases where data does not exist)
    target_n_values = [1000]
    n_values = target_n_values
    
    if not distributions:
        print(f"No data available for n = {n_value}")
        return
    
    # Plot settings
    plt.rcParams['figure.figsize'] = [COLUMN_WIDTH * len(distributions) + 0.5, ROW_HEIGHT * len(n_values)]
    fig, axes = plt.subplots(len(n_values), len(distributions))
    
    # Handle case where axes is 1D
    if len(n_values) == 1:
        if len(distributions) == 1:
            axes = [[axes]]
        else:
            axes = axes.reshape(1, -1)
    elif len(distributions) == 1:
        axes = axes.reshape(-1, 1)
    
    # Calculate y-axis range
    min_time = min(df_injection_stats['time_mean'].min(), df_upper_bound_stats['time_mean'].min())
    max_time = max(df_injection_stats['time_mean'].max(), df_upper_bound_stats['time_mean'].max())
    
    for i, n_val in enumerate(n_values):
        for j, dist in enumerate(distributions):
            ax = axes[i][j]
            dataset_name, data_type, R = dist
            
            # Check if data exists
            has_injection_data = len(df_injection_stats[
                (df_injection_stats['dataset_name'] == dataset_name) & 
                (df_injection_stats['data_type'] == data_type) & 
                (df_injection_stats['R'] == R)
            ]) > 0
            
            has_upper_bound_data = len(df_upper_bound_stats[
                (df_upper_bound_stats['dataset_name'] == dataset_name) & 
                (df_upper_bound_stats['data_type'] == data_type) & 
                (df_upper_bound_stats['R'] == R)
            ]) > 0
            
            # If no data, create empty plot
            if not has_injection_data and not has_upper_bound_data:
                ax.set_xlim(0, 15)  # Default x-axis range
                ax.set_ylim(min_time * 0.5, max_time * 2.0)
                ax.grid(True, which='both', linestyle='--', linewidth=0.8)
                ax.tick_params(axis='both', labelsize=TICK_SIZE)
                ax.set_xlabel('Poisoning Percentage', fontsize=XLABEL_SIZE)
                if i == 0:
                    ax.set_title(DATASET_NAMES[dist][1], fontsize=FONT_SIZE)
                if j == 0:
                    if len(n_values) == 1:
                        ax.set_ylabel('Time [s]', fontsize=FONT_SIZE)
                    else:
                        ax.set_ylabel(rf'$\mathbf{{n={n_val}}}$' + '\nTime [s]', fontsize=FONT_SIZE)
                continue
            
            # Filter data (specific n value)
            injection_data = df_injection_stats[
                (df_injection_stats['dataset_name'] == dataset_name) & 
                (df_injection_stats['data_type'] == data_type) & 
                (df_injection_stats['R'] == R) &
                (df_injection_stats['lambda'].isin(df_injection_time[df_injection_time['n'] == n_val]['lambda'].unique()))
            ].sort_values(by='lambda')
            
            upper_bound_data = df_upper_bound_stats[
                (df_upper_bound_stats['dataset_name'] == dataset_name) & 
                (df_upper_bound_stats['data_type'] == data_type) & 
                (df_upper_bound_stats['R'] == R) &
                (df_upper_bound_stats['lambda'].isin(df_upper_bound_time[df_upper_bound_time['n'] == n_val]['lambda'].unique()))
            ]
            
            # Convert lambda value to percentage and plot each approach
            if not injection_data.empty:
                injection_data = injection_data.copy()
                injection_data['percentage'] = injection_data['lambda'] / n_val * 100
                
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
                for approach in injection_data['approach'].unique():
                    approach_data = injection_data[injection_data['approach'] == approach].sort_values(by='lambda')
                    if not approach_data.empty:
                        color = approach_colors.get(approach, 'blue')
                        marker = approach_markers.get(approach, 'o')
                        label = approach_labels.get(approach, approach)
                        
                        if show_error_bars:
                            ax.errorbar(approach_data['percentage'], approach_data['time_mean'], 
                                       yerr=approach_data['time_std'], 
                                       fmt=marker + '-', label=label, color=color, 
                                       linewidth=2, markersize=8, capsize=5, capthick=2)
                        else:
                            ax.plot(approach_data['percentage'], approach_data['time_mean'], 
                                   marker + '-', label=label, color=color, 
                                   linewidth=2, markersize=8)
                        
                        # Output number of samples for each point
                        if VERBOSE:
                            for _, row in approach_data.iterrows():
                                print(f"{label} - {DATASET_NAMES[dist][1]}, n={n_val}, percentage={row['percentage']:.1f}%: {row['time_count']} samples")
            
            if not upper_bound_data.empty:
                # Handle different algorithms
                if 'algorithm' in upper_bound_data.columns:
                    algorithms = upper_bound_data['algorithm'].unique()
                    colors = {'binary_search': 'red', 'golden_section': 'orange', 'strict': 'brown', 'legacy': 'red'}
                    markers = {'binary_search': 's', 'golden_section': '^', 'strict': 'd', 'legacy': 's'}
                    labels = {'binary_search': 'Upper Bound (Binary Search)', 'golden_section': 'Upper Bound (Golden Section)', 'strict': 'Upper Bound (Strict)', 'legacy': 'Upper Bound'}
                    
                    for alg in algorithms:
                        alg_data = upper_bound_data[upper_bound_data['algorithm'] == alg].copy().sort_values(by='lambda')
                        if not alg_data.empty:
                            alg_data['percentage'] = alg_data['lambda'] / n_val * 100
                            if show_error_bars:
                                ax.errorbar(alg_data['percentage'], alg_data['time_mean'], 
                                           yerr=alg_data['time_std'], 
                                           fmt=markers.get(alg, 's') + '-', 
                                           label=labels.get(alg, f'Upper Bound ({alg})'), 
                                           color=colors.get(alg, 'red'), 
                                           linewidth=2, markersize=8, capsize=5, capthick=2)
                            else:
                                ax.plot(alg_data['percentage'], alg_data['time_mean'], 
                                       markers.get(alg, 's') + '-', 
                                       label=labels.get(alg, f'Upper Bound ({alg})'), 
                                       color=colors.get(alg, 'red'), 
                                       linewidth=2, markersize=8)
                            
                            # Output number of samples for each point
                            if VERBOSE:
                                for _, row in alg_data.iterrows():
                                    print(f"Upper Bound ({alg}) - {DATASET_NAMES[dist][1]}, n={n_val}, percentage={row['percentage']:.1f}%: {row['time_count']} samples")
                else:
                    upper_bound_data = upper_bound_data.copy().sort_values(by='lambda')
                    upper_bound_data['percentage'] = upper_bound_data['lambda'] / n_val * 100
                    if show_error_bars:
                        ax.errorbar(upper_bound_data['percentage'], upper_bound_data['time_mean'], 
                                   yerr=upper_bound_data['time_std'], 
                                   fmt='s-', label='Upper Bound', color='red', 
                                   linewidth=2, markersize=8, capsize=5, capthick=2)
                    else:
                        ax.plot(upper_bound_data['percentage'], upper_bound_data['time_mean'], 
                               's-', label='Upper Bound', color='red', 
                               linewidth=2, markersize=8)
                    
                    # Output number of samples for each point
                    if VERBOSE:
                        for _, row in upper_bound_data.iterrows():
                            print(f"Upper Bound - {DATASET_NAMES[dist][1]}, n={n_val}, percentage={row['percentage']:.1f}%: {row['time_count']} samples")
            
            # ax.set_xscale('log')
            ax.set_yscale('log')
            ax.grid(True, which='both', linestyle='--', linewidth=0.8)
            ax.tick_params(axis='both', labelsize=TICK_SIZE)
            ax.set_xlabel('Poisoning Percentage', fontsize=XLABEL_SIZE)
            if i == 0:
                ax.set_title(DATASET_NAMES[dist][1], fontsize=FONT_SIZE)
            
            if j == 0:
                if len(n_values) == 1:
                    ax.set_ylabel('Time [s]', fontsize=FONT_SIZE)
                else:
                    ax.set_ylabel(rf'$\mathbf{{n={n_val}}}$' + '\nTime [s]', fontsize=FONT_SIZE)
            
            # Set y-axis range
            ax.set_ylim(min_time * 0.5, max_time * 2.0)
    
    # Place legend horizontally at the bottom of the graph
    handles, labels = axes[len(n_values) - 1][len(distributions) - 1].get_legend_handles_labels()
    if handles and labels:
        desired_order = [
            'Greedy Poisoning',
            'Seg{+}E',
            'Seg{+}E (Relaxed)',
            'Seg{+}E (Heuristic)',
            'Duplicate Allowed',
            'Upper Bound (Golden Section)',
            'Upper Bound (Binary Search)', 
            'Upper Bound (Strict)',
        ]
        ordered_handles = []
        ordered_labels = []
        for desired_label in desired_order:
            for handle, label in zip(handles, labels):
                if label == desired_label:
                    ordered_handles.append(handle)
                    ordered_labels.append(label)
                    break
        # Add labels that are not in the specified order (just in case)
        for handle, label in zip(handles, labels):
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
    os.makedirs(os.path.dirname(fig_path), exist_ok=True)
    plt.savefig(fig_path, bbox_inches='tight')
    plt.close()
    
    print(f"Saved {fig_path}")

if __name__ == '__main__':
    # Set output directory to results/figures
    output_dir = "../results/figures/lambda_time"
    os.makedirs(output_dir, exist_ok=True)
    
    # Data directory (poisoning folder)
    data_dir = ".."
    
    # Define which approaches to plot (can be customized)
    approaches_to_plot = ['inject_poison', 'consecutive_w_endpoints', 'consecutive_w_endpoints_duplicate_allowed', 'consecutive_w_endpoints_using_relaxed_solution']
    
    df_injection_time = load_injection_time_results(data_dir, approaches=approaches_to_plot)
    df_upper_bound_time = load_upper_bound_time_results(data_dir)
    
    # Plot based on settings
    ns = [1000]
    df_injection_time_ns = df_injection_time[df_injection_time['n'].isin(ns)]
    df_upper_bound_time_ns = df_upper_bound_time[df_upper_bound_time['n'].isin(ns)]
    plot_lambda_time(df_injection_time_ns, df_upper_bound_time_ns, f'{output_dir}/lambda_time_all.pdf', 
                    n_value=None, show_error_bars=None, approaches=approaches_to_plot)
