#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE_NAME="${IMAGE_NAME:-d3smond/scatter-paint-tools-gaffer-build:gaffer-1.6.18.0}"
CONTAINER_WORKDIR="/workspace"
GAFFER_VERSION="${GAFFER_VERSION:-1.6.18.0}"
GAFFER_TARBALL="gaffer-${GAFFER_VERSION}-linux-gcc11.tar.gz"
GAFFER_URL_DEFAULT="https://github.com/GafferHQ/gaffer/releases/download/${GAFFER_VERSION}/${GAFFER_TARBALL}"
GAFFER_URL="${GAFFER_URL:-$GAFFER_URL_DEFAULT}"
GAFFER_SHA256_DEFAULT="39f58326607524806c856647de7f36fec35a6a08c117ed747ca6bac7dc3bf8d2"
GAFFER_SHA256="${GAFFER_SHA256:-$GAFFER_SHA256_DEFAULT}"
DOCKER_VOLUME="${DOCKER_VOLUME:-scatter-paint-tools-gaffer-cache}"
PLUGIN_ARG="all"
RUN_TESTS=0
PULL_IMAGE=0
BUILD_IMAGE=0

usage() {
  cat <<'EOF'
Usage: ./build-plugins.sh [options]

Options:
  --plugin <name>   Build one plugin: scatter_paint, scatter_plus, pointcloud_plus, or all
  --tests           Run full unittest packages after smoke tests
  --pull            Pull the configured builder image before running
  --build-image     Build the local builder image from Dockerfile.build
  --image <name>    Override the Docker image name
  --help            Show this help text

Environment:
  IMAGE_NAME        Builder image to run (default: d3smond/scatter-paint-tools-gaffer-build:gaffer-1.6.18.0)
  GAFFER_VERSION    Gaffer version to download (default: 1.6.18.0)
  GAFFER_URL        Override the Gaffer runtime tarball URL
  GAFFER_SHA256     Override the Gaffer runtime tarball SHA256
  DOCKER_VOLUME     Docker volume used as the Gaffer/runtime cache
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --plugin)
      PLUGIN_ARG="$2"
      shift 2
      ;;
    --tests)
      RUN_TESTS=1
      shift
      ;;
    --pull)
      PULL_IMAGE=1
      shift
      ;;
    --build-image)
      BUILD_IMAGE=1
      shift
      ;;
    --image)
      IMAGE_NAME="$2"
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

if ! command -v docker >/dev/null 2>&1; then
  printf 'Docker is required to run build-plugins.sh\n' >&2
  exit 1
fi

if [[ "$BUILD_IMAGE" -eq 1 ]]; then
  docker build -f "$ROOT_DIR/Dockerfile.build" -t "$IMAGE_NAME" "$ROOT_DIR"
fi

if [[ "$PULL_IMAGE" -eq 1 ]]; then
  docker pull "$IMAGE_NAME"
fi

if ! docker volume inspect "$DOCKER_VOLUME" >/dev/null 2>&1; then
  docker volume create "$DOCKER_VOLUME" >/dev/null
fi

docker run --rm \
  -v "$ROOT_DIR":"$CONTAINER_WORKDIR" \
  -v "$DOCKER_VOLUME":/cache \
  -w "$CONTAINER_WORKDIR" \
  -e GAFFER_VERSION="$GAFFER_VERSION" \
  -e GAFFER_URL="$GAFFER_URL" \
  -e GAFFER_SHA256="$GAFFER_SHA256" \
  -e RUN_TESTS="$RUN_TESTS" \
  "$IMAGE_NAME" \
  bash -lc "./ci/download_gaffer.sh && ./ci/build_plugin.sh --plugin '$PLUGIN_ARG' && ./ci/package_plugin.sh --plugin '$PLUGIN_ARG' && ./ci/smoke_test_plugin.sh --plugin '$PLUGIN_ARG'"
