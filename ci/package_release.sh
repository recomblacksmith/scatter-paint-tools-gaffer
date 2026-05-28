#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST_ROOT="$ROOT_DIR/dist/gaffer"
VERSION_FILE="$ROOT_DIR/VERSION"
GAFFER_VERSION="${GAFFER_VERSION:-1.6.18.0}"
TARGET_OS="${TARGET_OS:-linux}"

if [[ ! -f "$VERSION_FILE" ]]; then
  printf 'Missing VERSION file at %s\n' "$VERSION_FILE" >&2
  exit 1
fi

VERSION="$(tr -d '[:space:]' < "$VERSION_FILE")"
if [[ -z "$VERSION" ]]; then
  printf 'VERSION file is empty\n' >&2
  exit 1
fi

ARCHIVE_BASENAME="scatter-paint-tools-gaffer-v${VERSION}-${TARGET_OS}-gaffer-${GAFFER_VERSION}"
STAGING_ROOT="$ROOT_DIR/dist/release"
PACKAGE_ROOT="$STAGING_ROOT/$ARCHIVE_BASENAME"
PAYLOAD_ROOT="$PACKAGE_ROOT/scatter-paint-tools-gaffer"
SETUP_GUIDE="$PACKAGE_ROOT/how-to-setup.md"

if [[ ! -d "$DIST_ROOT" ]]; then
  printf 'Expected combined build payload at %s\n' "$DIST_ROOT" >&2
  exit 1
fi

rm -rf "$PACKAGE_ROOT"
mkdir -p "$PAYLOAD_ROOT"

copy_tree_contents() {
  local src="$1"
  local dst="$2"
  if [[ -d "$src" ]]; then
    mkdir -p "$dst"
    cp -a "$src"/. "$dst/"
  fi
}

for plugin_dir in gaffer_scatter_paint gaffer_scatter_plus gaffer_pointcloud_plus; do
  plugin_payload_root="$PAYLOAD_ROOT/$plugin_dir"
  copy_tree_contents "$ROOT_DIR/$plugin_dir/python" "$plugin_payload_root/python"
  copy_tree_contents "$ROOT_DIR/$plugin_dir/startup" "$plugin_payload_root/startup"

  if [[ "$plugin_dir" == "gaffer_scatter_paint" ]]; then
    copy_tree_contents "$ROOT_DIR/$plugin_dir/graphics" "$plugin_payload_root/graphics"
    copy_tree_contents "$ROOT_DIR/$plugin_dir/demo" "$plugin_payload_root/demo"
  fi
done

cat > "$SETUP_GUIDE" <<'EOF'
# How To Setup

This package contains the Scatter Paint Tools for Gaffer release for Gaffer __GAFFER_VERSION__.

Archive layout:

- __ARCHIVE_BASENAME__/
- __ARCHIVE_BASENAME__/scatter-paint-tools-gaffer/gaffer_scatter_paint/
- __ARCHIVE_BASENAME__/scatter-paint-tools-gaffer/gaffer_scatter_plus/
- __ARCHIVE_BASENAME__/scatter-paint-tools-gaffer/gaffer_pointcloud_plus/

## Linux setup

1. Unpack the archive.
2. Point Gaffer at the plugin folders inside:

   __ARCHIVE_BASENAME__/scatter-paint-tools-gaffer/

3. Use the packaged Gaffer __GAFFER_VERSION__ runtime this release was built against.

Example:

```bash
export TOOL_ROOT="/path/to/__ARCHIVE_BASENAME__/scatter-paint-tools-gaffer"
export GAFFER_ROOT="/path/to/gaffer-__GAFFER_VERSION__-linux-gcc11"

export PYTHONPATH="\
\$TOOL_ROOT/gaffer_scatter_paint/python:\
\$TOOL_ROOT/gaffer_scatter_plus/python:\
\$TOOL_ROOT/gaffer_pointcloud_plus/python:\
\$GAFFER_ROOT/python"

export GAFFER_STARTUP_PATHS="\
\$TOOL_ROOT/gaffer_scatter_paint/startup:\
\$TOOL_ROOT/gaffer_scatter_plus/startup:\
\$TOOL_ROOT/gaffer_pointcloud_plus/startup"

export LD_LIBRARY_PATH="\$GAFFER_ROOT/lib"

"\$GAFFER_ROOT/bin/gaffer"
```

If you only want a quick validation, run:

```bash
PYTHONNOUSERSITE=1 \
QT_QPA_PLATFORM=offscreen \
IECORE_FONT_PATHS="\$GAFFER_ROOT/fonts" \
LD_LIBRARY_PATH="\$GAFFER_ROOT/lib" \
PYTHONPATH="\$TOOL_ROOT/gaffer_scatter_paint/python:\$TOOL_ROOT/gaffer_scatter_plus/python:\$TOOL_ROOT/gaffer_pointcloud_plus/python:\$GAFFER_ROOT/python" \
GAFFER_STARTUP_PATHS="\$TOOL_ROOT/gaffer_scatter_paint/startup:\$TOOL_ROOT/gaffer_scatter_plus/startup:\$TOOL_ROOT/gaffer_pointcloud_plus/startup" \
"\$GAFFER_ROOT/bin/python" -c "import GafferScatterPaint, GafferScatterPaintUI, GafferScatterPlus, GafferScatterPlusUI, GafferPointCloudPlus, GafferPointCloudPlusUI"
```
EOF

sed -i \
  -e "s|__ARCHIVE_BASENAME__|$ARCHIVE_BASENAME|g" \
  -e "s|__GAFFER_VERSION__|$GAFFER_VERSION|g" \
  "$SETUP_GUIDE"

printf 'Prepared release package staging at %s\n' "$PACKAGE_ROOT"
