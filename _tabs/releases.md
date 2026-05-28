---
title: Releases
icon: fas fa-box-open
order: 2
---

# Releases

This project ships as one toolset release.

You do not need to download three separate plugins.

## Release Artifact

Each release publishes this archive:

```text
scatter-paint-tools-gaffer-toolset.tar.gz
```

Inside it, you get one combined `dist/gaffer` payload containing:

- Scatter Paint
- Scatter Plus
- PointCloud Plus

## Release Flow

- `Linux Toolset Build` checks that the full toolset still builds
- `Release Toolset` rebuilds the toolset from `main`, packages it, and publishes a GitHub release
- the current build image is `d3smond/scatter-paint-tools-gaffer-build:gaffer-1.6.18.0`

## Builder Image Tags

Available build image tags:

- `d3smond/scatter-paint-tools-gaffer-build:gaffer-1.6.18.0`
- `d3smond/scatter-paint-tools-gaffer-build:latest`
- `d3smond/scatter-paint-tools-gaffer-build:1`

Pull explicitly with:

```bash
docker pull d3smond/scatter-paint-tools-gaffer-build:gaffer-1.6.18.0
```

## GitHub Releases

- GitHub releases: <https://github.com/recomblacksmith/scatter-paint-tools-gaffer/releases>
- source repository: <https://github.com/recomblacksmith/scatter-paint-tools-gaffer>
- Docker Hub build image: <https://hub.docker.com/r/d3smond/scatter-paint-tools-gaffer-build>

## Current Release Notes

See the release post on this site for the current packaging and publish flow.
