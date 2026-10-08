#!/usr/bin/env bash
set -euo pipefail

# [BUILD] Step 1: Build C++ applications
# This script builds all C++ applications needed for the poisoning experiment

# Load common environment
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/env.sh"

echo "[BUILD] Step 1: Building C++ applications..."

# Clean up build directory
# rm -rf "$BUILD_DIR"

# Build applications
cd "$SCRIPT_DIR"
./build.sh
cd "$BUILD_DIR"

# Verify executables exist
for exe in gen_real gen_sync inject_poison calc_loss calc_upper_bound_binary calc_upper_bound_golden calc_upper_bound_strict; do
    if [ ! -f "$exe" ]; then
        echo "[ERROR] Error: $exe not found in $BUILD_DIR"
        echo "Build may have failed. Please check build output."
        exit 1
    fi
done

echo "[OK] Build completed successfully"
echo "  Build directory: $BUILD_DIR"
echo "  Available executables:"
for exe in gen_real gen_sync inject_poison calc_loss calc_upper_bound_binary calc_upper_bound_golden calc_upper_bound_strict; do
    if [ -f "$exe" ]; then
        echo "    [OK] $exe"
    fi
done
echo "" 