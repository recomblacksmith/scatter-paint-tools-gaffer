---
title: Scatter Paint
icon: fas fa-paintbrush
order: 3
---

# Gaffer Scatter Paint

Scatter Paint is the hand-placed scatter tool in this set.

Use it when you want an artist-driven result instead of fully procedural scatter.

## What It Gives You

- `PaintedPoints` to store painted scatter data
- `AttachedPoints` to stick those points back onto geometry
- `StaticPoints` to bake a final result
- `PaintPointsTool` for interactive painting in the viewer

## Good Uses For It

- painting placement by hand on a surface
- organizing work into layers and strokes
- muting, soloing, or timing layers for shot work
- baking a painted result into a simpler output node
- exporting or validating the stored paint data when needed

## Runtime Registration

After the toolset is loaded, Gaffer adds:

- Scatter nodes in the node menu
- Scatter Paint tools and utility actions in the top menus
- demo setups you can use as starting points
- benchmark scenes for testing heavier paint cases

## More Detail

Use the other pages if you need more detail:

- Build
- Nodes
- Cache Schema
- Status
- Stats
