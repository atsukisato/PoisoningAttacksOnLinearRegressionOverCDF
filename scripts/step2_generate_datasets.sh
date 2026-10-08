#!/usr/bin/env bash
set -euo pipefail

# Step 2: Generate datasets
# This script generates real and synthetic datasets for the poisoning experiment

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
        --help|-h)
            echo "Usage: $0 --all|--all-one-seed|--quick"
            echo ""
            echo "[DATA] Dataset Generation Tool"
            echo ""
            echo "Options:"
            echo "  --all           Generate large datasets with all parameters"
            echo "  --all-one-seed  Generate all-parameter datasets with a single seed"
            echo "  --quick         Generate small test datasets"
            echo "  --help,-h       Show this help message"
            echo ""
            echo "This script uses parameters defined in env.sh and passes them to gen_real and gen_sync programs."
            exit 0
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

echo "[DATA] Step 2: Generating datasets..."
echo "  Data directory: $DATA_DIR"
if [ "$ALL_MODE" = true ]; then
    echo "  Mode: ALL"
elif [ "$ALL_ONE_SEED_MODE" = true ]; then
    echo "  Mode: ALL-ONE-SEED"
elif [ "$QUICK_MODE" = true ]; then
    echo "  Mode: QUICK"
else
    echo "  Mode: DEFAULT"
fi
echo ""

# Create data directory
mkdir -p "$DATA_DIR"

# Change to build directory
cd "$BUILD_DIR"

# Helper function to join array elements with comma
join_by_comma() {
    local IFS=','
    echo "$*"
}

# Set parameters based on mode
if [ "$ALL_MODE" = true ] || [ "$ALL_ONE_SEED_MODE" = true ]; then
    # Use all_ prefixed arrays (one-seed mode only narrows seeds)
    real_datasets_param=$(join_by_comma "${all_real_dataset_names[@]}")
    sync_datasets_param=$(join_by_comma "${all_sync_dataset_names[@]}")
    ns_param=$(join_by_comma "${all_ns[@]}")
    if [ "$ALL_ONE_SEED_MODE" = true ]; then
        seeds_param=$(join_by_comma "${one_seed_seeds[@]}")
    else
        seeds_param=$(join_by_comma "${all_seeds[@]}")
    fi
    Rs_param=$(join_by_comma "${all_Rs[@]}")
    brute_force_ns_param=$(join_by_comma "${all_brute_force_ns[@]}")
    brute_force_Rs_param=$(join_by_comma "${all_brute_force_Rs[@]}")
else
    # Use quick_ prefixed arrays
    real_datasets_param=$(join_by_comma "${quick_real_dataset_names[@]}")
    sync_datasets_param=$(join_by_comma "${quick_sync_dataset_names[@]}")
    ns_param=$(join_by_comma "${quick_ns[@]}")
    seeds_param=$(join_by_comma "${quick_seeds[@]}")
    Rs_param=$(join_by_comma "${quick_Rs[@]}")
    brute_force_ns_param=$(join_by_comma "${quick_brute_force_ns[@]}")
    brute_force_Rs_param=$(join_by_comma "${quick_brute_force_Rs[@]}")
fi

# Generate real datasets
echo "  Generating real datasets..."
echo "    Parameters:"
echo "      Real datasets: $real_datasets_param"
echo "      Sample sizes: $ns_param"
echo "      Seeds: $seeds_param"
echo ""

./gen_real \
    --real_dataset_names "$real_datasets_param" \
    --ns "$ns_param" \
    --seeds "$seeds_param"

echo "  Generating real brute force datasets..."
echo "    Parameters:"
echo "      Real datasets: $real_datasets_param"
echo "      Sample sizes: $brute_force_ns_param"
echo "      Seeds: $seeds_param"
echo ""

./gen_real \
    --real_dataset_names "$real_datasets_param" \
    --ns "$brute_force_ns_param" \
    --seeds "$seeds_param"

# Generate synthetic datasets  
echo "  [SYNTH] Generating synthetic datasets for brute force..."
echo "    Parameters:"
echo "      Sync datasets: $sync_datasets_param"
echo "      Sample sizes: $ns_param"
echo "      Seeds: $seeds_param"
echo "      Range values: $Rs_param"
echo ""

## base_n, Rs_param
./gen_sync \
    --sync_dataset_names "$sync_datasets_param" \
    --ns "$base_n" \
    --seeds "$seeds_param" \
    --Rs "$Rs_param"

## ns_param, base_R
./gen_sync \
    --sync_dataset_names "$sync_datasets_param" \
    --ns "$ns_param" \
    --seeds "$seeds_param" \
    --Rs "$base_R"

echo "  [SYNTH] Generating synthetic datasets for brute force..."
echo "    Parameters:"
echo "      Sync datasets: $sync_datasets_param"
echo "      Sample sizes: $brute_force_ns_param"
echo "      Seeds: $seeds_param"
echo "      Range values: $brute_force_Rs_param"
echo ""

## base_brute_force_n, brute_force_Rs_param
./gen_sync \
    --sync_dataset_names "$sync_datasets_param" \
    --ns "$base_brute_force_n" \
    --seeds "$seeds_param" \
    --Rs "$brute_force_Rs_param"

## brute_force_ns_param, base_brute_force_R
./gen_sync \
    --sync_dataset_names "$sync_datasets_param" \
    --ns "$brute_force_ns_param" \
    --seeds "$seeds_param" \
    --Rs "$base_brute_force_R"

echo "[OK] Dataset generation completed"
echo "  Data directory: $DATA_DIR"
echo "  Generated files:"
if [ -d "$DATA_DIR" ]; then
    real_count=$(find "$DATA_DIR" -name "*books*" -o -name "*fb*" -o -name "*osm*" | wc -l)
    sync_count=$(find "$DATA_DIR" -name "*uniform*" -o -name "*normal*" -o -name "*exponential*" | wc -l)
    echo "    [REAL] Real datasets: $real_count"
    echo "    [SYNTH] Synthetic datasets: $sync_count"
fi
echo ""

# Drop unused large SOSD sources under the repository-root data/; keep only the
# three uint64 files used by gen_real.
sosd_data_dir="$RAW_DATA_DIR"
keep_files=(
    "books_200M_uint64"
    "fb_200M_uint64"
    "osm_cellids_200M_uint64"
)

if [ -d "$sosd_data_dir" ]; then
    echo "[CLEAN] Pruning unused SOSD source files in: $sosd_data_dir"
    for keep in "${keep_files[@]}"; do
        if [ -f "$sosd_data_dir/$keep" ]; then
            echo "  [OK] keep $keep"
        else
            echo "  [WARN] missing keep file: $keep"
        fi
    done

    removed=0
    freed_bytes=0
    shopt -s nullglob
    for path in "$sosd_data_dir"/*; do
        [ -f "$path" ] || continue
        name="$(basename "$path")"
        is_keep=false
        for keep in "${keep_files[@]}"; do
            if [ "$name" = "$keep" ]; then
                is_keep=true
                break
            fi
        done
        if [ "$is_keep" = true ]; then
            continue
        fi
        size=$(wc -c < "$path" | tr -d ' ')
        echo "  [DEL] $name"
        rm -f "$path"
        removed=$((removed + 1))
        freed_bytes=$((freed_bytes + size))
    done
    shopt -u nullglob

    if [ "$removed" -eq 0 ]; then
        echo "  Nothing to delete."
    else
        echo "  Removed $removed file(s), freed ${freed_bytes} bytes."
    fi
    echo ""
else
    echo "[CLEAN] SOSD data directory not found, skipping: $sosd_data_dir"
fi
