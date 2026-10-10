#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/env.sh"

"$SCRIPT_DIR/build.sh"
exec "$BUILD_DIR/smoke_test"
