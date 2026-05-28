#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUNTIME_DIR="$(cat /cache/gaffer-runtime-path.txt)"
PLUGIN="all"
JOBS="${JOBS:-8}"

usage() {
  cat <<'EOF'
Usage: ./ci/build_plugin.sh [--plugin scatter_paint|scatter_plus|pointcloud_plus|all]
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
  scatter_paint) plugins+=("gaffer_scatter_paint") ;;
  scatter_plus) plugins+=("gaffer_scatter_plus") ;;
  pointcloud_plus) plugins+=("gaffer_pointcloud_plus") ;;
  all) plugins+=("gaffer_scatter_paint" "gaffer_scatter_plus" "gaffer_pointcloud_plus") ;;
  *)
    printf 'Unsupported plugin selection: %s\n' "$PLUGIN" >&2
    exit 2
    ;;
esac

for plugin_dir in "${plugins[@]}"; do
  printf 'Building %s against %s\n' "$plugin_dir" "$RUNTIME_DIR"
  scons -C "$ROOT_DIR/$plugin_dir" "GAFFER_ROOT=$RUNTIME_DIR" "-j$JOBS"
done
