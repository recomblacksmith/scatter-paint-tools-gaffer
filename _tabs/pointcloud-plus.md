---
title: PointCloud Plus
icon: fas fa-cloud
order: 5
---

# Gaffer PointCloud Plus

PointCloud Plus is the point-cloud utility tool in the set.

Use it when you want to generate points from geometry, republish points cleanly, or remap point playback.

## What It Does

- creates random points on surfaces
- creates one point per primitive center when that is the better fit
- republishes incoming point clouds into a cleaner scene location
- remaps playback timing for file-style point caches
- keeps the rest of the source scene intact while adding the point output

## Runtime Registration

- `/PointCloud/PointCloud Plus`
- `/PointCloudPlus/Nodes/PointCloud Plus`
- `/PointCloudPlus/Demos/Geometry Basic`
- `/PointCloudPlus/Demos/Primitive Center`
- `/PointCloudPlus/Demos/File Republish`
- `/Tools/PointCloud Plus/...`

## What Is Already Covered

Current build coverage checks:

- geometry mode
- primitive-center mode
- file republish mode
- auto-discovery of input point locations
- playback remapping
- intermediate branch creation
- preserved ancestor transforms and attributes
- demo action wiring
