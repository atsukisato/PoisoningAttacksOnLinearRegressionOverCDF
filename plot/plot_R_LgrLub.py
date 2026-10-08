import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import os
from load_loss import load_loss
from load_upper_bound import load_upper_bound
import numpy as np # Added for np.log10
from plot_config import (
    TICK_SIZE, LEGEND_SIZE, FONT_SIZE, LOSS_COLUMN, UPPER_BOUND_COLUMN,
    DATASET_ORDER, ARTIFICIAL_DATASET_NAMES, calc_widths_for_boxplot,
    BOXPLOT, ERROR_BARS
)

VERBOSE = False
ROW_HEIGHT = 3.5
COLUMN_WIDTH = 5

def plot_R_LgrLub(df_results, df_upper_bound, fig_path, n_value=None, percentage_value=None, show_boxplot=None):
    """
    Plot R on the horizontal axis and the ratio of Lgr/Lub on the vertical axis.
    For a specific combination of n and percentage, display each dataset, distribution, and data_type separately.
    If show_boxplot=True, plot boxplots; if show_boxplot=False, plot mean/min/max.
    """
    # Determine show_boxplot based on settings
    if show_boxplot is None:
        show_boxplot = BOXPLOT

    # Filter by specified n values
    if n_value is not None:
        df_results = df_results[df_results['n'] == n_value]
        df_upper_bound = df_upper_bound[df_upper_bound['n'] == n_value]
    
    # Use only data where lambda > 0
    df_results = df_results[df_results['lambda'] > 0]
    df_upper_bound = df_upper_bound[df_upper_bound['lambda'] > 0]
    
    # Calculate percentage
    df_results = df_results.copy()
    if n_value is not None:
        df_results['percentage'] = df_results['lambda'] / df_results['n'] * 100
    else:
        df_results['percentage'] = df_results['lambda'] / df_results['n'] * 100
    
    df_upper_bound = df_upper_bound.copy()
    if n_value is not None:
        df_upper_bound['percentage'] = df_upper_bound['lambda'] / df_upper_bound['n'] * 100
    else:
        df_upper_bound['percentage'] = df_upper_bound['lambda'] / df_upper_bound['n'] * 100
    
    # Filter by specified percentage value (considering tolerance)
    if percentage_value is not None:
        tolerance = 0.1
        df_results = df_results[abs(df_results['percentage'] - percentage_value) < tolerance]
        df_upper_bound = df_upper_bound[abs(df_upper_bound['percentage'] - percentage_value) < tolerance]
    
    # If there is no data, exit
    if df_results.empty or df_upper_bound.empty:
        print(f"No data available for n={n_value}, percentage={percentage_value}%")
        return
    
    # Get n values
    n_values = sorted(df_results['n'].unique())
    
    # Get available combinations for each n value (dataset/distribution/data_type)
    available_combinations_by_n = {}
    # Fixed distribution order (leave blank if exponential is not present)
    fixed_artificial_order = sorted(ARTIFICIAL_DATASET_NAMES.keys(), key=lambda x: ARTIFICIAL_DATASET_NAMES[x][0])
    
    for n_val in n_values:
        df_results_n = df_results[df_results['n'] == n_val]
        available_combinations = []
        for name, dtype in fixed_artificial_order:
            # Get available R values for this dataset/data_type combination
            available_Rs = sorted(df_results_n[(df_results_n['dataset_name'] == name) & (df_results_n['data_type'] == dtype)]['R'].unique())
            if len(available_Rs) >= 1:  # Plot if at least one R value exists
                available_combinations.append((name, dtype, available_Rs))
            else:
                # Add empty combination if data does not exist
                available_combinations.append((name, dtype, []))
        
        # Sort datasets (only for artificial datasets)
        # available_combinations = sorted(available_combinations, key=lambda x: ARTIFICIAL_DATASET_NAMES.get((x[0], x[1]), (999, "")))
        available_combinations_by_n[n_val] = available_combinations
    
    # If there is no combination available for all n values, exit
    if not any(available_combinations_by_n.values()):
        print(f"No distributions with R values for n={n_value}, percentage={percentage_value}%")
        return
    
    # Get the maximum number of combinations (determine the number of columns for plotting)
    max_combinations = max(len(combinations) for combinations in available_combinations_by_n.values())
    
    # Plot settings
    plt.rcParams['figure.figsize'] = [COLUMN_WIDTH * max_combinations + 0.5, ROW_HEIGHT * len(n_values)]
    fig, axes = plt.subplots(len(n_values), max_combinations)
    
    # Process if axes is one-dimensional
    if len(n_values) == 1:
        if max_combinations == 1:
            axes = [[axes]]
        else:
            axes = axes.reshape(1, -1)
    elif max_combinations == 1:
        axes = axes.reshape(-1, 1)
    
    for i, n_val in enumerate(n_values):
        available_combinations = available_combinations_by_n[n_val]
        
        for j, (dataset_name, data_type, R_values) in enumerate(available_combinations):
            ax = axes[i][j]
            
            # Create empty plot if no data exists
            if not R_values:
                ax.set_xlim(1000, 1000000)  # Default x-axis range
                ax.set_ylim(0.55, 1.05)
                ax.set_xscale('log')
                ax.grid(True, which='both', linestyle='--', linewidth=0.8)
                ax.tick_params(axis='both', labelsize=TICK_SIZE)
                ax.set_xlabel('R', fontsize=FONT_SIZE)
                if i == 0:
                    title = ARTIFICIAL_DATASET_NAMES.get((dataset_name, data_type), (999, f"{dataset_name.title()}"))[1]
                    ax.set_title(title, fontsize=FONT_SIZE)
                if j == 0:
                    if len(n_values) == 1:
                        ax.set_ylabel(r'$\frac{\mathrm{MSE}_\mathrm{G}}{\mathrm{MSE}_\mathrm{UB}}$', fontsize=FONT_SIZE*1.5)
                    else:
                        ax.set_ylabel(rf'$\mathbf{{n={n_val}}}$' + '\n' + r'$\frac{\mathrm{MSE}_\mathrm{G}}{\mathrm{MSE}_\mathrm{UB}}$', fontsize=FONT_SIZE*1.5)
                continue
            
            # Filter by specific n value
            df_results_n = df_results[df_results['n'] == n_val]
            df_upper_bound_n = df_upper_bound[df_upper_bound['n'] == n_val]
            
            # Handle different algorithms
            upper_bound_data = df_upper_bound_n[
                (df_upper_bound_n['dataset_name'] == dataset_name) & 
                (df_upper_bound_n['data_type'] == data_type)
            ]
            
            if 'algorithm' in upper_bound_data.columns:
                # New format with algorithm column
                algorithms = upper_bound_data['algorithm'].unique()
                colors = {'binary_search': 'red', 'golden_section': 'orange', 'strict': 'brown', 'legacy': 'red'}
                markers = {'binary_search': 'o', 'golden_section': '^', 'strict': 'd', 'legacy': 'o'}
                min_markers = {'binary_search': 'v', 'golden_section': 's', 'strict': '<', 'legacy': 'v'}
                max_markers = {'binary_search': '*', 'golden_section': 'D', 'strict': '>', 'legacy': '*'}
                linestyles = {'binary_search': '-', 'golden_section': '--', 'strict': '-.', 'legacy': '-'}
                labels = {'binary_search': 'Binary Search', 'golden_section': 'Golden Section', 'strict': 'Strict', 'legacy': 'Legacy'}
                
                for alg in algorithms:
                    # Calculate Lgr/Lub for each R value
                    R_plot_data = []
                    Lgr_Lub_means = []
                    Lgr_Lub_mins = []
                    Lgr_Lub_maxs = []
                    Lgr_Lub_all_data = []  # For boxplot
                    
                    for R in R_values:
                        poison_data = df_results_n[
                            (df_results_n['dataset_name'] == dataset_name) & 
                            (df_results_n['data_type'] == data_type) & 
                            (df_results_n['R'] == R)
                        ][['lambda', 'seed', LOSS_COLUMN]].sort_values(by=['lambda', 'seed'])
                        
                        alg_upper_bound_data = upper_bound_data[
                            (upper_bound_data['algorithm'] == alg) &
                            (upper_bound_data['R'] == R)
                        ][['lambda', 'seed', UPPER_BOUND_COLUMN]].sort_values(by=['lambda', 'seed'])
                        
                        if poison_data.empty or alg_upper_bound_data.empty:
                            continue
                        
                        # Merge data
                        merged_data = poison_data.merge(alg_upper_bound_data, on=['lambda', 'seed'], how='inner')
                        if merged_data.empty:
                            continue
                        
                        # Calculate Lgr/Lub
                        merged_data['Lgr_divided_by_Lub'] = merged_data[LOSS_COLUMN] / merged_data[UPPER_BOUND_COLUMN]
                        
                        if show_boxplot:
                            # For boxplot, collect all data points
                            Lgr_Lub_all_data.append(merged_data['Lgr_divided_by_Lub'].values)
                        else:
                            # Calculate statistics
                            ratio_stats = merged_data['Lgr_divided_by_Lub'].agg(['mean', 'min', 'max'])
                            
                            R_plot_data.append(R)
                            Lgr_Lub_means.append(ratio_stats['mean'])
                            Lgr_Lub_mins.append(ratio_stats['min'])
                            Lgr_Lub_maxs.append(ratio_stats['max'])
                    
                    if show_boxplot:
                        if Lgr_Lub_all_data:
                            # Draw boxplot (widen the width)
                            bp = ax.boxplot(Lgr_Lub_all_data, positions=R_values[:len(Lgr_Lub_all_data)], 
                                          patch_artist=True, widths=calc_widths_for_boxplot(R_values))
                            
                            # Set boxplot color (white)
                            for patch in bp['boxes']:
                                patch.set_facecolor('white')
                                patch.set_alpha(1.0)
                            
                            # Make boxplot lines thicker
                            for element in ['boxes', 'whiskers', 'fliers', 'means', 'medians']:
                                plt.setp(bp[element], linewidth=2)
                            
                            # Output the number of samples for each boxplot
                            if VERBOSE:
                                for k, data in enumerate(Lgr_Lub_all_data):
                                    # Use ARTIFICIAL_DATASET_NAMES for artificial datasets
                                    dataset_label = ARTIFICIAL_DATASET_NAMES.get((dataset_name, data_type), (999, f"{dataset_name.title()}"))[1]
                                    print(f"Boxplot ({alg}) - {dataset_label}, n={n_val}, R={R_values[k]}: {len(data)} samples")
                    else:
                        if not R_plot_data:
                            print(f"No valid data for {dataset_name}, {data_type}, algorithm={alg}, n={n_val}")
                            continue
                        
                        # Plot
                        color = colors.get(alg, 'blue')
                        marker = markers.get(alg, 'o')
                        min_marker = min_markers.get(alg, 'v')
                        max_marker = max_markers.get(alg, '*')
                        linestyle = linestyles.get(alg, '-')
                        label_prefix = labels.get(alg, alg)
                        
                        ax.plot(R_plot_data, Lgr_Lub_means, marker + linestyle, label=f'Mean ({label_prefix})', color=color, linewidth=2, alpha=0.8)
                        ax.plot(R_plot_data, Lgr_Lub_mins, min_marker + ':', label=f'Min ({label_prefix})', color=color, linewidth=2, alpha=0.8)
                        ax.plot(R_plot_data, Lgr_Lub_maxs, max_marker + '-.', label=f'Max ({label_prefix})', color=color, linewidth=2, alpha=0.8)
                    
                    # Output the number of samples for each point
                    if VERBOSE and not show_boxplot:
                        for i_, R in enumerate(R_values[:len(Lgr_Lub_all_data) if show_boxplot else len(R_plot_data)]):
                            # Calculate the number of samples for the corresponding lambda value
                            lambda_val = merged_data.iloc[i_]['lambda'] if i_ < len(merged_data) else 0
                            n_samples = len(merged_data[merged_data['lambda'] == lambda_val]) if lambda_val > 0 else 0
                            # Use ARTIFICIAL_DATASET_NAMES for artificial datasets
                            dataset_label = ARTIFICIAL_DATASET_NAMES.get((dataset_name, data_type), (999, f"{dataset_name.title()}"))[1]
                            print(f"Upper Bound ({alg}) - {dataset_label}, n={n_val}, R={R}: {n_samples} samples")
            else:
                # Legacy format without algorithm column
                # Calculate Lgr/Lub for each R value
                R_plot_data = []
                Lgr_Lub_means = []
                Lgr_Lub_mins = []
                Lgr_Lub_maxs = []
                Lgr_Lub_all_data = []  # For boxplot
                
                for R in R_values:
                    poison_data = df_results_n[
                        (df_results_n['dataset_name'] == dataset_name) & 
                        (df_results_n['data_type'] == data_type) & 
                        (df_results_n['R'] == R)
                    ][['lambda', 'seed', LOSS_COLUMN]].sort_values(by=['lambda', 'seed'])
                    
                    alg_upper_bound_data = upper_bound_data[
                        (upper_bound_data['R'] == R)
                    ][['lambda', 'seed', UPPER_BOUND_COLUMN]].sort_values(by=['lambda', 'seed'])
                    
                    if poison_data.empty or alg_upper_bound_data.empty:
                        continue
                    
                    # Merge data
                    merged_data = poison_data.merge(alg_upper_bound_data, on=['lambda', 'seed'], how='inner')
                    if merged_data.empty:
                        continue
                    
                    # Calculate Lgr/Lub
                    merged_data['Lgr_divided_by_Lub'] = merged_data[LOSS_COLUMN] / merged_data[UPPER_BOUND_COLUMN]
                    
                    if show_boxplot:
                        # For boxplot, collect all data points
                        Lgr_Lub_all_data.append(merged_data['Lgr_divided_by_Lub'].values)
                    else:
                        # Calculate statistics
                        ratio_stats = merged_data['Lgr_divided_by_Lub'].agg(['mean', 'min', 'max'])
                        
                        R_plot_data.append(R)
                        Lgr_Lub_means.append(ratio_stats['mean'])
                        Lgr_Lub_mins.append(ratio_stats['min'])
                        Lgr_Lub_maxs.append(ratio_stats['max'])
                
                if show_boxplot:
                    if Lgr_Lub_all_data:
                        # Draw boxplot (widen the width)
                        bp = ax.boxplot(Lgr_Lub_all_data, positions=R_values[:len(Lgr_Lub_all_data)], 
                                      patch_artist=True, widths=calc_widths_for_boxplot(R_values))
                        
                        # Set boxplot color (white)
                        for patch in bp['boxes']:
                            patch.set_facecolor('white')
                            patch.set_alpha(1.0)
                        
                        # Make boxplot lines thicker
                        for element in ['boxes', 'whiskers', 'fliers', 'means', 'medians']:
                            plt.setp(bp[element], linewidth=2)
                        
                        # Output the number of samples for each boxplot
                        if VERBOSE:
                            for k, data in enumerate(Lgr_Lub_all_data):
                                # Use ARTIFICIAL_DATASET_NAMES for artificial datasets
                                if dataset_name in ['uniform', 'normal', 'exponential']:
                                    dataset_label = ARTIFICIAL_DATASET_NAMES.get((dataset_name, data_type), (999, f"{dataset_name.title()}"))[1]
                                else:
                                    # Use DATASET_ORDER for real datasets
                                    dataset_label = DATASET_ORDER.get((dataset_name, data_type), (999, f"{dataset_name.title()}"))[1]
                                print(f"Boxplot - {dataset_label}, n={n_val}, R={R_values[k]}: {len(data)} samples")
                else:
                    if not R_plot_data:
                        print(f"No valid data for {dataset_name}, {data_type}, n={n_val}")
                        continue
                    
                    # Plot
                    ax.plot(R_plot_data, Lgr_Lub_means, 'o-', label='Mean', color='blue', linewidth=2)
                    ax.plot(R_plot_data, Lgr_Lub_mins, 'x:', label='Min', color='blue', linewidth=2)
                    ax.plot(R_plot_data, Lgr_Lub_maxs, 'x-.', label='Max', color='blue', linewidth=2)
                
                # Output the number of samples for each point
                if VERBOSE and not show_boxplot:
                    for i_, R in enumerate(R_values[:len(Lgr_Lub_all_data) if show_boxplot else len(R_plot_data)]):
                        # Calculate the number of samples for the corresponding lambda value
                        lambda_val = merged_data.iloc[i_]['lambda'] if i_ < len(merged_data) else 0
                        n_samples = len(merged_data[merged_data['lambda'] == lambda_val]) if lambda_val > 0 else 0
                        print(f"Upper Bound - {DATASET_ORDER.get((dataset_name, data_type), (999, ''))[1]}, n={n_val}, R={R}: {n_samples} samples")
            
            # Common settings
            if show_boxplot:
                # For boxplot, do not use log scale for y-axis
                ax.set_xlim(min(R_values) / 1.5, max(R_values) * 1.5)
                ax.set_ylim(0.55, 1.05)
            else:
                # For mean/min/max, use the default settings
                ax.set_ylim(0.55, 1.05)
            
            ax.set_xscale('log')
            ax.grid(True, which='both', linestyle='--', linewidth=0.8)
            ax.tick_params(axis='both', labelsize=TICK_SIZE)
            ax.set_xlabel('R', fontsize=FONT_SIZE)
            if i == 0:
                # Use ARTIFICIAL_DATASET_NAMES for artificial datasets
                title = ARTIFICIAL_DATASET_NAMES.get((dataset_name, data_type), (999, f"{dataset_name.title()}"))[1]
                ax.set_title(title, fontsize=FONT_SIZE)
            
            if j == 0:
                if len(n_values) == 1:
                    ax.set_ylabel(r'$\frac{\mathrm{MSE}_\mathrm{G}}{\mathrm{MSE}_\mathrm{UB}}$', fontsize=FONT_SIZE*1.5)
                else:
                    ax.set_ylabel(rf'$\mathbf{{n={n_val}}}$' + '\n' + r'$\frac{\mathrm{MSE}_\mathrm{G}}{\mathrm{MSE}_\mathrm{UB}}$', fontsize=FONT_SIZE*1.5)

        # If there are few available combinations, hide empty graphs
        for j in range(len(available_combinations), max_combinations):
            ax = axes[i][j]
            ax.set_visible(False)
    
    # Place legend in the lower right corner of the graph (if applicable)
    if not show_boxplot:
        # Find a valid graph and place legend
        for i in range(len(n_values) - 1, -1, -1):
            for j in range(max_combinations - 1, -1, -1):
                if axes[i][j].get_visible():
                    handles, labels = axes[i][j].get_legend_handles_labels()
                    if handles and labels:
                        axes[i][j].legend(handles, labels, 
                                          loc='lower right',
                                          frameon=True,
                                          facecolor='white',
                                          edgecolor='black',
                                          fontsize=LEGEND_SIZE)
                        break
            else:
                continue
            break
    
    plt.tight_layout()
    os.makedirs(os.path.dirname(fig_path), exist_ok=True)
    plt.savefig(fig_path, bbox_inches='tight')
    plt.close()
    
    print(f"Saved {fig_path}")

if __name__ == '__main__':
    # Set output directory to results/figures/R_plots
    output_dir = "../results/figures/R_LgrLub"
    os.makedirs(output_dir, exist_ok=True)
    
    # Data directory (poisoning folder)
    data_dir = ".."
    df_results = load_loss(data_dir)
    df_upper_bound = load_upper_bound(data_dir)
    df_upper_bound = df_upper_bound[df_upper_bound['algorithm'] == 'binary_search']
    df_upper_bound = df_upper_bound.drop(columns=['algorithm'])
    
    # Get available n and percentage combinations
    df_results = df_results[df_results['lambda'] > 0].copy()
    df_results['percentage'] = df_results['lambda'] / df_results['n'] * 100

    # all plot (including multiple n values)
    ns = [1000]
    Rs = [2000, 3000, 4000, 5000, 7000, 10000, 20000, 50000, 100000, 200000, 500000, 1000000]
    df_results = df_results[df_results['n'].isin(ns) & df_results['R'].isin(Rs)]
    df_upper_bound = df_upper_bound[df_upper_bound['n'].isin(ns) & df_upper_bound['R'].isin(Rs)]
    
    # Get all percentage values (use the same percentage value for each n value)
    df_results = df_results[df_results['lambda'] > 0].copy()
    df_results['percentage'] = df_results['lambda'] / df_results['n'] * 100
    
    # Plot based on settings
    percentage_value = 10
    if BOXPLOT:
        plot_R_LgrLub(df_results, df_upper_bound, f'{output_dir}/R_LgrLub_all.pdf', n_value=None, percentage_value=percentage_value, show_boxplot=True)
    else:
        plot_R_LgrLub(df_results, df_upper_bound, f'{output_dir}/R_LgrLub_all.pdf', n_value=None, percentage_value=percentage_value, show_boxplot=False)
