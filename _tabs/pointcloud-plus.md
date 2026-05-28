---
title: PointCloud Plus
icon: fas fa-cloud
order: 4
---

# Gaffer PointCloud Plus

PointCloud Plus is the point-cloud utility plugin in the set.

Use it when you want to generate points from geometry, republish points cleanly, or remap point playback.

## What It Does

- creates random points on surfaces
- creates one point per primitive center when that is the better fit
- republishes incoming point clouds into a cleaner scene location
- remaps playback timing for file-style point caches
- keeps the rest of the source scene intact while adding the point output

Typical examples:

- turning a mesh into points for downstream scatter
- rebuilding a point branch into a cleaner location in the scene
- republishing points before handing them to lighting or FX
- quick point prep without a big graph rewrite

## What You See In Gaffer

- `/PointCloud/PointCloud Plus`
- `/PointCloudPlus/Nodes/PointCloud Plus`
- `/PointCloudPlus/Demos/Geometry Basic`
- `/PointCloudPlus/Demos/Primitive Center`
- `/PointCloudPlus/Demos/File Republish`
- `/Tools/PointCloud Plus/...`

## Notes

This one is less flashy than Scatter Paint, but it is useful glue when a point job needs to be tidied up fast.

For the deeper technical notes, use the developer pages from the home screen.
