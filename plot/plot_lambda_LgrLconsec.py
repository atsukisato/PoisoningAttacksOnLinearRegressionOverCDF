import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import os
from load_loss import load_loss
from plot_config import (
    TICK_SIZE, LEGEND_SIZE, XLABEL_SIZE, FONT_SIZE,
    LOSS_COLUMN, DATASET_NAMES, BOXPLOT, ERROR_BARS
)

VERBOSE = False
ROW_HEIGHT = 3.7
COLUMN_WIDTH = 5

XTICK_INTERVAL = 2

def plot_lambda_LgrLconsec(original_df, consecutive_w_endpoints_df, fig_path, n=None, show_boxplot=None):
    """
    Plot for each n. The x-axis of each graph is lambda. The y-axis displays MSE_G/MSE_{CONSEC_E}.
    
    Args:
        original_df: Original loss dataframe
        consecutive_w_endpoints_df: Consecutive with endpoints loss dataframe
        fig_path: Output file path
        n: Plot only specific n values
        show_boxplot: Specify 'boxplot' or 'scatter'
    """
    # Determine show_boxplot based on settings
    if show_boxplot is None:
        show_boxplot = BOXPLOT

    if n is not None:
        original_df = original_df[original_df['n'] == n]
        consecutive_w_endpoints_df = consecutive_w_endpoints_df[consecutive_w_endpoints_df['n'] == n]

    # Combine data for finding available combinations
    combined_df = pd.concat([original_df, consecutive_w_endpoints_df], ignore_index=True)
    available_combinations = [
        (name, dtype, R) for name, dtype, R in combined_df[['dataset_name', 'data_type', 'R']].drop_duplicates().itertuples(index=False)
        if (name, dtype, R) in DATASET_NAMES
    ]
    distributions = sorted(available_combinations, key=lambda x: DATASET_NAMES[x][0])

    plt.rcParams['figure.figsize'] = [COLUMN_WIDTH * len(distributions) + 0.5, ROW_HEIGHT * 1]
    fig, axes = plt.subplots(1, len(distributions))

    # Handle case where axes is 1D
    if len(distributions) == 1:
        axes = axes.reshape(1, 1)

    # Settings for Lgr/LconsecE ratio plots
    # Collect all ratios for overall statistics
    all_lgr_lconsecE_ratios = []
    y_min_0 = float('inf')
    y_max_0 = float('-inf')
    
    for j, dist in enumerate(distributions):
        dataset_name, data_type, R = dist
        
        # Get data for original and consecutive_w_endpoints approaches
        original_data = original_df[(original_df['dataset_name'] == dataset_name) & 
                                  (original_df['data_type'] == data_type) & 
                                  (original_df['n'] == 1000) & 
                                  (original_df['lambda'] > 0) & 
                                  (original_df['R'] == R)]
        
        consecutive_w_endpoints_data = consecutive_w_endpoints_df[(consecutive_w_endpoints_df['dataset_name'] == dataset_name) & 
                                                                (consecutive_w_endpoints_df['data_type'] == data_type) & 
                                                                (consecutive_w_endpoints_df['n'] == 1000) & 
                                                                (consecutive_w_endpoints_df['lambda'] > 0) & 
                                                                (consecutive_w_endpoints_df['R'] == R)]
        
        if original_data.empty or consecutive_w_endpoints_data.empty:
            print(f"Warning: Missing data for {dataset_name}, {data_type}, n=1000, R={R}")
            continue
        
        # Merge data on lambda and seed
        original_data = original_data[['lambda', 'seed', LOSS_COLUMN]].sort_values(by=['lambda', 'seed'])
        consecutive_w_endpoints_data = consecutive_w_endpoints_data[['lambda', 'seed', LOSS_COLUMN]].sort_values(by=['lambda', 'seed'])
        
        # Rename columns for merge
        original_data = original_data.rename(columns={LOSS_COLUMN: 'original_loss'})
        consecutive_w_endpoints_data = consecutive_w_endpoints_data.rename(columns={LOSS_COLUMN: 'consecutive_w_endpoints_loss'})
        
        # Merge datasets
        merged_data = original_data.merge(consecutive_w_endpoints_data, on=['lambda', 'seed'], how='inner')
        
        if merged_data.empty:
            print(f"Warning: No matching data after merge for {dataset_name}, {data_type}, n=1000, R={R}")
            continue
        
        # Convert lambda to percentage
        merged_data['percentage'] = merged_data['lambda'] / 1000 * 100
        merged_data['Lgr_divided_by_LconsecE'] = merged_data['original_loss'] / merged_data['consecutive_w_endpoints_loss']
        
        # Print minimum/maximum Lgr/LconsecE values for each lambda (percentage) and their corresponding seeds
        print(f"\n=== Minimum Lgr/LconsecE values for {dataset_name}, {data_type}, R={R} ===")
        for percentage in sorted(merged_data['percentage'].unique()):
            group_data = merged_data[merged_data['percentage'] == percentage]
            min_idx = group_data['Lgr_divided_by_LconsecE'].idxmin()
            min_row = group_data.loc[min_idx]
            print(f"  Lambda={min_row['lambda']} ({percentage:.1f}%): Min Lgr/LconsecE = {min_row['Lgr_divided_by_LconsecE']:.30f} (seed={min_row['seed']})")
            print(f"    Original loss: {min_row['original_loss']:.30f}, Consecutive with endpoints loss: {min_row['consecutive_w_endpoints_loss']:.30f}")
        
        print(f"\n=== Maximum Lgr/LconsecE values for {dataset_name}, {data_type}, R={R} ===")
        for percentage in sorted(merged_data['percentage'].unique()):
            group_data = merged_data[merged_data['percentage'] == percentage]
            max_idx = group_data['Lgr_divided_by_LconsecE'].idxmax()
            max_row = group_data.loc[max_idx]
            print(f"  Lambda={max_row['lambda']} ({percentage:.1f}%): Max Lgr/LconsecE = {max_row['Lgr_divided_by_LconsecE']:.30f} (seed={max_row['seed']})")
            print(f"    Original loss: {max_row['original_loss']:.30f}, Consecutive with endpoints loss: {max_row['consecutive_w_endpoints_loss']:.30f}")

        # Update global statistics
        all_lgr_lconsecE_ratios.extend(merged_data['Lgr_divided_by_LconsecE'].tolist())
        
        # Update global y-axis limits
        y_min_0 = min(y_min_0, merged_data['Lgr_divided_by_LconsecE'].min())
        y_max_0 = max(y_max_0, merged_data['Lgr_divided_by_LconsecE'].max())

        # Plot Lgr/LconsecE ratio
        ax = axes[j]
        if show_boxplot:
            # Boxplot version for Lgr/LconsecE
            box_data_lconsecE = []
            box_positions = []
            for percentage in sorted(merged_data['percentage'].unique()):
                group_data = merged_data[merged_data['percentage'] == percentage]['Lgr_divided_by_LconsecE'].values
                if len(group_data) > 0:
                    box_data_lconsecE.append(group_data)
                    box_positions.append(percentage)
            
            if box_data_lconsecE:
                # Draw boxplot for Lgr/LconsecE
                bp = ax.boxplot(box_data_lconsecE, positions=box_positions, patch_artist=True, widths=0.8)
                
                # Set color of boxplot (white fill)
                for patch in bp['boxes']:
                    patch.set_facecolor('white')
                    patch.set_alpha(1.0)
                
                # Make boxplot lines thicker
                for element in ['boxes', 'whiskers', 'fliers', 'means', 'medians']:
                    plt.setp(bp[element], linewidth=2)
                
                # Set x-axis
                ax.set_xlim(min(box_positions) - 1, max(box_positions) + 1)
                # Display x-axis labels that are multiples of XTICK_INTERVAL
                xticks_positions = [i_ * XTICK_INTERVAL for i_ in range(int(min(box_positions) // XTICK_INTERVAL), int(max(box_positions) // XTICK_INTERVAL) + 1)]
                ax.set_xticks(xticks_positions)
                ax.set_xticklabels([f'{x}' for x in xticks_positions])
        else:
            # Mean/Min/Max version for Lgr/LconsecE
            stats_lconsecE = merged_data.groupby('percentage')['Lgr_divided_by_LconsecE'].agg(['mean', 'min', 'max'])
            
            ax.plot(stats_lconsecE.index, stats_lconsecE['mean'], 's-', label='Mean', color='green', linewidth=2)
            ax.plot(stats_lconsecE.index, stats_lconsecE['min'], 'v:', label='Min', color='green', linewidth=2)
            ax.plot(stats_lconsecE.index, stats_lconsecE['max'], 'v-.', label='Max', color='green', linewidth=2)
        
        # Common settings for Lgr/LconsecE plot
        ax.grid(True, which='both', linestyle='--', linewidth=0.8)
        ax.tick_params(axis='both', labelsize=TICK_SIZE)
        ax.set_title(f'{DATASET_NAMES[dist][1]}', fontsize=XLABEL_SIZE)
        ax.set_xlabel('Poisoning Percentage', fontsize=XLABEL_SIZE)
        if j == 0:
            ax.set_ylabel(r'$\frac{\mathrm{MSE}_\mathrm{G}}{\mathrm{MSE}_\mathrm{SEG{+}E}}$', fontsize=FONT_SIZE*1.5)
        
        # Add horizontal line at y=1
        ax.axhline(y=1, color='red', linestyle='--', alpha=0.7)
    
    # Set consistent y-axis limits across all subplots
    # y_margin_0 = (y_max_0 - y_min_0) * 0.1
    # for j in range(len(distributions)):
    #     axes[j].set_ylim(max(0, y_min_0 - y_margin_0), y_max_0 + y_margin_0)
    for j in range(len(distributions)):
        axes[j].set_ylim(0.78, 1.02)

    # Place legend in the lower right corner of the graph (if applicable)
    if not show_boxplot:
        handles, labels = axes[len(distributions) - 1].get_legend_handles_labels()
        axes[len(distributions) - 1].legend(handles, labels, 
                                    loc='lower right',
                                    frameon=True,
                                    facecolor='white',
                                    edgecolor='black',
                                    fontsize=LEGEND_SIZE)
    
    # Print overall statistics for Lgr/LconsecE ratio
    if all_lgr_lconsecE_ratios:
        overall_min = min(all_lgr_lconsecE_ratios)
        overall_max = max(all_lgr_lconsecE_ratios)
        overall_mean = sum(all_lgr_lconsecE_ratios) / len(all_lgr_lconsecE_ratios)
        print(f"Overall Lgr/LconsecE ratio statistics:")
        print(f"  Minimum: {overall_min:.30f}")
        print(f"  Maximum: {overall_max:.30f}")
        print(f"  Mean: {overall_mean:.30f}")
        print(f"  Total samples: {len(all_lgr_lconsecE_ratios)}")

    plt.tight_layout()
    os.makedirs(os.path.dirname(fig_path), exist_ok=True)
    plt.savefig(fig_path, bbox_inches='tight')
    plt.close()

    print(f"Saved {fig_path}")



if __name__ == '__main__':
    # Set output directory to results/figures
    output_dir = "../results/figures/lambda_LgrLconsec"
    os.makedirs(output_dir, exist_ok=True)

    # Data directory (poisoning folder)
    data_dir = ".."
    
    print("Loading comparison data...")
    original_df = load_loss(data_dir)
    consecutive_w_endpoints_df = load_loss(data_dir, approach="consecutive_w_endpoints")
    
    if original_df.empty or consecutive_w_endpoints_df.empty:
        print("Error: No data loaded")
        exit(1)
    
    print(f"Loaded data:")
    print(f"  Original: {len(original_df)} rows")
    print(f"  Consecutive with endpoints: {len(consecutive_w_endpoints_df)} rows")
    
    # Filter for n=1000 only
    ns = [1000]
    original_df_ns = original_df[original_df['n'].isin(ns)]
    consecutive_w_endpoints_df_ns = consecutive_w_endpoints_df[consecutive_w_endpoints_df['n'].isin(ns)]
    
    # Plot based on settings
    if BOXPLOT:
        plot_lambda_LgrLconsec(original_df_ns, consecutive_w_endpoints_df_ns, 
                              f'{output_dir}/lambda_Lgr_divided_by_Lconsec_all.pdf', 
                              n=None, show_boxplot='boxplot')
    else:
        plot_lambda_LgrLconsec(original_df_ns, consecutive_w_endpoints_df_ns, 
                              f'{output_dir}/lambda_Lgr_divided_by_Lconsec_all.pdf', 
                              n=None, show_boxplot='scatter')
