#!/usr/bin/env bash
set -euo pipefail

# [CALC] Step 5: Calculate upper bounds for all datasets
# This script calculates upper bounds for all datasets

# Load common environment
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/env.sh"

# Debug output
echo "DEBUG: SCRIPT_DIR=$SCRIPT_DIR"
echo "DEBUG: PROJECT_ROOT=$PROJECT_ROOT"
echo "DEBUG: BUILD_DIR=$BUILD_DIR"
echo "DEBUG: DATA_DIR=$DATA_DIR"
echo "DEBUG: RESULTS_DIR=$RESULTS_DIR"
echo ""

# Configuration
# Use presets from env.sh
POISONING_PERCENTAGES=("${all_POISONING_PERCENTAGES[@]}")
real_dataset_names=("${all_real_dataset_names[@]}")
sync_dataset_names=("${all_sync_dataset_names[@]}")
ns=("${all_ns[@]}")
seeds=("${all_seeds[@]}")
Rs=("${all_Rs[@]}")
brute_force_POISONING_PERCENTAGES=("${all_brute_force_POISONING_PERCENTAGES[@]}")
brute_force_ns=("${all_brute_force_ns[@]}")
brute_force_Rs=("${all_brute_force_Rs[@]}")

# Parse command line arguments
ALL_MODE=false
ALL_ONE_SEED_MODE=false
QUICK_MODE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --all)
            ALL_MODE=true
            shift
            ;;
        --all-one-seed)
            ALL_ONE_SEED_MODE=true
            seeds=("${one_seed_seeds[@]}")
            shift
            ;;
        --quick)
            QUICK_MODE=true
            POISONING_PERCENTAGES=("${quick_POISONING_PERCENTAGES[@]}")
            real_dataset_names=("${quick_real_dataset_names[@]}")
            sync_dataset_names=("${quick_sync_dataset_names[@]}")
            ns=("${quick_ns[@]}")
            seeds=("${quick_seeds[@]}")
            Rs=("${quick_Rs[@]}")
            brute_force_POISONING_PERCENTAGES=("${quick_brute_force_POISONING_PERCENTAGES[@]}")
            base_brute_force_POISONING_PERCENTAGE="${quick_brute_force_POISONING_PERCENTAGES[0]}"
            brute_force_ns=("${quick_brute_force_ns[@]}")
            brute_force_Rs=("${quick_brute_force_Rs[@]}")
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 --all|--all-one-seed|--quick"
            exit 1
            ;;
    esac
done

# Check if a mode was specified
if [ "$ALL_MODE" = false ] && [ "$ALL_ONE_SEED_MODE" = false ] && [ "$QUICK_MODE" = false ]; then
    echo "Error: Either --all, --all-one-seed, or --quick option must be specified"
    echo "Usage: $0 --all|--all-one-seed|--quick"
    exit 1
fi

echo "[CALC] Step 5: Calculating upper bounds for all datasets..."
echo "  Data directory: $DATA_DIR"
echo "  Results directory: $RESULTS_DIR"
echo "  Base poisoning percentage: $base_POISONING_PERCENTAGE"
echo "  Base n: $base_n"
echo "  Base R: $base_R"
echo ""

# Create results directory
mkdir -p "$RESULTS_DIR/upper_bound"

# Change to build directory
cd "$BUILD_DIR"

# Function to calculate upper bounds for a dataset
run_calc_upper_bound() {
    input_file_name="$1"
    poison_num="$2"
    base_output_name="$3"
    dataset_name="$4"
    n="$5"
    
    if [ ! -f "$DATA_DIR/${input_file_name}" ]; then
        echo "    Input file not found: $DATA_DIR/${input_file_name}"
        return
    fi
    
    # Check if all output JSON files already exist
    binary_json_file="$RESULTS_DIR/upper_bound/binary/${dataset_name}/n${n}/lambda${poison_num}/${base_output_name}_binary.json"
    golden_json_file="$RESULTS_DIR/upper_bound/golden/${dataset_name}/n${n}/lambda${poison_num}/${base_output_name}_golden.json"
    strict_json_file="$RESULTS_DIR/upper_bound/strict/${dataset_name}/n${n}/lambda${poison_num}/${base_output_name}_strict.json"
    mkdir -p "$RESULTS_DIR/upper_bound/binary/${dataset_name}/n${n}/lambda${poison_num}"
    mkdir -p "$RESULTS_DIR/upper_bound/golden/${dataset_name}/n${n}/lambda${poison_num}"
    mkdir -p "$RESULTS_DIR/upper_bound/strict/${dataset_name}/n${n}/lambda${poison_num}"
    
    echo "    Calculating upper bounds for: $input_file_name (lambda=$poison_num)"
    
    # Binary Search Algorithm
    if [ -f "$binary_json_file" ]; then
        echo "        [Skipped] Binary search JSON file already exists: ${base_output_name}_binary.json"
    else
        ./calc_upper_bound_binary "$DATA_DIR/${input_file_name}" $poison_num "$binary_json_file" 2>/dev/null || echo "      Error running binary search for $input_file_name with lambda=$poison_num"
    fi
    
    # Golden Section Algorithm
    if [ -f "$golden_json_file" ]; then
        echo "        [Skipped] Golden section JSON file already exists: ${base_output_name}_golden.json"
    else
        ./calc_upper_bound_golden "$DATA_DIR/${input_file_name}" $poison_num "$golden_json_file" 2>/dev/null || echo "      Error running golden section for $input_file_name with lambda=$poison_num"
    fi
    
    # Strict/Exact Algorithm
    if [ -f "$strict_json_file" ]; then
        echo "        [Skipped] Strict/exact JSON file already exists: ${base_output_name}_strict.json"
    else
        ./calc_upper_bound_strict "$DATA_DIR/${input_file_name}" $poison_num "$strict_json_file" 2>/dev/null || echo "      Error running strict algorithm for $input_file_name with lambda=$poison_num"
    fi
}

# Function to process real datasets with given parameters
process_real_datasets_upper_bound() {
    local n_val="$1"
    local percentage="$2"
    local poison_num=$((n_val * percentage / 100))
    
    for real_dataset_name in "${real_dataset_names[@]}"; do
        for seed in "${seeds[@]}"; do
            for dtype in "uint64"; do
                input_file_name="${real_dataset_name}_n${n_val}_seed${seed}_${dtype}"
                base_output_name="${real_dataset_name}_n${n_val}_seed${seed}_lambda${poison_num}_percentage${percentage}_${dtype}"
                run_calc_upper_bound "$input_file_name" "$poison_num" "$base_output_name" "$real_dataset_name" "$n_val"
            done
        done
    done
}

# Function to process synthetic datasets with given parameters
process_sync_datasets_upper_bound() {
    local n_val="$1"
    local R_val="$2"
    local percentage="$3"
    local poison_num=$((n_val * percentage / 100))
    
    for sync_dataset_name in "${sync_dataset_names[@]}"; do
        for seed in "${seeds[@]}"; do
            for dtype in "uint64"; do
                input_file_name="${sync_dataset_name}_n${n_val}_R${R_val}_seed${seed}_${dtype}"
                base_output_name="${sync_dataset_name}_n${n_val}_R${R_val}_seed${seed}_lambda${poison_num}_percentage${percentage}_${dtype}"
                run_calc_upper_bound "$input_file_name" "$poison_num" "$base_output_name" "$sync_dataset_name" "$n_val"
            done
        done
    done
}

# Experiment 1: Vary n while keeping base_R and base_POISONING_PERCENTAGE
echo "  Experiment 1: Varying n (base_R=$base_R, base_POISONING_PERCENTAGE=$base_POISONING_PERCENTAGE)"
for n in "${ns[@]}"; do
    echo "    Varying n: $n"
    process_real_datasets_upper_bound "$n" "$base_POISONING_PERCENTAGE"
    process_sync_datasets_upper_bound "$n" "$base_R" "$base_POISONING_PERCENTAGE"
done

for n in "${brute_force_ns[@]}"; do
    echo "    Varying n (for brute force): $n"
    process_real_datasets_upper_bound "$n" "$base_brute_force_POISONING_PERCENTAGE"
    process_sync_datasets_upper_bound "$n" "$base_brute_force_R" "$base_brute_force_POISONING_PERCENTAGE"
done

# Experiment 2: Vary R while keeping base_n and base_POISONING_PERCENTAGE
echo "  Experiment 2: Varying R (base_n=$base_n, base_POISONING_PERCENTAGE=$base_POISONING_PERCENTAGE)"
for R in "${Rs[@]}"; do
    if [ "$R" -le "$base_n" ]; then
        continue
    fi
    echo "    Varying R: $R"
    process_sync_datasets_upper_bound "$base_n" "$R" "$base_POISONING_PERCENTAGE"
done

for R in "${brute_force_Rs[@]}"; do
    if [ "$R" -le "$base_brute_force_n" ]; then
        continue
    fi
    echo "    Varying R (for brute force): $R"
    process_sync_datasets_upper_bound "$base_brute_force_n" "$R" "$base_brute_force_POISONING_PERCENTAGE"
done

# Experiment 3: Vary poisoning percentage while keeping base_n and base_R
echo "  Experiment 3: Varying poisoning percentage (base_n=$base_n, base_R=$base_R)"
for percentage in "${POISONING_PERCENTAGES[@]}"; do
    echo "    Varying poisoning percentage: $percentage"
    process_real_datasets_upper_bound "$base_n" "$percentage"
    process_sync_datasets_upper_bound "$base_n" "$base_R" "$percentage"
done

for percentage in "${brute_force_POISONING_PERCENTAGES[@]}"; do
    echo "    Varying poisoning percentage (for brute force): $percentage"
    process_real_datasets_upper_bound "$base_brute_force_n" "$percentage"
    process_sync_datasets_upper_bound "$base_brute_force_n" "$base_brute_force_R" "$percentage"
done

echo "[OK] Upper bound calculation completed"
echo "  Results saved to: $RESULTS_DIR/upper_bound/"
echo ""
