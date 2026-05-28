#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST_ROOT="$ROOT_DIR/dist/gaffer"
PLUGIN="all"

usage() {
  cat <<'EOF'
Usage: ./ci/package_plugin.sh [--plugin scatter_paint|scatter_plus|pointcloud_plus|all]
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

if [[ "$PLUGIN" == "all" ]]; then
  rm -rf "$DIST_ROOT"
fi

mkdir -p "$DIST_ROOT/python" "$DIST_ROOT/startup"

copy_tree_contents() {
  local src="$1"
  local dst="$2"
  if [[ -d "$src" ]]; then
    mkdir -p "$dst"
    cp -a "$src"/. "$dst/"
  fi
}

for plugin_dir in "${plugins[@]}"; do
  copy_tree_contents "$ROOT_DIR/$plugin_dir/python" "$DIST_ROOT/python"
  copy_tree_contents "$ROOT_DIR/$plugin_dir/startup" "$DIST_ROOT/startup"
  if [[ "$plugin_dir" == "gaffer_scatter_paint" ]]; then
    copy_tree_contents "$ROOT_DIR/$plugin_dir/graphics" "$DIST_ROOT/graphics"
    copy_tree_contents "$ROOT_DIR/$plugin_dir/demo" "$DIST_ROOT/demo"
  fi
done
