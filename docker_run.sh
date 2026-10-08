#!/bin/bash

set -euo pipefail

IMAGE_NAME="poisoning"
IMAGE_TAG="latest"

docker run -it --rm \
    --cpus="1" \
    --memory="4g" \
    --memory-swap="4g" \
    --cpuset-cpus="0" \
    --shm-size=1g \
    --cap-add=SYS_ADMIN \
    -v "$(pwd)":/workspace \
    ${IMAGE_NAME}:${IMAGE_TAG} /bin/bash
