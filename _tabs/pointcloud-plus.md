---
title: PointCloud Plus
icon: fas fa-cloud
order: 5
---

# Gaffer PointCloud Plus

`gaffer_pointcloud_plus` is the point-cloud generation and republish plugin in the toolset.

## Current Scope

- random surface point generation
- primitive-center point generation
- republish of an upstream `PointsPrimitive`
- file-style playback remapping through `frame`, `frameOffset`, and `animationBehavior`
- injecting point output at a chosen location while preserving the surrounding source scene

## Runtime Registration

- `/PointCloud/PointCloud Plus`
- `/PointCloudPlus/Nodes/PointCloud Plus`
- `/PointCloudPlus/Demos/Geometry Basic`
- `/PointCloudPlus/Demos/Primitive Center`
- `/PointCloudPlus/Demos/File Republish`
- `/Tools/PointCloud Plus/...`

## Tests

Current coverage exercises:

- geometry mode
- primitive-center mode
- file republish mode
- auto-discovery of input point locations
- playback remapping
- intermediate branch creation
- preserved ancestor transforms and attributes
- demo action wiring
