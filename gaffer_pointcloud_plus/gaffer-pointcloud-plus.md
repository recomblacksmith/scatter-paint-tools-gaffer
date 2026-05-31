# Gaffer PointCloud Plus Design Overview

`PointCloudPlus` is the current point-cloud plugin in this repository.

It focuses on scene-driven point generation, cleanup, remap, and output workflows that can be loaded into Gaffer as a standalone extension.

## Current plugin role

- generate point clouds from support geometry
- output points at a chosen location as a `PointsPrimitive`
- output upstream point data in file-style playback workflows
- support simple frame remapping for repeated or held playback
- keep the surrounding input scene visible while injecting point output

## Current node

The shipped node is `PointCloudPlus`.

Its current public workflow covers:

- random surface point generation
- primitive-center point generation
- output of upstream `PointsPrimitive` input
- file-style playback remapping through `frame`, `frameOffset`, and `animationBehavior`

The startup integration currently registers:

- `/PointCloud/PointCloud Plus`
- `/PointCloudPlus/Nodes/PointCloud Plus`
- `/PointCloudPlus/Demos/Geometry Basic`
- `/PointCloudPlus/Demos/Primitive Center`
- `/PointCloudPlus/Demos/File Output`
- `/Tools/PointCloud Plus/...`

## Output model

`PointCloudPlus` emits a `PointsPrimitive` at the configured output location while preserving the rest of the source scene around that injected subtree.

The current demo graphs wrap the result with `GafferScene.OpenGLAttributes` so helper points preview cleanly in `SceneView`.

## Supported workflows

The plugin currently supports:

- random surface point generation from support geometry
- primitive-center generation
- file-mode output of an upstream point cloud input
- file-mode auto-discovery of an upstream point location when `primPath` is empty
- playback remapping using `frame`, `frameOffset`, and `animationBehavior`
- injecting output under intermediate branch paths while preserving the surrounding scene state

Current playback behavior:

- `Hold` freezes to `frame + frameOffset`
- `Repeat` follows the current context frame with those values applied as offsets

## Current behavior

`PointCloudPlus` is a practical point-generation and output node for Gaffer. It emits `PointsPrimitive` output into the scene, preserves the surrounding input scene, and supports the modes and playback controls that are already present in the repository.

The current node and tests cover:

- random surface point generation
- primitive-center point generation
- file-mode output of upstream point data
- file-mode auto-discovery of an upstream point location when `primPath` is empty
- playback remapping through `frame`, `frameOffset`, and `animationBehavior`
- output insertion under intermediate branch paths while preserving ancestor transforms and attributes

## Current boundaries

This document describes the shipped point-generation and output workflows that exist in the repository now. It does not describe a broader point-cloud import system beyond the controls, demos, and regression coverage already included with `PointCloudPlus`.

## Relationship to the README

Use `gaffer_pointcloud_plus/README.md` for build, runtime, test, and launch commands.

Use this document for the current design and workflow shape of `PointCloudPlus`.
