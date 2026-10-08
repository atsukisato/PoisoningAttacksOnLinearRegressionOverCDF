import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import os
from load_loss import load_loss
from load_upper_bound import load_upper_bound
from load_optimal_poison import load_optimal_poison
from plot_config import (
    TICK_SIZE, LEGEND_SIZE, XLABEL_SIZE, FONT_SIZE,
    LOSS_COLUMN, UPPER_BOUND_COLUMN, ARTIFICIAL_DATASET_NAMES,
    BOXPLOT, ERROR_BARS, calc_widths_for_boxplot
)

VERBOSE = False

# Boxplot spacing and width configuration
BOXPLOT_WIDTH = 1.0    # Width of each boxplot
BOXPLOT_ALPHA = 1.0
BOXPLOT_LINEWIDTH = 2
ROW_HEIGHT = 2.5
COL_WIDTH = 5

# Fixed axis limits so empty panels keep the same scale as filled ones
XLIM = (100 / 1.5, 10000 * 1.5)
YLIM = (0.65, 1.05)


def _style_axis(ax, *, ratio_label, title, show_xlabel, show_ylabel, show_title):
    """Apply shared axis styling even when there is no data to plot."""
    ax.set_xlim(XLIM)
    ax.set_ylim(YLIM)
    ax.set_xscale('log')
    ax.grid(True, which='both', linestyle='--', linewidth=0.8)
    ax.tick_params(axis='both', labelsize=TICK_SIZE)
    if show_xlabel:
        ax.set_xlabel('R', fontsize=XLABEL_SIZE)
    if show_title:
        ax.set_title(title, fontsize=FONT_SIZE)
    if show_ylabel:
        ax.set_ylabel(ratio_label, fontsize=FONT_SIZE)


def plot_R_L_ratios_brute_force(df_results, df_upper_bound, df_optimal_poison, fig_path, n=None, percentage=None, algorithm=None, show_boxplot=None):
    """
    Plot for each ratio type. The x-axis of each graph is R. The y-axis of each graph displays boxplots for:
    1. Lop,dup_allowed/Lub (brute force duplicate allowed / upper bound)
    2. Lop/Lub (brute force / upper bound)
    3. Lup/Lop (greedy / brute force)
    
    Args:
        df_results: Result dataframe (greedy poisoning)
        df_upper_bound: Upper bound dataframe
        df_optimal_poison: Optimal poison dataframe (brute force results)
        fig_path: Output file path
        n: Plot only specific n values
        percentage: Plot only specific percentage values
        algorithm: Specify algorithm for upper bound
        show_boxplot: Specify 'boxplot' or 'scatter'
    """
    # Determine show_boxplot based on settings
    if show_boxplot is None:
        show_boxplot = BOXPLOT

    if n is not None:
        df_results = df_results[df_results['n'] == n]
        df_upper_bound = df_upper_bound[df_upper_bound['n'] == n]
        df_optimal_poison = df_optimal_poison[df_optimal_poison['n'] == n]

    if algorithm is not None:
        df_upper_bound = df_upper_bound[df_upper_bound['algorithm'] == algorithm]
        # drop algorithm column
        df_upper_bound = df_upper_bound.drop(columns=['algorithm'])

    # Use only data where lambda > 0
    df_results = df_results[df_results['lambda'] > 0]
    df_upper_bound = df_upper_bound[df_upper_bound['lambda'] > 0]
    df_optimal_poison = df_optimal_poison[df_optimal_poison['lambda'] > 0]

    # Calculate percentage
    df_results = df_results.copy()
    df_results['percentage'] = df_results['lambda'] / df_results['n'] * 100
    df_upper_bound = df_upper_bound.copy()
    df_upper_bound['percentage'] = df_upper_bound['lambda'] / df_upper_bound['n'] * 100
    df_optimal_poison = df_optimal_poison.copy()
    df_optimal_poison['percentage'] = df_optimal_poison['lambda'] / df_optimal_poison['n'] * 100

    # Filter by specified percentage value (considering tolerance)
    if percentage is not None:
        tolerance = 0.1
        df_results = df_results[abs(df_results['percentage'] - percentage) < tolerance]
        df_upper_bound = df_upper_bound[abs(df_upper_bound['percentage'] - percentage) < tolerance]
        df_optimal_poison = df_optimal_poison[abs(df_optimal_poison['percentage'] - percentage) < tolerance]

    # Always use the fixed artificial-dataset layout so empty panels keep titles/labels.
    distributions = sorted(ARTIFICIAL_DATASET_NAMES.keys(), key=lambda x: ARTIFICIAL_DATASET_NAMES[x][0])

    n_values = sorted(df_results['n'].unique()) if len(df_results) > 0 else []
    if len(n_values) > 1:
        print(f"Warning: Multiple n values found: {n_values}. Using only the first one: {n_values[0]}")
        n_values = [n_values[0]]
    plot_n = n_values[0] if n_values else (n if n is not None else 50)

    plt.rcParams['figure.figsize'] = [COL_WIDTH * 3 + 0.5, ROW_HEIGHT * 3]
    fig, axes = plt.subplots(3, 3)

    ratio_types = [
        ('Lup_divided_by_Lop', r'$\rho_{\mathrm{G}}$'),
        ('Lop_divided_by_Lop_dup', r'$\rho_{\mathrm{R}}$'),
        ('Lop_dup_divided_by_Lub', r'$\rho_{\mathrm{UB}}$')
    ]

    min_ratio = 1.0

    # Settings for R ratio plot
    for i, (ratio_type, ratio_label) in enumerate(ratio_types):
        for j, dist in enumerate(distributions[:3]):
            ax = axes[i][j]
            dataset_name, data_type = dist
            title = ARTIFICIAL_DATASET_NAMES[(dataset_name, data_type)][1]

            # Get available R values for this dataset/data_type combination
            available_Rs = sorted(df_optimal_poison[(df_optimal_poison['dataset_name'] == dataset_name) & 
                                           (df_optimal_poison['data_type'] == data_type) & 
                                           (df_optimal_poison['n'] == plot_n)]['R'].unique()) if len(df_optimal_poison) > 0 else []

            R_plot_data = []
            ratio_all_data = []  # For boxplot

            if len(available_Rs) == 0:
                print(f"No R values found for {dataset_name}, {data_type}, n={plot_n}")
            else:
                # Calculate ratio for each R value
                for R in available_Rs:
                    # Get greedy poisoning data for this R
                    poison_data = df_results[(df_results['dataset_name'] == dataset_name) & 
                                           (df_results['data_type'] == data_type) & 
                                           (df_results['n'] == plot_n) & 
                                           (df_results['R'] == R)]
                    
                    # Get upper bound data for this R
                    upper_bound_data = df_upper_bound[(df_upper_bound['dataset_name'] == dataset_name) & 
                                                    (df_upper_bound['data_type'] == data_type) & 
                                                    (df_upper_bound['n'] == plot_n) & 
                                                    (df_upper_bound['R'] == R)]
                    
                    # Get brute force data (duplicate not allowed) for this R
                    brute_force_data = df_optimal_poison[(df_optimal_poison['dataset_name'] == dataset_name) & 
                                                       (df_optimal_poison['data_type'] == data_type) & 
                                                       (df_optimal_poison['n'] == plot_n) & 
                                                       (df_optimal_poison['R'] == R) & 
                                                       (df_optimal_poison['algorithm'] == 'brute_force')]
                    
                    # Get brute force data (duplicate allowed) for this R
                    brute_force_dup_data = df_optimal_poison[(df_optimal_poison['dataset_name'] == dataset_name) & 
                                                            (df_optimal_poison['data_type'] == data_type) & 
                                                            (df_optimal_poison['n'] == plot_n) & 
                                                            (df_optimal_poison['R'] == R) & 
                                                            (df_optimal_poison['algorithm'] == 'brute_force_duplicate_allowed')]

                    # Sort data
                    poison_data = poison_data[['lambda', 'seed', LOSS_COLUMN]].sort_values(by=['lambda', 'seed'])
                    upper_bound_data = upper_bound_data[['lambda', 'seed', UPPER_BOUND_COLUMN]].sort_values(by=['lambda', 'seed'])
                    brute_force_data = brute_force_data[['lambda', 'seed', 'loss']].sort_values(by=['lambda', 'seed'])
                    brute_force_dup_data = brute_force_dup_data[['lambda', 'seed', 'loss']].sort_values(by=['lambda', 'seed'])

                    # Rename loss columns to avoid conflicts
                    poison_data_renamed = poison_data.rename(columns={LOSS_COLUMN: 'loss_greedy'})
                    brute_force_data_renamed = brute_force_data.rename(columns={'loss': 'loss_brute_force'})
                    brute_force_dup_data_renamed = brute_force_dup_data.rename(columns={'loss': 'loss_brute_force_dup'})
                    upper_bound_data_renamed = upper_bound_data.rename(columns={UPPER_BOUND_COLUMN: 'upper_bound'})
                    
                    # Merge all data into one table, starting with brute_force_data as the base
                    # This ensures only lambda, seed combinations that exist in brute_force_data are kept
                    merged_data = brute_force_data_renamed.merge(upper_bound_data_renamed, on=['lambda', 'seed'], how='inner')
                    merged_data = merged_data.merge(poison_data_renamed, on=['lambda', 'seed'], how='left')
                    merged_data = merged_data.merge(brute_force_dup_data_renamed, on=['lambda', 'seed'], how='left')
                    
                    # Check for missing data
                    if (merged_data.empty or
                        merged_data['loss_greedy'].isna().sum() > 0 or merged_data['upper_bound'].isna().sum() > 0 or
                        merged_data['loss_brute_force'].isna().sum() > 0 or merged_data['loss_brute_force_dup'].isna().sum() > 0):
                        print(f"Warning: Missing data for {dataset_name}, {data_type}, n={plot_n}, R={R}.")
                        continue

                    # Calculate ratios
                    merged_data['Lop_dup_divided_by_Lub'] = merged_data['loss_brute_force_dup'] / merged_data['upper_bound']
                    merged_data['Lop_divided_by_Lop_dup'] = merged_data['loss_brute_force'] / merged_data['loss_brute_force_dup']
                    merged_data['Lup_divided_by_Lop'] = merged_data['loss_greedy'] / merged_data['loss_brute_force']

                    if show_boxplot:
                        # For boxplot, collect all data points for this R
                        ratio_group = merged_data[ratio_type].values
                        if len(ratio_group) > 0:
                            ratio_all_data.append(ratio_group)
                            R_plot_data.append(R)
                            min_ratio = min(min_ratio, min(ratio_group))
                    else:
                        # Mean/Min/Max version (not implemented for this plot)
                        print("Mean/Min/Max version not implemented for R brute force comparison plot")
                        continue

            if show_boxplot and ratio_all_data:
                # Draw boxplot (widen the width)
                bp = ax.boxplot(ratio_all_data, positions=R_plot_data, 
                              patch_artist=True, widths=calc_widths_for_boxplot(R_plot_data))
                
                # Set boxplot color (white)
                for patch in bp['boxes']:
                    patch.set_facecolor('white')
                    patch.set_alpha(1.0)
                
                # Make boxplot lines thicker
                for element in ['boxes', 'whiskers', 'fliers', 'means', 'medians']:
                    plt.setp(bp[element], linewidth=2)
                
                # Output the number of samples for each boxplot
                if VERBOSE:
                    for k, data in enumerate(ratio_all_data):
                        print(f"Boxplot {i},{j} - {title}, n={plot_n}, R={R_plot_data[k]}: {len(data)} samples")

            # Always apply axis styling, even when this panel has no data.
            _style_axis(
                ax,
                ratio_label=ratio_label,
                title=title,
                show_xlabel=(i == len(ratio_types) - 1),
                show_ylabel=(j == 0),
                show_title=(i == 0),
            )

    assert min_ratio >= YLIM[0], f"min_ratio {min_ratio} is less than ylim[0] {YLIM[0]}"
    print(f"min_ratio: {min_ratio}")

    plt.tight_layout()
    os.makedirs(os.path.dirname(fig_path), exist_ok=True)
    plt.savefig(fig_path, bbox_inches='tight')
    plt.close()

    print(f"Saved {fig_path}")

if __name__ == '__main__':
    # Set output directory to results/figures
    output_dir = "../results/figures/R_L_ratios_brute_force"
    os.makedirs(output_dir, exist_ok=True)

    # Data directory (poisoning folder)
    data_dir = ".."
    df_results = load_loss(data_dir)
    df_upper_bound = load_upper_bound(data_dir)
    df_optimal_poison = load_optimal_poison(data_dir)

    # Filter for specific n and percentage values
    ns = [50]
    percentage_value = 10

    Rs = [100, 150, 200, 300, 500, 1000, 2000, 5000, 10000]
    
    df_results_ns = df_results[(df_results['n'].isin(ns)) & (df_results['R'].isin(Rs))]
    df_upper_bound_ns = df_upper_bound[(df_upper_bound['n'].isin(ns)) & (df_upper_bound['R'].isin(Rs))]
    df_optimal_poison_ns = df_optimal_poison[(df_optimal_poison['n'].isin(ns)) & (df_optimal_poison['R'].isin(Rs))]
    
    # Plot based on settings
    if BOXPLOT:
        plot_R_L_ratios_brute_force(df_results_ns, df_upper_bound_ns, df_optimal_poison_ns, f'{output_dir}/R_L_ratios_brute_force_all.pdf', n=None, percentage=percentage_value, algorithm="binary_search", show_boxplot=True)
    else:
        plot_R_L_ratios_brute_force(df_results_ns, df_upper_bound_ns, df_optimal_poison_ns, f'{output_dir}/R_L_ratios_brute_force_all.pdf', n=None, percentage=percentage_value, algorithm="binary_search", show_boxplot=False)
