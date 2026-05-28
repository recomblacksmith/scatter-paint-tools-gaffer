---
title: Releases
icon: fas fa-box-open
order: 2
---

# Releases

The toolset is published as one combined release, not as separate plugin packages.

## Release Artifact

Each release archives the combined payload as:

```text
scatter-paint-tools-gaffer-toolset.tar.gz
```

That archive contains the merged runtime payload from `dist/gaffer` with all three plugins inside.

## Release Flow

- `Linux Toolset Build` builds and smoke-tests the combined toolset
- `Release Toolset` rebuilds on `main`, archives `dist/gaffer`, creates a timestamp tag, and publishes a GitHub release
- current builder image: `d3smond/scatter-paint-tools-gaffer-build:gaffer-1.6.18.0`

## Builder Image Tags

Available Docker tags:

- `d3smond/scatter-paint-tools-gaffer-build:gaffer-1.6.18.0`
- `d3smond/scatter-paint-tools-gaffer-build:latest`
- `d3smond/scatter-paint-tools-gaffer-build:1`

Pull explicitly with:

```bash
docker pull d3smond/scatter-paint-tools-gaffer-build:gaffer-1.6.18.0
```

## GitHub Releases

- repository releases: <https://github.com/recomblacksmith/scatter-paint-tools-gaffer/releases>
- source repository: <https://github.com/recomblacksmith/scatter-paint-tools-gaffer>
- Docker Hub builder image: <https://hub.docker.com/r/d3smond/scatter-paint-tools-gaffer-build>

## Current Release Notes

See the release post on this site for the current combined toolset release pipeline and artifact shape.
