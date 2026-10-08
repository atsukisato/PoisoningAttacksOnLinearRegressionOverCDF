import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import os
from load_query_time import load_query_time_results
from plot_config import (
    TICK_SIZE, LEGEND_SIZE, XLABEL_SIZE, FONT_SIZE, DATASET_NAMES,
    BOXPLOT, ERROR_BARS
)

VERBOSE = False
ROW_HEIGHT = 4.25
COLUMN_WIDTH = 5


def plot_lambda_query_time(df_query_time, fig_path, n_value=None, show_error_bars=None, approaches=None):
    """
    Plot poisoning percentage (lambda/n) vs average query time (microseconds per query).

    Args:
        df_query_time: DataFrame from load_query_time_results
        fig_path: output path for figure
        n_value: specific n to filter (if None, use defaults)
        show_error_bars: whether to show std as error bars
        approaches: list of approaches to include
    """
    if show_error_bars is None:
        show_error_bars = ERROR_BARS

    if approaches is not None:
        df_query_time = df_query_time[df_query_time['approach'].isin(approaches)]

    if n_value is not None:
        df_query_time = df_query_time[df_query_time['n'] == n_value]

    # Use only lambda > 0 for poisoned curves (but keep lambda==0 for reference if present)
    # We'll include lambda==0 as legitimate baseline points when available

    # Aggregate: mean and std across seeds for avg_query_time_us and
    # avg_std_lower_bound_query_time_us (the std::lower_bound variant).
    agg = df_query_time.groupby(['dataset_name', 'data_type', 'R', 'lambda', 'approach']).agg(
        avg_query_time_us_mean=pd.NamedAgg(column='avg_query_time_us', aggfunc='mean'),
        avg_query_time_us_std=pd.NamedAgg(column='avg_query_time_us', aggfunc='std'),
        avg_query_time_us_count=pd.NamedAgg(column='avg_query_time_us', aggfunc='count'),
        avg_std_lower_bound_query_time_us_mean=pd.NamedAgg(column='avg_std_lower_bound_query_time_us', aggfunc='mean'),
        avg_std_lower_bound_query_time_us_std=pd.NamedAgg(column='avg_std_lower_bound_query_time_us', aggfunc='std'),
        avg_std_lower_bound_query_time_us_count=pd.NamedAgg(column='avg_std_lower_bound_query_time_us', aggfunc='count')
    ).reset_index()

    # normalize column names used later in the plotting logic
    df_stats = agg.rename(columns={
        'avg_query_time_us_mean': 'qtime_mean_us',
        'avg_query_time_us_std': 'qtime_std_us',
        'avg_query_time_us_count': 'qtime_count',
        'avg_std_lower_bound_query_time_us_mean': 'qtime_lb_mean_us',
        'avg_std_lower_bound_query_time_us_std': 'qtime_lb_std_us',
        'avg_std_lower_bound_query_time_us_count': 'qtime_lb_count'
    })

    # Fill NaNs for std/counts to avoid plotting issues
    df_stats['qtime_std_us'] = df_stats['qtime_std_us'].fillna(0)
    df_stats['qtime_lb_std_us'] = df_stats['qtime_lb_std_us'].fillna(0)
    df_stats['qtime_count'] = df_stats['qtime_count'].fillna(0).astype(int)
    df_stats['qtime_lb_count'] = df_stats['qtime_lb_count'].fillna(0).astype(int)

    # Also provide values in nanoseconds for plotting (1 us = 1000 ns)
    df_stats['qtime_mean_ns'] = df_stats['qtime_mean_us'] * 1000.0
    df_stats['qtime_std_ns'] = df_stats['qtime_std_us'] * 1000.0
    df_stats['qtime_lb_mean_ns'] = df_stats['qtime_lb_mean_us'] * 1000.0
    df_stats['qtime_lb_std_ns'] = df_stats['qtime_lb_std_us'] * 1000.0

    # Prepare list of distributions (dataset_name, data_type, R)
    available_combinations = [
        (name, dtype, R) for name, dtype, R in df_stats[['dataset_name', 'data_type', 'R']].drop_duplicates().itertuples(index=False)
        if (name, dtype, R) in DATASET_NAMES
    ]
    distributions = sorted(available_combinations, key=lambda x: DATASET_NAMES[x][0])

    # Fixed distribution order
    fixed_distributions = [key for key in sorted(DATASET_NAMES.keys(), key=lambda x: DATASET_NAMES[x][0])]
    distributions = fixed_distributions

    # restrict n values to a default (keep pattern from other plots)
    target_n_values = [1000]

    if not distributions:
        print('No data available')
        return

    plt.rcParams['figure.figsize'] = [COLUMN_WIDTH * len(distributions) + 0.5, ROW_HEIGHT * len(target_n_values)]
    fig, axes = plt.subplots(len(target_n_values), len(distributions))

    if len(target_n_values) == 1:
        if len(distributions) == 1:
            axes = [[axes]]
        else:
            axes = axes.reshape(1, -1)
    elif len(distributions) == 1:
        axes = axes.reshape(-1, 1)

    # Determine global y-range from values that are actually plotted:
    # - inject_poison_random_std_lower_bound -> qtime_lb_mean_ns
    # - all other approaches (incl. legitimate) -> qtime_mean_ns
    std_mask = (
        (df_stats['approach'] == 'inject_poison_random_std_lower_bound')
        & (df_stats['qtime_lb_count'] > 0)
    )
    displayed_vals = []
    if std_mask.any():
        displayed_vals.append(df_stats.loc[std_mask, 'qtime_lb_mean_ns'])
    other_mask = ~std_mask
    if other_mask.any():
        displayed_vals.append(df_stats.loc[other_mask, 'qtime_mean_ns'])

    if displayed_vals:
        displayed_max = pd.concat(displayed_vals).dropna().max()
        if pd.isna(displayed_max) or displayed_max <= 0:
            max_q = 1.0
        else:
            max_q = float(displayed_max) * 1.05
    else:
        max_q = 1.0

    for i, n_val in enumerate(target_n_values):
        for j, dist in enumerate(distributions):
            ax = axes[i][j]
            dataset_name, data_type, R = dist

            # Filter dataframe for this distribution
            data = df_stats[(df_stats['dataset_name'] == dataset_name) & (df_stats['data_type'] == data_type) & (df_stats['R'] == R)]

            if data.empty:
                ax.set_xlim(0, 15)
                ax.set_ylim(0.0, max_q)
                ax.grid(True, which='both', linestyle='--', linewidth=0.8)
                ax.tick_params(axis='both', labelsize=TICK_SIZE)
                ax.set_xlabel('Poisoning Percentage', fontsize=XLABEL_SIZE)
                if i == 0:
                    ax.set_title(DATASET_NAMES[dist][1], fontsize=FONT_SIZE)
                if j == 0:
                    ax.set_ylabel('Lookup time [ns]', fontsize=FONT_SIZE)
                continue
            
            # Convert lambda to percentage for x-axis
            plot_data = data.copy()
            plot_data['percentage'] = plot_data['lambda'] / n_val * 100
            plot_data = plot_data.sort_values(by='lambda')

            approach_colors = {
                'inject_poison_random': 'gray',
                'inject_poison_random_std_lower_bound': 'gray',
                'inject_poison': 'blue',
                'consecutive_w_endpoints': 'green',
                'consecutive_w_endpoints_duplicate_allowed': 'purple',
                'consecutive_w_endpoints_using_relaxed_solution': 'magenta',
                'duplicate_allowed': 'orange',
                'consecutive': 'brown'
            }
            approach_markers = {
                'inject_poison_random': 'x',
                'inject_poison_random_std_lower_bound': 'o',
                'inject_poison': '.',
                'consecutive_w_endpoints': 's',
                'consecutive_w_endpoints_duplicate_allowed': '^',
                'consecutive_w_endpoints_using_relaxed_solution': 'p',
                'duplicate_allowed': 'd',
                'consecutive': 'v'
            }
            approach_linesyles = {
                'inject_poison_random': '--',
                'inject_poison_random_std_lower_bound': '--',
                'inject_poison': ':',
                'consecutive_w_endpoints': '-',
                'consecutive_w_endpoints_duplicate_allowed': '--',
                'consecutive_w_endpoints_using_relaxed_solution': '-.',
                'duplicate_allowed': '--',
                'consecutive': ':'
            }
            approach_labels = {
                'inject_poison_random': 'Random',
                'inject_poison_random_std_lower_bound': 'Random (std::lower_bound)',
                'inject_poison': 'Greedy',
                'consecutive_w_endpoints': 'Seg+E',
                'consecutive_w_endpoints_duplicate_allowed': 'Seg+E (Relaxed)',
                'consecutive_w_endpoints_using_relaxed_solution': 'Seg+E (Heuristic)',
                'duplicate_allowed': 'Duplicate Allowed',
                'consecutive': 'Consecutive'
            }

            # Draw legitimate baseline (lambda == 0) as green horizontal line if present
            legitimate_data = plot_data[plot_data['approach'] == 'legitimate']
            if not legitimate_data.empty:
                # take mean across any legitimate entries (multiple seeds aggregated)
                # use ns values
                legitimate_mean = legitimate_data['qtime_mean_ns'].mean()
                ax.axhline(y=legitimate_mean, color='black', linestyle='--', label='Legitimate')

            # Plot poisoned approaches (exclude 'legitimate')
            poison_plot_data = plot_data[plot_data['approach'] != 'legitimate']
            for approach in poison_plot_data['approach'].unique():
                adata = poison_plot_data[poison_plot_data['approach'] == approach]
                if adata.empty:
                    continue
                # skip any lambda==0 entries just in case
                adata = adata[adata['lambda'] > 0]
                if adata.empty:
                    continue
                color = approach_colors.get(approach, 'blue')
                marker = approach_markers.get(approach, 'o')
                linestyle = approach_linesyles.get(approach, '-')
                label = approach_labels.get(approach, approach)

                # Decide which columns to plot: if this approach is the dedicated
                # std_lower_bound variant, plot the lower_bound aggregated values.
                if approach == 'inject_poison_random_std_lower_bound':
                    # plot lower_bound mean/std if present
                    if 'qtime_lb_mean_ns' in adata.columns and adata['qtime_lb_count'].sum() > 0:
                        if show_error_bars:
                            ax.errorbar(adata['percentage'], adata['qtime_lb_mean_ns'],
                                        yerr=adata['qtime_lb_std_ns'], fmt=marker + '-', label=label, color=color,
                                        linewidth=2, markersize=8, capsize=5, capthick=2)
                        else:
                            ax.plot(adata['percentage'], adata['qtime_lb_mean_ns'], marker + linestyle, label=label, color=color,
                                    linewidth=2, markersize=8)
                    else:
                        # fallback to qtime_mean if lb not available
                        if show_error_bars:
                            ax.errorbar(adata['percentage'], adata['qtime_mean_ns'],
                                        yerr=adata['qtime_std_ns'], fmt=marker + '-', label=label, color=color,
                                        linewidth=2, markersize=8, capsize=5, capthick=2)
                        else:
                            ax.plot(adata['percentage'], adata['qtime_mean_ns'], marker + linestyle, label=label, color=color,
                                    linewidth=2, markersize=8)
                else:
                    # regular plotting for other approaches (use qtime_mean)
                    if show_error_bars:
                        ax.errorbar(adata['percentage'], adata['qtime_mean_ns'],
                                    yerr=adata['qtime_std_ns'], fmt=marker + '-', label=label, color=color,
                                    linewidth=2, markersize=8, capsize=5, capthick=2)
                    else:
                        ax.plot(adata['percentage'], adata['qtime_mean_ns'], marker + linestyle, label=label, color=color,
                                linewidth=2, markersize=8)

                if VERBOSE:
                    for _, row in adata.iterrows():
                        print(f"{label} - {DATASET_NAMES[dist][1]}, n={n_val}, percentage={row['percentage']:.1f}%: {row['qtime_count']} samples")

            # ax.set_yscale('log')
            ax.grid(True, which='both', linestyle='--', linewidth=0.8)
            ax.tick_params(axis='both', labelsize=TICK_SIZE)
            ax.set_xlabel('Poisoning Percentage', fontsize=XLABEL_SIZE)
            if i == 0:
                ax.set_title(DATASET_NAMES[dist][1], fontsize=FONT_SIZE)
            if j == 0:
                ax.set_ylabel('Lookup time [ns]', fontsize=FONT_SIZE)

            ax.set_ylim(0.0, max_q)

    # Place legend
    handles, labels = axes[len(target_n_values) - 1][len(distributions) - 1].get_legend_handles_labels()
    if handles and labels:
        # Build a mapping from approach key to plotted label text.
        # Use the approaches parameter order when available to drive legend ordering.
        # Use the same label texts here as used when plotting (approach_labels)
        # so that mapping from approach key -> label matches the actual legend
        # entries. This ensures the legend ordering follows `approaches`.
        approach_to_label = {
            'legitimate': 'Legitimate',
            'inject_poison_random': 'Random',
            'inject_poison_random_std_lower_bound': 'Random (std::lower_bound)',
            'inject_poison': 'Greedy',
            'consecutive_w_endpoints': 'Seg+E',
            'consecutive_w_endpoints_duplicate_allowed': 'Seg+E (Relaxed)',
            'consecutive_w_endpoints_using_relaxed_solution': 'Seg+E (Heuristic)',
            'duplicate_allowed': 'Duplicate Allowed',
            'consecutive': 'Consecutive'
        }

        # Map label text to handle for quick lookup
        label_to_handle = {}
        for h, l in zip(handles, labels):
            if l not in label_to_handle:
                label_to_handle[l] = h

        ordered_handles = []
        ordered_labels = []

        # Determine desired order of approach keys: use provided approaches list
        if approaches is not None:
            desired_order_keys = list(approaches)
            # Ensure 'legitimate' appears first if present in data but not in approaches
            if 'legitimate' in label_to_handle and 'legitimate' not in desired_order_keys:
                desired_order_keys.insert(0, 'legitimate')
        else:
            desired_order_keys = ['legitimate', 'inject_poison_random', 'inject_poison_random_std_lower_bound', 'inject_poison', 'consecutive_w_endpoints', 'consecutive_w_endpoints_using_relaxed_solution']

        # Add handles in the desired order when present
        for approach_key in desired_order_keys:
            lbl = approach_to_label.get(approach_key)
            if lbl and lbl in label_to_handle and lbl not in ordered_labels:
                ordered_handles.append(label_to_handle[lbl])
                ordered_labels.append(lbl)

        # Append any remaining handles that were not included above
        for h, l in zip(handles, labels):
            if l not in ordered_labels:
                ordered_handles.append(h)
                ordered_labels.append(l)

        fig.legend(ordered_handles, ordered_labels,
                   bbox_to_anchor=(0.5, 0.0), loc='upper center',
                   ncol=(len(ordered_handles)), frameon=True,
                   facecolor='white', edgecolor='black', fontsize=LEGEND_SIZE)

    plt.tight_layout()
    os.makedirs(os.path.dirname(fig_path), exist_ok=True)
    plt.savefig(fig_path, bbox_inches='tight')
    plt.close()
    print(f"Saved {fig_path}")


if __name__ == '__main__':
    output_dir = '../results/figures/lambda_query_time'
    os.makedirs(output_dir, exist_ok=True)

    data_dir = '..'
    approaches_to_plot = [
        'legitimate',
        'inject_poison_random',
        'inject_poison_random_std_lower_bound',
        'inject_poison',
        'consecutive_w_endpoints',
        'consecutive_w_endpoints_using_relaxed_solution'
    ]

    df_q = load_query_time_results(data_dir, approaches=approaches_to_plot)

    # narrow to n=1000 like other scripts
    df_q_ns = df_q[df_q['n'].isin([1000])]

    plot_lambda_query_time(df_q_ns, f'{output_dir}/lambda_query_time_all.pdf', n_value=None, show_error_bars=None, approaches=approaches_to_plot)
