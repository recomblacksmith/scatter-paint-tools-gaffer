---
layout: page
title: Home
permalink: /
---

![Scatter Paint Tools Logo](/assets/logo/dev-logo.png){: w="320" h="320" }

# Scatter Paint Tools for Gaffer

These plugins are for artists and TDs who want better scatter control inside Gaffer.

Use this set when you want to:

- paint points by hand in the viewer
- stick painted points back onto geo
- freeze a painted result for downstream work
- scatter things procedurally from meshes, masks, and attrs
- make or clean up point clouds for layout, dressing, or FX handoff

On this site you can quickly find:

- what each plugin does
- how to install the release build
- where to grab the latest package
- what to watch out for in the current build

## Quick Start

If you just want the latest Linux package, start here:

- [GitHub Releases](https://github.com/recomblacksmith/scatter-paint-tools-gaffer/releases)

Current release file:

```text
scatter-paint-tools-gaffer-v0.1.0-linux-gaffer-1.6.18.0.tar.gz
```

That archive contains all three plugins together:

- Scatter Paint
- Scatter Plus
- PointCloud Plus

## What Each Plugin Is For

### Scatter Paint

Paint points by hand in the viewer for art-directed placement.

Good for:

- grass touch-up
- rock dressing
- debris passes
- ivy, leaves, bolts, and other hand-placed detail
- quick shot-specific fixes when full procedural scatter is too broad

### Scatter Plus

Scatter points or instances procedurally across a mesh.

Good for:

- larger ground cover passes
- image-driven density
- attribute-driven masks
- fast look-dev on support geo

### PointCloud Plus

Make point clouds from geometry, or clean up and republish incoming points.

Good for:

- turning surfaces into points
- republishing points into a cleaner scene branch
- simple point-cache prep for downstream work

## Getting Started

Start with the `Install` page in the sidebar.

In most cases the flow is simple:

1. download the release archive
2. unpack it
3. point Gaffer at the included plugin folder

If your studio already has a Gaffer package in place, that is usually all you need.

## Developer Notes

If you need the technical pages, they are still here:

- [Developer Notes]({{ '/developer-notes/' | relative_url }})
- [Nodes]({{ '/developer-notes/nodes/' | relative_url }})
- [Cache Schema]({{ '/developer-notes/cache-schema/' | relative_url }})
- [Status Details]({{ '/developer-notes/status-details/' | relative_url }})
- [Stats]({{ '/developer-notes/stats/' | relative_url }})

## Videos

{% assign wip_video = '/assets/scatter-paint-tools-wip-720.mp4' %}
{% include embed/video.html src=wip_video title='Scatter Paint Tools work-in-progress' %}

{% include embed/youtube.html id='R47paQI2cdY' %}

{% include embed/youtube.html id='FumNi5t9K7k' %}

{% include embed/youtube.html id='lyFmMzGyIc8' %}

{% include embed/youtube.html id='5Qni1gobubE' %}
