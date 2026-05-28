#!/usr/bin/env bash

set -euo pipefail

CACHE_ROOT="/cache/gaffer"
VERSION="${GAFFER_VERSION:?GAFFER_VERSION is required}"
TARBALL="gaffer-${VERSION}-linux-gcc11.tar.gz"
URL="${GAFFER_URL:?GAFFER_URL is required}"
SHA256="${GAFFER_SHA256:?GAFFER_SHA256 is required}"
ARCHIVE_PATH="$CACHE_ROOT/$TARBALL"
EXTRACT_ROOT="$CACHE_ROOT/$VERSION"
RUNTIME_DIR="$EXTRACT_ROOT/gaffer-${VERSION}-linux-gcc11"

mkdir -p "$CACHE_ROOT" "$EXTRACT_ROOT"

if [[ ! -f "$ARCHIVE_PATH" ]]; then
  curl -L --fail --output "$ARCHIVE_PATH" "$URL"
fi

ACTUAL_SHA256="$(sha256sum "$ARCHIVE_PATH" | awk '{print $1}')"
if [[ "$ACTUAL_SHA256" != "$SHA256" ]]; then
  printf 'SHA256 mismatch for %s\nExpected: %s\nActual:   %s\n' "$ARCHIVE_PATH" "$SHA256" "$ACTUAL_SHA256" >&2
  exit 1
fi

if [[ ! -x "$RUNTIME_DIR/bin/python" ]]; then
  rm -rf "$EXTRACT_ROOT"/*
  tar -xzf "$ARCHIVE_PATH" -C "$EXTRACT_ROOT"
fi

printf '%s\n' "$RUNTIME_DIR" > /cache/gaffer-runtime-path.txt
