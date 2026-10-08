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
    LOSS_COLUMN, UPPER_BOUND_COLUMN, DATASET_NAMES_BRUTE_FORCE,
    BOXPLOT, ERROR_BARS
)

VERBOSE = False

# Boxplot spacing and width configuration
BOXPLOT_WIDTH = 1.0    # Width of each boxplot
BOXPLOT_ALPHA = 1.0
BOXPLOT_LINEWIDTH = 2
ROW_HEIGHT = 2.5
COL_WIDTH = 5

XTICK_INTERVAL = 2

def plot_lambda_LgrLub_brute_force(df_results, df_upper_bound, df_optimal_poison, fig_path, n = None, algorithm = None, show_boxplot=None):
    """
    Plot for each n. The x-axis of each graph is lambda. The y-axis of each graph displays three boxplots:
    1. Lop,dup_allowed/Lub (brute force duplicate allowed / upper bound)
    2. Lop/Lub (brute force / upper bound)
    3. Lup/Lub (greedy / upper bound)
    
    Args:
        df_results: Result dataframe (greedy poisoning)
        df_upper_bound: Upper bound dataframe
        df_optimal_poison: Optimal poison dataframe (brute force results)
        fig_path: Output file path
        n: Plot only specific n values
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

    available_combinations = [
        (name, dtype, R) for name, dtype, R in df_results[['dataset_name', 'data_type', 'R']].drop_duplicates().itertuples(index=False)
        if (name, dtype, R) in DATASET_NAMES_BRUTE_FORCE
    ]
    distributions = sorted(available_combinations, key=lambda x: DATASET_NAMES_BRUTE_FORCE[x][0])
    
    n_values = sorted(df_results['n'].unique())
    if len(n_values) > 1:
        print(f"Warning: Multiple n values found: {n_values}. Using only the first one: {n_values[0]}")
        n_values = [n_values[0]]

    plt.rcParams['figure.figsize'] = [COL_WIDTH * len(distributions) + 0.5, ROW_HEIGHT * 3]
    fig, axes = plt.subplots(3, len(distributions))

    # Handle case where axes is 1D
    if len(distributions) == 1:
        axes = axes.reshape(-1, 1)

    ratio_types = [
        ('Lup_divided_by_Lop', r'$\rho_{\mathrm{G}}$'),
        ('Lop_divided_by_Lop_dup', r'$\rho_{\mathrm{R}}$'),
        ('Lop_dup_divided_by_Lub', r'$\rho_{\mathrm{UB}}$')
    ]

    ylim = (0.88, 1.02)
    min_ratio = 1.0

    min_ratio_results = []

    # Settings for Lgr/Lub ratio plot
    for j, dist in enumerate(distributions):
        for i, (ratio_type, ratio_label) in enumerate(ratio_types):
            ax = axes[i][j]
            dataset_name, data_type, R = dist
            n = n_values[0]
            
            # Get greedy poisoning data
            poison_data = df_results[(df_results['dataset_name'] == dataset_name) & 
                                   (df_results['data_type'] == data_type) & 
                                   (df_results['n'] == n) & 
                                   (df_results['lambda'] > 0) & 
                                   (df_results['R'] == R)]
            
            # Get upper bound data
            upper_bound_data = df_upper_bound[(df_upper_bound['dataset_name'] == dataset_name) & 
                                            (df_upper_bound['data_type'] == data_type) & 
                                            (df_upper_bound['n'] == n) & 
                                            (df_upper_bound['R'] == R)]
            
            # Get brute force data (duplicate not allowed)
            brute_force_data = df_optimal_poison[(df_optimal_poison['dataset_name'] == dataset_name) & 
                                               (df_optimal_poison['data_type'] == data_type) & 
                                               (df_optimal_poison['n'] == n) & 
                                               (df_optimal_poison['R'] == R) & 
                                               (df_optimal_poison['algorithm'] == 'brute_force')]
            
            # Get brute force data (duplicate allowed)
            brute_force_dup_data = df_optimal_poison[(df_optimal_poison['dataset_name'] == dataset_name) & 
                                                    (df_optimal_poison['data_type'] == data_type) & 
                                                    (df_optimal_poison['n'] == n) & 
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
            if (merged_data['loss_greedy'].isna().sum() > 0 or merged_data['upper_bound'].isna().sum() > 0 or
                merged_data['loss_brute_force'].isna().sum() > 0 or merged_data['loss_brute_force_dup'].isna().sum() > 0):
                print(f"Warning: Missing data for {dataset_name}, {data_type}, n={n}, R={R}.")
                if merged_data['loss_greedy'].isna().sum() > 0:
                    print(f"merged_data's loss_greedy is nan")
                if merged_data['upper_bound'].isna().sum() > 0:
                    print(f"merged_data's upper_bound is nan")
                if merged_data['loss_brute_force'].isna().sum() > 0:
                    print(f"merged_data's loss_brute_force is nan")
                if merged_data['loss_brute_force_dup'].isna().sum() > 0:
                    print(f"merged_data's loss_brute_force_dup is nan")

                continue

            # Convert lambda to percentage
            merged_data['percentage'] = merged_data['lambda'] / n * 100
            
            # Calculate ratios
            merged_data['Lup_divided_by_Lop'] = merged_data['loss_greedy'] / merged_data['loss_brute_force']
            merged_data['Lop_divided_by_Lop_dup'] = merged_data['loss_brute_force'] / merged_data['loss_brute_force_dup']
            merged_data['Lop_dup_divided_by_Lub'] = merged_data['loss_brute_force_dup'] / merged_data['upper_bound']

            if show_boxplot:
                # Boxplot version
                all_percentages = sorted(merged_data['percentage'].unique())
                
                # Prepare data for boxplots
                box_data = []
                box_positions = []
                
                for percentage in all_percentages:
                    ratio_group = merged_data[merged_data['percentage'] == percentage][ratio_type].values
                    if len(ratio_group) > 0:
                        box_data.append(ratio_group)
                    else:
                        box_data.append([])

                    box_positions.append(percentage)
                    min_ratio = min(min_ratio, min(ratio_group))
                
                if box_positions:
                    # Draw boxplots
                    bp = ax.boxplot(box_data, positions=box_positions, patch_artist=True, widths=BOXPLOT_WIDTH)
                    
                    # Set color of boxplot (white fill)
                    for patch in bp['boxes']:
                        patch.set_facecolor('white')
                        patch.set_alpha(BOXPLOT_ALPHA)
                    
                    # Make boxplot lines thicker
                    for element in ['boxes', 'whiskers', 'fliers', 'means', 'medians']:
                        plt.setp(bp[element], linewidth=BOXPLOT_LINEWIDTH)
                    
                    # Set x-axis
                    ax.set_xlim(min(box_positions) - 1.0, max(box_positions) + 1.0)
                    # Display x-axis labels that are multiples of XTICK_INTERVAL
                    xticks_positions = [i_ * XTICK_INTERVAL for i_ in range(int(min(box_positions) // XTICK_INTERVAL), int(max(box_positions) // XTICK_INTERVAL) + 1)]
                    ax.set_xticks(xticks_positions)
                    ax.set_xticklabels([f'{x}' for x in xticks_positions])
                    
                    # Output number of samples for each boxplot
                    if VERBOSE:
                        for k, data in enumerate(box_data):
                            if len(data) > 0:
                                print(f"Boxplot {k} - {DATASET_NAMES_BRUTE_FORCE[dist][1]}, n={n}, percentage={box_positions[k]:.1f}%: {len(data)} samples")
                    
                    # Find and output the seed with minimum ratio for each percentage
                    for percentage in all_percentages:
                        percentage_data = merged_data[merged_data['percentage'] == percentage]
                        if len(percentage_data) > 0:
                            min_ratio_idx = percentage_data[ratio_type].idxmin()
                            min_ratio_seed = percentage_data.loc[min_ratio_idx, 'seed']
                            min_ratio_value = percentage_data.loc[min_ratio_idx, ratio_type]
                            min_ratio_results.append({
                                'Dataset': dist[0],
                                'Ratio Type': ratio_label,
                                'n': n,
                                'Percentage (%)': f"{percentage:.1f}",
                                'Seed': min_ratio_seed,
                                'Min Ratio': f"{min_ratio_value:.6f}"
                            })

            else:
                # Mean/Min/Max version (not implemented for this plot)
                print("Mean/Min/Max version not implemented for brute force comparison plot")
                continue

            # Common settings
            if show_boxplot:
                # For boxplot, do not use log scale for y-axis
                ax.set_ylim(ylim)
            else:
                # For mean/min/max, use the default settings
                ax.set_ylim(ylim)
            
            ax.grid(True, which='both', linestyle='--', linewidth=0.8)
            ax.tick_params(axis='both', labelsize=TICK_SIZE)
            if i == len(ratio_types) - 1:
                ax.set_xlabel('Poisoning Percentage', fontsize=XLABEL_SIZE)
            if i == 0:
                ax.set_title(DATASET_NAMES_BRUTE_FORCE[dist][1], fontsize=FONT_SIZE)
            if j == 0:
                ax.set_ylabel(ratio_label, fontsize=FONT_SIZE)

    assert min_ratio >= ylim[0], f"min_ratio {min_ratio} is less than ylim[0] {ylim[0]}"
    print(f"min_ratio: {min_ratio}")

    # Create and display table for current dataset and ratio type
    if min_ratio_results:
        min_ratio_df = pd.DataFrame(min_ratio_results)
        min_ratio_df = min_ratio_df[min_ratio_df['Percentage (%)'] == '10.0']
        
        # Get unique ratio types and datasets
        ratio_types = min_ratio_df['Ratio Type'].unique()
        datasets = min_ratio_df['Dataset'].unique()
        
        # Create pivot table
        for ratio_type in ratio_types:
            print(f"=== {ratio_type} ===")
            row = {}
            for dataset in datasets:
                dataset_data = min_ratio_df[(min_ratio_df['Ratio Type'] == ratio_type) & 
                                          (min_ratio_df['Dataset'] == dataset)]
                if len(dataset_data) > 0:
                    seed = dataset_data.iloc[0]['Seed']
                    ratio = dataset_data.iloc[0]['Min Ratio']
                    row[dataset] = f"({seed}, {ratio})"
                else:
                    row[dataset] = "N/A"
            df = pd.DataFrame([row])
            print(df.to_string(index=False))
            print()

    plt.tight_layout()
    os.makedirs(os.path.dirname(fig_path), exist_ok=True)
    plt.savefig(fig_path, bbox_inches='tight')
    plt.close()

    print(f"Saved {fig_path}")

if __name__ == '__main__':
    # Set output directory to results/figures
    output_dir = "../results/figures/lambda_L_ratios_brute_force"
    os.makedirs(output_dir, exist_ok=True)

    # Data directory (poisoning folder)
    data_dir = ".."
    df_results = load_loss(data_dir)
    df_upper_bound = load_upper_bound(data_dir)
    df_optimal_poison = load_optimal_poison(data_dir)

    # Get available n values
    # available_n_values = sorted(df_results['n'].unique())
    # for n in available_n_values:
    #     plot_lambda_LgrLub_brute_force(df_results, df_upper_bound, df_optimal_poison, f'{output_dir}/lambda_Lgr_divided_by_Lub_brute_force_n{n}.pdf', n = n, algorithm = "binary_search")

    ns = [50]
    df_results_ns = df_results[(df_results['n'].isin(ns))]
    df_upper_bound_ns = df_upper_bound[(df_upper_bound['n'].isin(ns))]
    df_optimal_poison_ns = df_optimal_poison[(df_optimal_poison['n'].isin(ns))]
    
    # Plot based on settings
    if BOXPLOT:
        plot_lambda_LgrLub_brute_force(df_results_ns, df_upper_bound_ns, df_optimal_poison_ns, f'{output_dir}/lambda_L_ratios_brute_force_all.pdf', n = None, algorithm = "binary_search", show_boxplot=True)
    else:
        plot_lambda_LgrLub_brute_force(df_results_ns, df_upper_bound_ns, df_optimal_poison_ns, f'{output_dir}/lambda_L_ratios_brute_force_all.pdf', n = None, algorithm = "binary_search", show_boxplot=False) 