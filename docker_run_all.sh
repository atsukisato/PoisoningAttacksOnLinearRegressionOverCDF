#!/bin/bash

set -euo pipefail

IMAGE_NAME="poisoning"
IMAGE_TAG="latest"

MODE=""

# Analyze arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --quick)
            MODE="--quick"
            shift
            ;;
        --all)
            MODE="--all"
            shift
            ;;
        --all-one-seed)
            MODE="--all-one-seed"
            shift
            ;;
        -h|--help)
            echo "Usage: $0 <--quick|--all|--all-one-seed>"
            echo "  --quick: Run quick experiment"
            echo "  --all: Run all experiments"
            echo "  --all-one-seed: Run all experiments with a single seed"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 <--quick|--all|--all-one-seed>"
            exit 1
            ;;
    esac
done

# Check required arguments
if [[ -z "$MODE" ]]; then
    echo "Error: --quick, --all, or --all-one-seed is required"
    echo "Usage: $0 <--quick|--all|--all-one-seed>"
    exit 1
fi

mkdir -p results

echo "Running mode: $MODE"

# Docker container resource limits and isolation
CID=$(docker run -d \
    --cpus="1" \
    --memory="4g" \
    --memory-swap="4g" \
    --cpuset-cpus="0" \
    --shm-size=1g \
    --cap-add=SYS_ADMIN \
    -v "$(pwd)":/workspace \
    ${IMAGE_NAME}:${IMAGE_TAG} \
    bash -c "cd /workspace/scripts && ./run_all.sh $MODE > /workspace/results/run_all.log 2>&1")

echo "Started container: $CID"
echo "Follow progress:   tail -f results/run_all.log"
echo "Wait for finish:   docker wait $CID"
echo "Check exit code:   docker inspect -f '{{.State.Status}} {{.State.ExitCode}}' $CID"
echo "Success criterion: Status=exited and ExitCode=0"
