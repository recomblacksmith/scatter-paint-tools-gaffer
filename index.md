---
layout: page
title: Home
permalink: /
---

![Scatter Paint Tools Logo](/assets/logo/dev-logo.png){: w="320" h="320" }

# Scatter Paint Tools for Gaffer

This site is the artist-facing home for the Scatter Paint Tools for Gaffer toolset.

You can use these tools to paint scatter by hand, generate scatter from support surfaces, and build or republish point clouds inside Gaffer.

This site explains:

- what each tool is for
- how to install the full toolset
- what gets downloaded in a release
- where to look for current status and limitations

## Quick Start

If you just want the full toolset build, run:

```bash
./build-plugins.sh --plugin all --pull
```

That command:

- uses the shared build image for this project
- downloads the matching Gaffer package automatically
- builds all three plugins together
- creates one ready-to-load toolset in `dist/gaffer`
- runs a quick validation pass before finishing

You do not need to build each plugin by hand.

## Current Release Shape

- each release contains all three tools together
- the downloadable archive is `scatter-paint-tools-gaffer-toolset.tar.gz`
- the runtime payload inside that archive lives under `dist/gaffer`

Use the tabs in the sidebar to jump to build help, releases, and tool-specific pages.

## Videos

{% assign wip_video = '/assets/scatter-paint-tools-wip-720.mp4' %}
{% include embed/video.html src=wip_video title='Scatter Paint Tools work-in-progress' %}

{% include embed/youtube.html id='R47paQI2cdY' %}

{% include embed/youtube.html id='FumNi5t9K7k' %}

{% include embed/youtube.html id='lyFmMzGyIc8' %}

{% include embed/youtube.html id='5Qni1gobubE' %}
