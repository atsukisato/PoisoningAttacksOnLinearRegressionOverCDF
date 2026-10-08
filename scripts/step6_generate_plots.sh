#!/usr/bin/env bash
set -euo pipefail

# [PLOT] Generate All Plots Script
# This script runs all the plotting scripts to generate comprehensive visualizations

# Load common environment
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/env.sh"

echo "[PLOT] Generating All Plots"
echo "======================="
echo "Project root: $PROJECT_ROOT"
echo "Plot directory: $PLOT_DIR"
echo "Results directory: $RESULTS_DIR"
echo ""

# Check if Python3 is available
if ! command -v python3 >/dev/null 2>&1; then
    echo "[ERROR] Error: Python3 is required but not found"
    echo "Please install Python3 and required packages:"
    echo "  pip3 install pandas matplotlib numpy"
    exit 1
fi

# Check if plot directory exists
if [ ! -d "$PLOT_DIR" ]; then
    echo "[ERROR] Error: Plot directory not found: $PLOT_DIR"
    exit 1
fi

# Change to plot directory
cd "$PLOT_DIR"

# Function to run a plot script with error handling
run_plot_script() {
    local script_name="$1"
    local script_path="$PLOT_DIR/$script_name"
    
    echo "[PLOT] Running $script_name..."
    
    if [ ! -f "$script_path" ]; then
        echo "  [WARN] Script not found: $script_path"
        return 1
    fi
    
    if python3 "$script_path" 2>/dev/null; then
        echo "  [OK] $script_name completed successfully"
        return 0
    else
        echo "  [ERROR] $script_name failed"
        return 1
    fi
}

# List of plot scripts to run
PLOT_SCRIPTS=(
    "plot_lambda_LgrLub.py"
    "plot_lambda_time.py"
    "plot_n_LgrLub.py"
    "plot_n_time.py"
    "plot_R_LgrLub.py"
    "plot_lambda_Ls_seed0.py"
    "plot_lambda_L_ratios_brute_force.py"
    "plot_R_L_ratios_brute_force.py"
    "print_overview_table.py"
    "plot_lambda_LgrLconsec.py"
    "plot_lambda_Lconsec_using_relaxed_solution_Lconsec.py"
    "plot_lambda_query_time.py"
)

# Track success/failure
SUCCESS_COUNT=0
FAILURE_COUNT=0

echo "[START] Starting plot generation..."
echo ""

# Run each plot script
for script in "${PLOT_SCRIPTS[@]}"; do
    if run_plot_script "$script"; then
        ((++SUCCESS_COUNT))
    else
        ((++FAILURE_COUNT))
    fi
    echo ""
done

# Summary
echo "[PLOT] Plot Generation Summary"
echo "=========================="
echo "[OK] Successful: $SUCCESS_COUNT"
echo "[ERROR] Failed: $FAILURE_COUNT"
echo "[INFO] Total scripts: ${#PLOT_SCRIPTS[@]}"
echo ""

# Check if any plots were generated
PLOT_COUNT=$(find "$RESULTS_DIR/figures" -name "*.pdf" 2>/dev/null | wc -l || echo "0")
echo "[INFO] Generated plots: $PLOT_COUNT"
echo "[INFO] Plot location: $RESULTS_DIR/figures/"

if [ "$FAILURE_COUNT" -eq 0 ]; then
    echo ""
    echo "[SUCCESS] All plots generated successfully!"
    exit 0
else
    echo ""
    echo "[WARN] Some plots failed to generate. Check the output above for details."
    exit 1
fi 

echo "[END] Reached end of script"
