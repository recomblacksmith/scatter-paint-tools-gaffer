---
title: Combined Toolset Release Pipeline
date: 2026-05-28 12:00:00 +0000
categories: [Releases]
tags: [gaffer, docker, github-actions]
pin: true
description: Current GitHub release flow for the combined Scatter Paint Tools for Gaffer toolset.
---

The current release path publishes one combined toolset payload, not separate plugin archives.

## What Gets Released

- artifact name: `scatter-paint-tools-gaffer-toolset.tar.gz`
- payload root: `dist/gaffer`
- included plugins:
  - `gaffer_scatter_paint`
  - `gaffer_scatter_plus`
  - `gaffer_pointcloud_plus`

## Build Inputs

- builder image: `d3smond/scatter-paint-tools-gaffer-build:gaffer-1.6.18.0`
- packaged Gaffer runtime: `1.6.18.0`
- entrypoint: `./build-plugins.sh --plugin all --pull`

## Automation

The release workflow:

1. rebuilds the combined toolset on `main`
2. archives `dist/gaffer`
3. creates a timestamp tag
4. publishes a GitHub release

This keeps docs, builder image, and release output aligned around one toolset contract.
