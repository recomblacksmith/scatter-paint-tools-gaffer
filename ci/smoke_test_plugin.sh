#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST_ROOT="$ROOT_DIR/dist/gaffer"
RUNTIME_DIR="$(cat /cache/gaffer-runtime-path.txt)"
PLUGIN="all"
RUN_TESTS="${RUN_TESTS:-0}"

usage() {
  cat <<'EOF'
Usage: ./ci/smoke_test_plugin.sh [--plugin scatter_paint|scatter_plus|pointcloud_plus|all]
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --plugin)
      PLUGIN="$2"
      shift 2
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      printf 'Unknown argument: %s\n' "$1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

plugins=()
case "$PLUGIN" in
  scatter_paint) plugins+=("scatter_paint") ;;
  scatter_plus) plugins+=("scatter_plus") ;;
  pointcloud_plus) plugins+=("pointcloud_plus") ;;
  all) plugins+=("scatter_paint" "scatter_plus" "pointcloud_plus") ;;
  *)
    printf 'Unsupported plugin selection: %s\n' "$PLUGIN" >&2
    exit 2
    ;;
esac

export PYTHONNOUSERSITE=1
export QT_QPA_PLATFORM=offscreen
export IECORE_FONT_PATHS="$RUNTIME_DIR/fonts"
export LD_LIBRARY_PATH="$RUNTIME_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PYTHONPATH="$DIST_ROOT/python:$RUNTIME_DIR/python"
export GAFFER_STARTUP_PATHS="$DIST_ROOT/startup"

python_bin="$RUNTIME_DIR/bin/python"

for plugin in "${plugins[@]}"; do
  case "$plugin" in
    scatter_paint)
      "$python_bin" -c 'import GafferScatterPaint, GafferScatterPaintUI'
      if [[ "$RUN_TESTS" == "1" ]]; then
        "$python_bin" -m unittest GafferScatterPaintTest
      fi
      ;;
    scatter_plus)
      "$python_bin" -c 'import GafferScatterPlus, GafferScatterPlusUI'
      if [[ "$RUN_TESTS" == "1" ]]; then
        "$python_bin" -m unittest GafferScatterPlusTest
      fi
      ;;
    pointcloud_plus)
      "$python_bin" -c 'import GafferPointCloudPlus, GafferPointCloudPlusUI'
      if [[ "$RUN_TESTS" == "1" ]]; then
        "$python_bin" -m unittest GafferPointCloudPlusTest
      fi
      ;;
  esac
done
