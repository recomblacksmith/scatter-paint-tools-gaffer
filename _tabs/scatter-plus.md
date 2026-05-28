---
title: Scatter Plus
icon: fas fa-seedling
order: 3
---

# Gaffer Scatter Plus

Scatter Plus is the procedural scatter plugin in the set.

Use it when you want to spread instances across a support surface without painting them by hand.

## What It Does

- scatters points and instances across support geometry
- can drive distribution from an image
- can vary density from primitive variables already on the mesh
- can output helper points as well as instanced prototypes
- gives you different spaces for support and decimation evaluation

Typical examples:

- grass, pebbles, leaves, and small ground clutter
- image-driven breakup for worn areas or paths
- mesh-attr masks from look-dev or surfacing
- quick population passes before a paint pass on top

## What You See In Gaffer

- `/ScatterPlus/Nodes/Scatter Plus`
- `/ScatterPlus/Demos/Image Scatter`
- `/Tools/Scatter Plus/...`

## Notes

This is the plugin to reach for when you want broad coverage fast, then art-direct the final bits with Scatter Paint.

For the technical test/coverage notes, use the developer pages from the home screen.
