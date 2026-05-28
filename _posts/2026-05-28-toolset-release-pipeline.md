---
title: Combined Toolset Release Pipeline
date: 2026-05-28 12:00:00 +0000
categories: [Releases]
tags: [gaffer, docker, github-actions]
pin: true
description: How the combined Scatter Paint Tools for Gaffer release is packaged and published.
---

This project now ships as one combined toolset release.

That means artists do not need to hunt for separate plugin downloads.

## What Gets Released

- artifact name: `scatter-paint-tools-gaffer-toolset.tar.gz`
- payload root: `dist/gaffer`
- included tools:
  - Scatter Paint
  - Scatter Plus
  - PointCloud Plus

## Build Inputs

- builder image: `d3smond/scatter-paint-tools-gaffer-build:gaffer-1.6.18.0`
- packaged Gaffer runtime: `1.6.18.0`
- entrypoint: `./build-plugins.sh --plugin all --pull`

## Automation

The release workflow:

1. rebuilds the full toolset on `main`
2. archives `dist/gaffer`
3. creates a timestamp tag
4. publishes a GitHub release

This keeps the docs, build image, and downloadable release aligned around one install path.
