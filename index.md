---
layout: page
title: Home
permalink: /
---

![Scatter Paint Tools Logo](/assets/logo/dev-logo.png){: w="320" h="320" }

# Scatter Paint Tools for Gaffer

This site tracks the current Scatter Paint Tools for Gaffer toolset.

It documents the Docker-first build flow, the combined toolset release payload, and the current shipped surface for:

- `gaffer_scatter_paint`
- `gaffer_scatter_plus`
- `gaffer_pointcloud_plus`

## Quick Start

Build the full toolset locally with Docker:

```bash
./build-plugins.sh --plugin all --pull
```

That flow:

- pulls or reuses `d3smond/scatter-paint-tools-gaffer-build:gaffer-1.6.18.0`
- downloads and verifies packaged Gaffer `1.6.18.0`
- builds all three plugins
- assembles one combined side-load payload under `dist/gaffer`
- smoke-tests the combined toolset

## Current Release Shape

- one combined GitHub release per toolset build
- one combined artifact: `scatter-paint-tools-gaffer-toolset.tar.gz`
- one combined runtime payload inside `dist/gaffer`

Use the sidebar tabs for build instructions, releases, plugin pages, and deeper scatter-paint docs.

## Videos

{% assign wip_video = '/assets/scatter-paint-tools-wip-720.mp4' %}
{% include embed/video.html src=wip_video title='Scatter Paint Tools work-in-progress' %}

{% include embed/youtube.html id='R47paQI2cdY' %}

{% include embed/youtube.html id='FumNi5t9K7k' %}

{% include embed/youtube.html id='lyFmMzGyIc8' %}

{% include embed/youtube.html id='5Qni1gobubE' %}
