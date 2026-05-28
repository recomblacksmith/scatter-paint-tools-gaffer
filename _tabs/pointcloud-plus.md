---
title: PointCloud Plus
icon: fas fa-cloud
order: 4
---

# Gaffer PointCloud Plus

PointCloud Plus is the point-cloud utility plugin in the set.

Use it when you want to generate points from geometry, republish points cleanly, or remap point playback.

<iframe width="560" height="315" src="https://www.youtube.com/embed/xRk4YGDx5qg?si=-JeBF1N0x-IHuqEQ" title="YouTube video player" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" referrerpolicy="strict-origin-when-cross-origin" allowfullscreen></iframe>


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

![pane01.png](/assets/images/pointcloud_plus/pane01.png)
![pane02.png](/assets/images/pointcloud_plus/pane02.png)
![pane03.png](/assets/images/pointcloud_plus/pane03.png)
