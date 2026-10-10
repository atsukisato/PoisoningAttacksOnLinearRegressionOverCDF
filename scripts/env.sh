#!/usr/bin/env bash

# Directory of this scripts folder
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Project root (one level up from scripts directory)
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Standard directories used across scripts
BUILD_DIR="$PROJECT_ROOT/build"
RAW_DATA_DIR="$PROJECT_ROOT/data/raw"
DATA_DIR="$PROJECT_ROOT/data/generated"
RESULTS_DIR="$PROJECT_ROOT/results"
PLOT_DIR="$PROJECT_ROOT/plot"
PAPER_DIR="$PROJECT_ROOT/paper"

export SCRIPT_DIR PROJECT_ROOT BUILD_DIR RAW_DATA_DIR DATA_DIR RESULTS_DIR PLOT_DIR PAPER_DIR

# Nested layout under data/generated:
#   Real legit:  $DATA_DIR/$dataset/n$n/seed$seed/legitimate_uint64
#   Sync legit:  $DATA_DIR/$dataset/n$n/R$R/seed$seed/legitimate_uint64
#   Real poison: $DATA_DIR/$dataset/n$n/seed$seed/lambda$lambda/${method}_uint64
#   Sync poison: $DATA_DIR/$dataset/n$n/R$R/seed$seed/lambda$lambda/${method}_uint64
#
# Optional trailing R: omit (or pass empty) for real datasets.
generated_legit_path() {
    local dataset="$1"
    local n="$2"
    local seed="$3"
    local R="${4:-}"
    local dtype="${5:-uint64}"
    if [ -n "$R" ]; then
        echo "$DATA_DIR/${dataset}/n${n}/R${R}/seed${seed}/legitimate_${dtype}"
    else
        echo "$DATA_DIR/${dataset}/n${n}/seed${seed}/legitimate_${dtype}"
    fi
}

generated_poison_path() {
    local dataset="$1"
    local n="$2"
    local seed="$3"
    local lambda="$4"
    local method="$5"
    local R="${6:-}"
    local dtype="${7:-uint64}"
    if [ -n "$R" ]; then
        echo "$DATA_DIR/${dataset}/n${n}/R${R}/seed${seed}/lambda${lambda}/${method}_${dtype}"
    else
        echo "$DATA_DIR/${dataset}/n${n}/seed${seed}/lambda${lambda}/${method}_${dtype}"
    fi
}

# Poisoning percentages
base_POISONING_PERCENTAGE=10
all_POISONING_PERCENTAGES=(2 4 6 8 10 12 14 16 18 20)
all_POISONING_PERCENTAGES_consecutive=(2 4 6 8 10)
quick_POISONING_PERCENTAGES=(10)

# Real dataset names
all_real_dataset_names=(
    "books_200M"
    "fb_200M"
    "osm_cellids_200M"
)
quick_real_dataset_names=(
    "books_200M"
    "fb_200M"
    "osm_cellids_200M"
)

# Synthetic dataset names
all_sync_dataset_names=(
    "uniform"
    "normal"
    "exponential"
)
quick_sync_dataset_names=(
    "uniform"
    "normal"
    "exponential"
)

# n values
base_n=1000
all_ns=(
    100
    200
    500
    1000
    2000
    5000
    10000
)
all_ns_consecutive=(
    100
    200
    500
    1000
)
quick_ns=(
    1000
)

# seeds
all_seeds=($(seq 0 99))
quick_seeds=(0)
# --all-one-seed: full --all grids but a single seed (same as quick)
one_seed_seeds=(0)

# R values
base_R=100000
all_Rs=(
    2000
    3000
    4000
    5000
    7000
    10000
    20000
    50000
    100000
    200000
    500000
    1000000
)
all_Rs_consecutive=(
    100000
)
quick_Rs=(
    100000
)

# Brute force
base_brute_force_POISONING_PERCENTAGE=10
all_brute_force_POISONING_PERCENTAGES=(2 4 6 8 10)
quick_brute_force_POISONING_PERCENTAGES=(6)
base_brute_force_n=50
all_brute_force_ns=(50)
quick_brute_force_ns=(50)
base_brute_force_R=1000
all_brute_force_Rs=(100 150 200 300 500 1000 2000 5000 10000)
quick_brute_force_Rs=(1000)

export base_POISONING_PERCENTAGE base_n base_R
export all_POISONING_PERCENTAGES quick_POISONING_PERCENTAGES
export all_real_dataset_names quick_real_dataset_names
export all_sync_dataset_names quick_sync_dataset_names
export all_ns quick_ns
export all_seeds quick_seeds one_seed_seeds
export all_Rs quick_Rs
export base_brute_force_POISONING_PERCENTAGE all_brute_force_POISONING_PERCENTAGES quick_brute_force_POISONING_PERCENTAGES
export base_brute_force_n all_brute_force_ns quick_brute_force_ns
export base_brute_force_R all_brute_force_Rs quick_brute_force_Rs
export all_POISONING_PERCENTAGES_consecutive all_ns_consecutive all_Rs_consecutive
