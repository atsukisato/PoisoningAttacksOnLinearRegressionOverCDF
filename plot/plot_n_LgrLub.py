import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import os
from load_loss import load_loss
from load_upper_bound import load_upper_bound
import numpy as np
from plot_config import (
    TICK_SIZE, LEGEND_SIZE, FONT_SIZE, LOSS_COLUMN, UPPER_BOUND_COLUMN,
    DATASET_ORDER, DATASET_NAMES, get_title, calc_widths_for_boxplot, BOXPLOT, 
    MIN_N, MAX_N
)

VERBOSE = False
ROW_HEIGHT = 3.5
COLUMN_WIDTH = 5

def plot_n_LgrLub(df_results, df_upper_bound, fig_path, R_value=None, percentage_value=None, show_boxplot=False):
    """
    Plot the ratio of Lgr/Lub on the x-axis and n on the y-axis.
    Display the data for each dataset/data_type separately for specific R and percentage values.
    If show_boxplot=True, plot boxplots; if show_boxplot=False, plot mean/min/max.
    """
    # Filter by specified R value
    if R_value is not None:
        df_results = df_results[df_results['R'] == R_value]
        df_upper_bound = df_upper_bound[df_upper_bound['R'] == R_value]

    # Use only data where lambda > 0
    df_results = df_results[df_results['lambda'] > 0]
    df_upper_bound = df_upper_bound[df_upper_bound['lambda'] > 0]

    # Calculate percentage
    df_results = df_results.copy()
    df_results['percentage'] = df_results['lambda'] / df_results['n'] * 100
    df_upper_bound = df_upper_bound.copy()
    df_upper_bound['percentage'] = df_upper_bound['lambda'] / df_upper_bound['n'] * 100

    # Filter by specified percentage value (considering tolerance)
    if percentage_value is not None:
        tolerance = 0.01
        df_results = df_results[abs(df_results['percentage'] - percentage_value) < tolerance]
        df_upper_bound = df_upper_bound[abs(df_upper_bound['percentage'] - percentage_value) < tolerance]

    # If no data is available, exit
    if df_results.empty or df_upper_bound.empty:
        print(f"No data available for R={R_value}, percentage={percentage_value}%")
        return

    # Get available combinations (dataset/data_type)
    df_results_filtered = df_results
    
    # Fixed distribution order (blank if exponential is not present)
    fixed_dataset_order = sorted(DATASET_ORDER.keys(), key=lambda x: DATASET_ORDER[x][0])
    
    available_combinations = []
    for name, dtype in fixed_dataset_order:
            
        # Get available n values for this dataset/data_type combination
        available_ns = sorted(df_results_filtered[(df_results_filtered['dataset_name'] == name) & (df_results_filtered['data_type'] == dtype)]['n'].unique())
        if len(available_ns) >= 1:  # Plot if at least one n value exists
            available_combinations.append((name, dtype, available_ns))
        else:
            # Add empty combination if no data exists
            available_combinations.append((name, dtype, []))

    if not available_combinations:
        print(f"No distributions with n values for R={R_value}, percentage={percentage_value}%")
        return

    # Sort and order datasets
    # available_combinations = sorted(available_combinations, key=lambda x: DATASET_ORDER.get((x[0], x[1]), (999, "Unknown"))[0])

    # Get percentage values
    percentage_values = sorted(df_results['percentage'].unique())

    # Plot settings
    plt.rcParams['figure.figsize'] = [COLUMN_WIDTH * len(available_combinations) + 0.5, ROW_HEIGHT * len(percentage_values)]
    fig, axes = plt.subplots(len(percentage_values), len(available_combinations))

    # Process if axes is one-dimensional
    if len(percentage_values) == 1:
        if len(available_combinations) == 1:
            axes = [[axes]]
        else:
            axes = axes.reshape(1, -1)
    elif len(available_combinations) == 1:
        axes = axes.reshape(-1, 1)

    for i, percentage_val in enumerate(percentage_values):
        for j, (dataset_name, data_type, n_values) in enumerate(available_combinations):
            ax = axes[i][j]
            
            # Create empty plot if no data exists
            if not n_values:
                ax.set_xlim(100, 10000)  # Default x-axis range
                ax.set_ylim(0.55, 1.05)
                ax.set_xscale('log')
                ax.grid(True, which='both', linestyle='--', linewidth=0.8)
                ax.tick_params(axis='both', labelsize=TICK_SIZE)
                ax.set_xlabel('n', fontsize=FONT_SIZE)
                if i == 0:
                    title = DATASET_ORDER.get((dataset_name, data_type), (999, f"{dataset_name.title()}"))[1]
                    ax.set_title(title, fontsize=FONT_SIZE)
                if j == 0:
                    if len(percentage_values) == 1:
                        ax.set_ylabel(r'$\frac{\mathrm{MSE}_\mathrm{G}}{\mathrm{MSE}_\mathrm{UB}}$', fontsize=FONT_SIZE*1.5)
                    else:
                        ax.set_ylabel(rf'$\mathbf{{Poisoning:~{percentage_val:.1f}\%}}$' + '\n' + r'$\frac{\mathrm{MSE}_\mathrm{G}}{\mathrm{MSE}_\mathrm{UB}}$', fontsize=FONT_SIZE*1.5)
                continue

            # Filter by specific percentage value
            tolerance = 0.01
            df_results_percentage = df_results_filtered[abs(df_results_filtered['percentage'] - percentage_val) < tolerance]
            df_upper_bound_percentage = df_upper_bound[abs(df_upper_bound['percentage'] - percentage_val) < tolerance]

            # Handle different algorithms
            upper_bound_data = df_upper_bound_percentage[
                (df_upper_bound_percentage['dataset_name'] == dataset_name) &
                (df_upper_bound_percentage['data_type'] == data_type)
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
                    # Calculate Lgr/Lub for each n value
                    n_plot_data = []
                    Lgr_Lub_means = []
                    Lgr_Lub_mins = []
                    Lgr_Lub_maxs = []
                    Lgr_Lub_all_data = []  # For boxplot

                    for n in n_values:
                        poison_data = df_results_percentage[
                            (df_results_percentage['dataset_name'] == dataset_name) &
                            (df_results_percentage['data_type'] == data_type) &
                            (df_results_percentage['n'] == n)
                        ][['lambda', 'seed', LOSS_COLUMN]].sort_values(by=['lambda', 'seed'])

                        alg_upper_bound_data = upper_bound_data[
                            (upper_bound_data['algorithm'] == alg) &
                            (upper_bound_data['n'] == n)
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

                            n_plot_data.append(n)
                            Lgr_Lub_means.append(ratio_stats['mean'])
                            Lgr_Lub_mins.append(ratio_stats['min'])
                            Lgr_Lub_maxs.append(ratio_stats['max'])

                    if show_boxplot:
                        if Lgr_Lub_all_data:
                            # Draw boxplot (widen the width)
                            bp = ax.boxplot(Lgr_Lub_all_data, positions=n_values[:len(Lgr_Lub_all_data)], 
                                          patch_artist=True, widths=calc_widths_for_boxplot(n_values))
                            
                            # Set boxplot color (white fill)
                            for patch in bp['boxes']:
                                patch.set_facecolor('white')
                                patch.set_alpha(1.0)
                            
                            # Make boxplot lines thicker
                            for element in ['boxes', 'whiskers', 'fliers', 'means', 'medians']:
                                plt.setp(bp[element], linewidth=2)
                            
                            # Output the number of samples for each boxplot
                            if VERBOSE:
                                for k, data in enumerate(Lgr_Lub_all_data):
                                    print(f"Boxplot ({alg}) - {get_title(dataset_name, data_type, R_value)}, n={n_values[k]}, percentage={percentage_val:.1f}%: {len(data)} samples")
                    else:
                        if not n_plot_data:
                            print(f"No valid data for {dataset_name}, {data_type}, algorithm={alg}, percentage={percentage_val}")
                            continue

                        # Plot
                        color = colors.get(alg, 'blue')
                        marker = markers.get(alg, 'o')
                        min_marker = min_markers.get(alg, 'v')
                        max_marker = max_markers.get(alg, '*')
                        linestyle = linestyles.get(alg, '-')
                        label_prefix = labels.get(alg, alg)
                        
                        ax.plot(n_plot_data, Lgr_Lub_means, marker + linestyle, label=f'Mean ({label_prefix})', color=color, linewidth=2)
                        ax.plot(n_plot_data, Lgr_Lub_mins, min_marker + ':', label=f'Min ({label_prefix})', color=color, linewidth=2)
                        ax.plot(n_plot_data, Lgr_Lub_maxs, max_marker + '-.', label=f'Max ({label_prefix})', color=color, linewidth=2)
                    
                    # Output the number of samples for each point
                    if VERBOSE and not show_boxplot:
                        for i_, n in enumerate(n_values[:len(Lgr_Lub_all_data) if show_boxplot else len(n_plot_data)]):
                            # merged_data does not have 'n' column, so calculate from original data
                            n_samples = len(poison_data[poison_data['lambda'] == poison_data.iloc[i_]['lambda']]) if i_ < len(poison_data) else 0
                            print(f"Upper Bound ({alg}) - {get_title(dataset_name, data_type, R_value)}, n={n}, percentage={percentage_val:.1f}%: {n_samples} samples")
            else:
                # Legacy format without algorithm column
                # Calculate Lgr/Lub for each n value
                n_plot_data = []
                Lgr_Lub_means = []
                Lgr_Lub_mins = []
                Lgr_Lub_maxs = []
                Lgr_Lub_all_data = []  # For boxplot

                for n in n_values:
                    poison_data = df_results_percentage[
                        (df_results_percentage['dataset_name'] == dataset_name) &
                        (df_results_percentage['data_type'] == data_type) &
                        (df_results_percentage['n'] == n)
                    ][['lambda', 'seed', LOSS_COLUMN]].sort_values(by=['lambda', 'seed'])

                    alg_upper_bound_data = upper_bound_data[
                        (upper_bound_data['n'] == n)
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

                        n_plot_data.append(n)
                        Lgr_Lub_means.append(ratio_stats['mean'])
                        Lgr_Lub_mins.append(ratio_stats['min'])
                        Lgr_Lub_maxs.append(ratio_stats['max'])

                if show_boxplot:
                    if Lgr_Lub_all_data:
                        # Draw boxplot (widen the width)
                        bp = ax.boxplot(Lgr_Lub_all_data, positions=n_values[:len(Lgr_Lub_all_data)], 
                                      patch_artist=True, widths=calc_widths_for_boxplot(n_values))
                        
                        # Set boxplot color (white fill)
                        for patch in bp['boxes']:
                            patch.set_facecolor('white')
                            patch.set_alpha(1.0)
                        
                        # Make boxplot lines thicker
                        for element in ['boxes', 'whiskers', 'fliers', 'means', 'medians']:
                            plt.setp(bp[element], linewidth=2)
                        
                        # Output the number of samples for each boxplot
                        if VERBOSE:
                            for k, data in enumerate(Lgr_Lub_all_data):
                                print(f"Boxplot - {get_title(dataset_name, data_type, R_value)}, n={n_values[k]}, percentage={percentage_val:.1f}%: {len(data)} samples")
                else:
                    if not n_plot_data:
                        print(f"No valid data for {dataset_name}, {data_type}, percentage={percentage_val}")
                        continue

                    # Plot
                    ax.plot(n_plot_data, Lgr_Lub_means, 'o-', label='Mean', color='blue', linewidth=2)
                    ax.plot(n_plot_data, Lgr_Lub_mins, 'x:', label='Min', color='blue', linewidth=2)
                    ax.plot(n_plot_data, Lgr_Lub_maxs, 'x-.', label='Max', color='blue', linewidth=2)
                
                # Output the number of samples for each point
                if VERBOSE and not show_boxplot:
                    for i_, n in enumerate(n_values[:len(Lgr_Lub_all_data) if show_boxplot else len(n_plot_data)]):
                        # merged_data does not have 'n' column, so calculate from original data
                        n_samples = len(poison_data[poison_data['lambda'] == poison_data.iloc[i_]['lambda']]) if i_ < len(poison_data) else 0
                        print(f"Upper Bound - {get_title(dataset_name, data_type, R_value)}, n={n}, percentage={percentage_val:.1f}%: {n_samples} samples")

            # Common settings
            if show_boxplot:
                # For boxplot, do not use log scale for y-axis
                ax.set_xlim(min(n_values) / 1.5, max(n_values) * 1.5)
                ax.set_ylim(0.55, 1.05)
            else:
                # For mean/min/max, use the default settings
                ax.set_ylim(0.55, 1.05)
            
            ax.set_xscale('log')
            ax.grid(True, which='both', linestyle='--', linewidth=0.8)
            ax.tick_params(axis='both', labelsize=TICK_SIZE)
            ax.set_xlabel('n', fontsize=FONT_SIZE)
            if i == 0:
                ax.set_title(get_title(dataset_name, data_type, R_value), fontsize=FONT_SIZE)

            if j == 0:
                if len(percentage_values) == 1:
                    ax.set_ylabel(r'$\frac{\mathrm{MSE}_\mathrm{G}}{\mathrm{MSE}_\mathrm{UB}}$', fontsize=FONT_SIZE*1.5)
                else:
                    ax.set_ylabel(rf'$\mathbf{{Poisoning:~{percentage_val:.1f}\%}}$' + '\n' + r'$\frac{\mathrm{MSE}_\mathrm{G}}{\mathrm{MSE}_\mathrm{UB}}$', fontsize=FONT_SIZE*1.5)

    # Place legend in the lower right corner of the graph (if applicable)
    if not show_boxplot:
        handles, labels = axes[len(percentage_values) - 1][len(available_combinations) - 1].get_legend_handles_labels()
        axes[len(percentage_values) - 1][len(available_combinations) - 1].legend(handles, labels, 
                                               loc='lower right',
                                               frameon=True,
                                               facecolor='white',
                                               edgecolor='black',
                                               fontsize=LEGEND_SIZE)

    plt.tight_layout()
    plt.savefig(fig_path, bbox_inches='tight')
    plt.close()

    print(f"Saved {fig_path}")

if __name__ == '__main__':
    # Set output directory to results/figures/n_plots
    output_dir = "../results/figures/n_LgrLub"
    os.makedirs(output_dir, exist_ok=True)

    # Data directory (poisoning folder)
    data_dir = ".."
    df_results = load_loss(data_dir)
    df_upper_bound = load_upper_bound(data_dir)

    # Get available R and percentage combinations
    df_results = df_results[(df_results['lambda'] > 0)].copy()
    df_results['percentage'] = df_results['lambda'] / df_results['n'] * 100
    df_upper_bound = df_upper_bound[(df_upper_bound['lambda'] > 0)].copy()
    df_upper_bound['percentage'] = df_upper_bound['lambda'] / df_upper_bound['n'] * 100
    
    # Filter only datasets in DATASET_ORDER
    valid_datasets = set()
    for (name, dtype) in DATASET_ORDER.keys():
        valid_datasets.add((name, dtype))
    
    df_results = df_results[df_results.apply(lambda row: (row['dataset_name'], row['data_type']) in valid_datasets, axis=1)]
    df_upper_bound = df_upper_bound[df_upper_bound.apply(lambda row: (row['dataset_name'], row['data_type']) in valid_datasets, axis=1)]

    Rs = [0, 100000]
    percentages = [10]
    df_results = df_results[(df_results['R'].isin(Rs)) & (df_results['percentage'].isin(percentages)) & (MIN_N <= df_results['n']) & (df_results['n'] <= MAX_N)]
    df_upper_bound = df_upper_bound[(df_upper_bound['R'].isin(Rs)) & (df_upper_bound['percentage'].isin(percentages)) & (MIN_N <= df_upper_bound['n']) & (df_upper_bound['n'] <= MAX_N)]

    df_upper_bound = df_upper_bound[df_upper_bound['algorithm'] == 'binary_search']
    df_upper_bound = df_upper_bound.drop(columns=['algorithm'])

    # Plot based on settings
    if BOXPLOT:
        plot_n_LgrLub(df_results, df_upper_bound, f"{output_dir}/n_LgrLub_all.pdf", None, None, show_boxplot=True)
    else:
        plot_n_LgrLub(df_results, df_upper_bound, f"{output_dir}/n_LgrLub_all.pdf", None, None, show_boxplot=False)
