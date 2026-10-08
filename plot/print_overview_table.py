"""Generate results/tables/overview.tex from brute-force experiment results.

Uses the same case set as Figure 14 (n=50, DATASET_NAMES_BRUTE_FORCE,
binary_search upper bound) and fills Empirical Perf. Ratio cells and the
case-count in the caption from the data.
"""

import os

import numpy as np
import pandas as pd

from load_loss import load_loss
from load_optimal_poison import load_optimal_poison
from load_upper_bound import load_upper_bound
from plot_config import (
    DATASET_NAMES_BRUTE_FORCE,
    LOSS_COLUMN,
    UPPER_BOUND_COLUMN,
)

N_BRUTE_FORCE = 50
UPPER_BOUND_ALGORITHM = "binary_search"
RATIO_DECIMALS = 3
ALWAYS_ONE_ATOL = 1e-9


def format_latex_int(n):
    """Format an integer for LaTeX math mode with thousand separators, e.g. 3000 -> 3{,}000."""
    s = f"{int(n):,}"
    return s.replace(",", "{,}")


def format_attack_ratio(ratios):
    """Format attack Empirical Perf. Ratio: $1$ if always 1, else $\\geq\\!min$."""
    ratios = np.asarray(ratios, dtype=float)
    if np.allclose(ratios, 1.0, rtol=0.0, atol=ALWAYS_ONE_ATOL):
        return "$1$"
    return f"$\\geq\\!{ratios.min():.{RATIO_DECIMALS}f}$"


def format_upper_bound_ratio(ratios):
    """Format upper-bound Empirical Perf. Ratio: $1$ if always 1, else $\\leq\\!max$."""
    ratios = np.asarray(ratios, dtype=float)
    if np.allclose(ratios, 1.0, rtol=0.0, atol=ALWAYS_ONE_ATOL):
        return "$1$"
    return f"$\\leq\\!{ratios.max():.{RATIO_DECIMALS}f}$"


def _prepare_loss_df(df, loss_col_name):
    return (
        df[df["lambda"] > 0][["dataset_name", "data_type", "R", "n", "lambda", "seed", LOSS_COLUMN]]
        .rename(columns={LOSS_COLUMN: loss_col_name})
        .sort_values(by=["dataset_name", "data_type", "R", "n", "lambda", "seed"])
    )


def collect_merged_cases(data_dir):
    """Load and merge all sources needed for overview Empirical Perf. Ratios."""
    df_greedy = load_loss(data_dir, approach="original")
    df_exact = load_loss(data_dir, approach="consecutive_w_endpoints")
    df_exact_dup = load_loss(data_dir, approach="consecutive_w_endpoints_duplicate_allowed")
    df_heuristic = load_loss(data_dir, approach="consecutive_w_endpoints_using_relaxed_solution")
    df_upper_bound = load_upper_bound(data_dir)
    df_optimal = load_optimal_poison(data_dir)

    n = N_BRUTE_FORCE
    df_greedy = df_greedy[df_greedy["n"] == n]
    df_exact = df_exact[df_exact["n"] == n]
    df_exact_dup = df_exact_dup[df_exact_dup["n"] == n]
    df_heuristic = df_heuristic[df_heuristic["n"] == n]
    df_upper_bound = df_upper_bound[df_upper_bound["n"] == n]
    df_optimal = df_optimal[df_optimal["n"] == n]

    df_upper_bound = df_upper_bound[df_upper_bound["algorithm"] == UPPER_BOUND_ALGORITHM]

    available_combinations = [
        (name, dtype, R)
        for name, dtype, R in df_greedy[["dataset_name", "data_type", "R"]].drop_duplicates().itertuples(index=False)
        if (name, dtype, R) in DATASET_NAMES_BRUTE_FORCE
    ]
    distributions = sorted(available_combinations, key=lambda x: DATASET_NAMES_BRUTE_FORCE[x][0])

    frames = []
    for dataset_name, data_type, R in distributions:
        def subset(df):
            return df[
                (df["dataset_name"] == dataset_name)
                & (df["data_type"] == data_type)
                & (df["R"] == R)
            ]

        greedy = _prepare_loss_df(subset(df_greedy), "loss_greedy")
        exact = _prepare_loss_df(subset(df_exact), "loss_exact")
        exact_dup = _prepare_loss_df(subset(df_exact_dup), "loss_exact_dup")
        heuristic = _prepare_loss_df(subset(df_heuristic), "loss_heuristic")

        upper = subset(df_upper_bound)[["lambda", "seed", UPPER_BOUND_COLUMN]].sort_values(
            by=["lambda", "seed"]
        ).rename(columns={UPPER_BOUND_COLUMN: "upper_bound"})

        brute = subset(df_optimal)
        brute = brute[brute["algorithm"] == "brute_force"][["lambda", "seed", "loss"]].sort_values(
            by=["lambda", "seed"]
        ).rename(columns={"loss": "loss_brute_force"})

        brute_dup = subset(df_optimal)
        brute_dup = brute_dup[brute_dup["algorithm"] == "brute_force_duplicate_allowed"][
            ["lambda", "seed", "loss"]
        ].sort_values(by=["lambda", "seed"]).rename(columns={"loss": "loss_brute_force_dup"})

        # Align keys: start from brute_force as in Figure 14
        merge_keys = ["lambda", "seed"]
        merged = brute.merge(upper, on=merge_keys, how="inner")
        merged = merged.merge(greedy[["lambda", "seed", "loss_greedy"]], on=merge_keys, how="left")
        merged = merged.merge(brute_dup, on=merge_keys, how="left")
        merged = merged.merge(exact[["lambda", "seed", "loss_exact"]], on=merge_keys, how="left")
        merged = merged.merge(exact_dup[["lambda", "seed", "loss_exact_dup"]], on=merge_keys, how="left")
        merged = merged.merge(heuristic[["lambda", "seed", "loss_heuristic"]], on=merge_keys, how="left")

        required_cols = [
            "loss_greedy",
            "upper_bound",
            "loss_brute_force",
            "loss_brute_force_dup",
            "loss_exact",
            "loss_exact_dup",
            "loss_heuristic",
        ]
        if merged[required_cols].isna().any().any():
            missing = {c: int(merged[c].isna().sum()) for c in required_cols if merged[c].isna().any()}
            print(
                f"Warning: Missing data for {dataset_name}, {data_type}, n={n}, R={R}: {missing}. Skipping."
            )
            continue

        merged["dataset_name"] = dataset_name
        merged["data_type"] = data_type
        merged["R"] = R
        frames.append(merged)

    if not frames:
        raise RuntimeError("No complete (dataset, n, R) cases found for overview table.")

    return pd.concat(frames, ignore_index=True)


def compute_ratio_strings(merged):
    """Compute LaTeX strings for each Empirical Perf. Ratio cell."""
    greedy_ratio = merged["loss_greedy"] / merged["loss_brute_force"]
    exact_ratio = merged["loss_exact"] / merged["loss_brute_force"]
    exact_dup_ratio = merged["loss_exact_dup"] / merged["loss_brute_force_dup"]
    heuristic_ratio = merged["loss_heuristic"] / merged["loss_brute_force"]
    ub_orig_ratio = merged["upper_bound"] / merged["loss_brute_force"]
    ub_relaxed_ratio = merged["upper_bound"] / merged["loss_brute_force_dup"]

    stats = {
        "n_cases": len(merged),
        "greedy_min": float(greedy_ratio.min()),
        "exact_min": float(exact_ratio.min()),
        "exact_max": float(exact_ratio.max()),
        "exact_dup_min": float(exact_dup_ratio.min()),
        "exact_dup_max": float(exact_dup_ratio.max()),
        "heuristic_min": float(heuristic_ratio.min()),
        "ub_orig_max": float(ub_orig_ratio.max()),
        "ub_relaxed_max": float(ub_relaxed_ratio.max()),
    }

    strings = {
        "n_cases_tex": format_latex_int(len(merged)),
        "greedy": format_attack_ratio(greedy_ratio),
        "exact": format_attack_ratio(exact_ratio),
        "exact_dup": format_attack_ratio(exact_dup_ratio),
        "heuristic": format_attack_ratio(heuristic_ratio),
        "optimal": "$1$",
        "optimal_dup": "$1$",
        "ub_orig": format_upper_bound_ratio(ub_orig_ratio),
        "ub_relaxed": format_upper_bound_ratio(ub_relaxed_ratio),
    }
    return strings, stats


def render_overview_tex(strings):
    """Render overview.tex with dynamic Empirical Perf. Ratio cells and case count."""
    return f"""\\begin{{table*}}[t]
    \\centering
    \\caption{{Overview over time complexity and empirical performance of poisoning attacks and upper-bounding methods under the original setting~\\cite{{kornaropoulos2022price}} (duplicate keys disallowed; \\cref{{def:poisoning_linear_regression_on_cdfs}}) and the relaxed setting (duplicate keys allowed; \\cref{{def:relaxed_poisoning_problem}}).
    Here, $n$ is the number of legitimate keys, $\\lambda$ is the poisoning budget, and $T$ is the number of iterations in our upper-bounding methods.
    \\textit{{Empirical Perf. Ratio}} is the achieved MSE (attacks) or the bound (upper bounds) normalized by the optimal-attack MSE over ${strings['n_cases_tex']}$ cases in \\cref{{sec:experiment:upper_bound_vs_greedy_brute_force}} (values closer to $1$ indicate a better attack or a tighter upper bound).}}
    \\label{{tab:seg_e}}
    \\footnotesize
    \\setlength{{\\tabcolsep}}{{4pt}}
    {{\\renewcommand{{\\arraystretch}}{{0.93}}
    \\begin{{tabular}}{{@{{}}lcccc@{{}}}}
        \\toprule
        & \\multicolumn{{2}}{{c}}{{Original Setting (\\cref{{def:poisoning_linear_regression_on_cdfs}})}} & \\multicolumn{{2}}{{c}}{{Relaxed Setting (\\cref{{def:relaxed_poisoning_problem}})}} \\\\ 
        \\cmidrule(lr){{2-3}} \\cmidrule(lr){{4-5}}
        Algorithm & Time Complexity & \\makecell{{Empirical Perf. Ratio}} & Time Complexity & \\makecell{{Empirical Perf. Ratio}} \\\\
        \\midrule
        Greedy~\\cite{{kornaropoulos2022price}}
            & $\\mathcal{{O}}(n \\lambda)$ & {strings['greedy']}
            & - & - \\\\ \\midrule
        Exact Seg+E (Ours)
            & $\\mathcal{{O}}(n \\lambda^3)$ & {strings['exact']}
            & $\\mathcal{{O}}(n \\lambda)$ & {strings['exact_dup']} \\\\
        Heuristic Seg+E (Ours)
            & $\\mathcal{{O}}(n \\lambda)$ & {strings['heuristic']}
            & - & -\\\\
        Optimal (Ours)
            & $\\mathcal{{O}}\\!\\left((n + \\lambda) \\binom{{2n-2+\\lambda}}{{\\lambda}}\\right)$ & {strings['optimal']}
            & $\\mathcal{{O}}\\!\\left((n + \\lambda) \\binom{{n+\\lambda-1}}{{\\lambda}}\\right)$ & {strings['optimal_dup']} \\\\
        \\midrule
        Upper Bound (Ours)
            & \\makecell{{$\\mathcal{{O}}(T(n + \\lambda))$ or \\\\$\\mathcal{{O}}((n + \\lambda) \\log (n + \\lambda))$}}  & {strings['ub_orig']}
            & \\makecell{{$\\mathcal{{O}}(T(n + \\lambda))$ or \\\\$\\mathcal{{O}}((n + \\lambda) \\log (n + \\lambda))$}}  & {strings['ub_relaxed']} \\\\
        \\bottomrule
    \\end{{tabular}}
    }}
\\end{{table*}}
"""


def main():
    data_dir = ".."
    output_dir = "../results/tables"
    output_path = os.path.join(output_dir, "overview.tex")

    print("Collecting merged cases (same setting as Figure 14)...")
    merged = collect_merged_cases(data_dir)
    strings, stats = compute_ratio_strings(merged)

    print(f"n_cases = {stats['n_cases']}")
    print(f"Greedy min ratio             = {stats['greedy_min']:.6f} -> {strings['greedy']}")
    print(f"Exact Seg+E min/max          = {stats['exact_min']:.6f} / {stats['exact_max']:.6f} -> {strings['exact']}")
    print(f"Exact Seg+E (relaxed) min/max= {stats['exact_dup_min']:.6f} / {stats['exact_dup_max']:.6f} -> {strings['exact_dup']}")
    print(f"Heuristic Seg+E min ratio    = {stats['heuristic_min']:.6f} -> {strings['heuristic']}")
    print(f"Upper Bound (orig) max ratio = {stats['ub_orig_max']:.6f} -> {strings['ub_orig']}")
    print(f"Upper Bound (relaxed) max    = {stats['ub_relaxed_max']:.6f} -> {strings['ub_relaxed']}")

    os.makedirs(output_dir, exist_ok=True)
    tex = render_overview_tex(strings)
    with open(output_path, "w", encoding="utf-8") as f:
        f.write(tex)
    print(f"Saved {output_path}")


if __name__ == "__main__":
    main()
