#!/usr/bin/env bash
set -euo pipefail

# Full poisoning experiment pipeline (download -> build -> experiments -> plots -> paper)

# Load common environment
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/env.sh"

# Configuration
ALL_MODE=false
ALL_ONE_SEED_MODE=false
QUICK_MODE=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --all)
            ALL_MODE=true
            shift
            ;;
        --all-one-seed)
            ALL_ONE_SEED_MODE=true
            shift
            ;;
        --quick)
            QUICK_MODE=true
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--all|--all-one-seed|--quick]"
            exit 1
            ;;
    esac
done

if [ "$ALL_MODE" = false ] && [ "$ALL_ONE_SEED_MODE" = false ] && [ "$QUICK_MODE" = false ]; then
    echo "Error: Please specify --all, --all-one-seed, or --quick"
    exit 1
fi

ALL_QUICK_ARG=""
if [ "$ALL_MODE" = true ]; then
    ALL_QUICK_ARG="--all"
elif [ "$ALL_ONE_SEED_MODE" = true ]; then
    ALL_QUICK_ARG="--all-one-seed"
elif [ "$QUICK_MODE" = true ]; then
    ALL_QUICK_ARG="--quick"
fi

# Print configuration
echo "Comprehensive Poisoning Experiment"
echo "======================================="
if [ "$ALL_MODE" = true ]; then
    echo "Mode: ALL (full experiments)"
elif [ "$ALL_ONE_SEED_MODE" = true ]; then
    echo "Mode: ALL-ONE-SEED (full grids, single seed)"
else
    echo "Mode: QUICK (limited datasets)"
fi
echo "Quick mode: $([ "$QUICK_MODE" = true ] && echo "Yes" || echo "No")"
echo "Project root: $PROJECT_ROOT"
echo "Data directory: $DATA_DIR"
echo "Results will be saved to: $RESULTS_DIR"
echo ""

# Create necessary directories
mkdir -p "$DATA_DIR"
mkdir -p "$RESULTS_DIR/inject_poison"
mkdir -p "$RESULTS_DIR/upper_bound"
mkdir -p "$RESULTS_DIR/loss"
mkdir -p "$RESULTS_DIR/figures"

# Step 0: Download SOSD raw datasets (skips if the three required files already exist)
echo "Step 0: Downloading datasets (if needed)..."
if ! "$SCRIPT_DIR/download.sh"; then
    echo "Step 0 failed"
    exit 1
fi

# Performance stabilization setup
echo "Setting up performance stabilization..."
{
    # Set CPU governor to performance mode
    echo performance | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor 2>/dev/null && echo "CPU governor set to performance" || echo "CPU governor setting failed"
    
    # Set CPU affinity
    taskset -p -c 0 $$ 2>/dev/null && echo "CPU affinity set" || echo "CPU affinity setting failed"
    
    echo "Performance stabilization setup completed"
} >> "$RESULTS_DIR/performance_setup.log" 2>&1

# Record system information for reproducibility
echo "Recording system information..."
{
    echo "=== System Information ==="
    echo "Date: $(date)"
    echo "Hostname: $(hostname)"
    echo "CPU: $(grep 'model name' /proc/cpuinfo | head -1 | cut -d: -f2 | xargs)"
    echo "CPU cores: $(nproc)"
    echo "Memory: $(free -h | grep Mem | awk '{print $2}')"
    echo "Kernel: $(uname -r)"
    echo "Docker version: $(docker --version 2>/dev/null || echo 'Not available')"
    echo ""
    echo "=== Performance Settings ==="
    echo "CPU governor: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo 'Not available')"
    echo "I/O scheduler: $(cat /sys/block/sda/queue/scheduler 2>/dev/null | grep -o '\[.*\]' || echo 'Not available')"
    echo "Process priority: $(ps -o nice -p $$ | tail -1)"
    echo "CPU affinity: $(taskset -p $$ 2>/dev/null | cut -d: -f2 || echo 'Not available')"
} > "$RESULTS_DIR/system_info.log"

# Step 1: Build C++ applications
echo "Step 1: Building C++ applications..."
if ! "$SCRIPT_DIR/step1_build.sh"; then
    echo "Step 1 failed"
    exit 1
fi

# Step 2: Generate datasets
echo "Step 2: Generating datasets..."
if ! "$SCRIPT_DIR/step2_generate_datasets.sh" $ALL_QUICK_ARG; then
    echo "Step 2 failed"
    exit 1
fi

# Step 3: Inject poison into datasets
echo "Step 3: Injecting poison into datasets..."
if ! "$SCRIPT_DIR/step3_inject_poison.sh" $ALL_QUICK_ARG; then
    echo "Step 3 failed"
    exit 1
fi

# Step 3.0: Inject poison into datasets (duplicate allowed)
echo "Step 3.0: Injecting poison into datasets (duplicate allowed)..."
if ! "$SCRIPT_DIR/step3_0_inject_poison_duplicate_allowed.sh" $ALL_QUICK_ARG; then
    echo "Step 3.0 failed"
    exit 1
fi

# Step 3.2: Inject poison into datasets (consecutive with endpoints approach)
echo "Step 3.2: Injecting poison into datasets (consecutive with endpoints approach)..."
if ! "$SCRIPT_DIR/step3_2_inject_poison_consecutive_w_endpoints.sh" $ALL_QUICK_ARG; then
    echo "Step 3.2 failed"
    exit 1
fi

# Step 3.3: Inject poison into datasets (consecutive with endpoints approach (duplicate allowed))
echo "Step 3.3: Injecting poison into datasets (consecutive with endpoints approach (duplicate allowed))..."
if ! "$SCRIPT_DIR/step3_3_inject_poison_consecutive_w_endpoints_duplicate_allowed.sh" $ALL_QUICK_ARG; then
    echo "Step 3.3 failed"
    exit 1
fi

# Step 3.4: Inject poison into datasets (consecutive with endpoints using relaxed solution approach)
echo "Step 3.4: Injecting poison into datasets (consecutive with endpoints using relaxed solution approach)..."
if ! "$SCRIPT_DIR/step3_4_inject_poison_consecutive_w_endpoints_using_relaxed_solution.sh" $ALL_QUICK_ARG; then
    echo "Step 3.4 failed"
    exit 1
fi

# Step 3.5: Inject poison into datasets (random approach)
echo "Step 3.5: Injecting poison into datasets (random approach)..."
if ! "$SCRIPT_DIR/step3_5_inject_poison_random.sh" $ALL_QUICK_ARG; then
    echo "Step 3.5 failed"
    exit 1
fi

# Step 4: Calculate loss for all datasets
echo "Step 4: Calculating loss for all datasets..."
if ! "$SCRIPT_DIR/step4_calculate_loss.sh" $ALL_QUICK_ARG; then
    echo "Step 4 failed"
    exit 1
fi

# Step 4.1: Measure time for lower_bound query
echo "Step 4.1: Measuring time for lower_bound query..."
if ! "$SCRIPT_DIR/step4_1_measure_query_time.sh" $ALL_QUICK_ARG; then
    echo "Step 4.1 failed"
    exit 1
fi

# Step 5: Calculate upper bounds for all datasets
echo "Step 5: Calculating upper bounds for all datasets..."
if ! "$SCRIPT_DIR/step5_calc_upper_bound.sh" $ALL_QUICK_ARG; then
    echo "Step 5 failed"
    exit 1
fi

# Step 5.1: Calculate optimal poison (duplicate not allowed)
echo "Step 5.1: Calculating optimal poison (duplicate not allowed)..."
if ! "$SCRIPT_DIR/step5_1_calc_optimal_poison.sh" $ALL_QUICK_ARG; then
    echo "Step 5.1 failed"
    exit 1
fi

# Step 5.2: Calculate optimal poison (duplicate allowed)
echo "Step 5.2: Calculating optimal poison (duplicate allowed)..."
if ! "$SCRIPT_DIR/step5_2_calc_optimal_poison_duplicate_allowed.sh" $ALL_QUICK_ARG; then
    echo "Step 5.2 failed"
    exit 1
fi

# Step 6: Generate plots
echo "Step 6: Generating plots..."
if ! "$SCRIPT_DIR/step6_generate_plots.sh" $ALL_QUICK_ARG; then
    echo "Step 6 failed"
    exit 1
fi

# Step 7: Compile paper
echo "Step 7: Compiling paper..."
if ! "$SCRIPT_DIR/step7_compile_paper.sh"; then
    echo "Step 7 failed"
    exit 1
fi

# Final summary
echo "Comprehensive experiment completed!"
echo "======================================="
echo "Results location: $RESULTS_DIR/"
echo ""
echo "Generated files:"
echo "  Data: $DATA_DIR"
echo "  Results: $RESULTS_DIR/"
echo "  Plots: $RESULTS_DIR/figures/"
echo "  Paper PDF: $PAPER_DIR/main.pdf"
echo ""

if [ -f "$RESULTS_DIR/inject_poison/real_injection_results.csv" ]; then
    real_injections=$(tail -n +2 "$RESULTS_DIR/inject_poison/real_injection_results.csv" | wc -l)
    echo "  Real dataset poison injections: $real_injections"
fi

if [ -f "$RESULTS_DIR/inject_poison/sync_injection_results.csv" ]; then
    sync_injections=$(tail -n +2 "$RESULTS_DIR/inject_poison/sync_injection_results.csv" | wc -l)
    echo "  Synthetic dataset poison injections: $sync_injections"
fi

total_datasets=$([ -d "$DATA_DIR" ] && find "$DATA_DIR" -type f | wc -l || echo "0")
echo "  Total generated datasets: $total_datasets"
echo "Experiment completed successfully!"
