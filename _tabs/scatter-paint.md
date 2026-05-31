---
title: Scatter Paint
icon: fas fa-paintbrush
order: 2
---

# Gaffer Scatter Paint

Scatter Paint is the hand-placed scatter plugin in this set.

Use it when a shot needs specific placement: grass touch-up, rocks, leaves, bolts, debris, footprints, dressing fixes, or any detail that needs an artist's eye.

<iframe width="560" height="315" src="https://www.youtube.com/embed/R47paQI2cdY?si=AWoMOpYbvQ6hVp1D" title="YouTube video player" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" referrerpolicy="strict-origin-when-cross-origin" allowfullscreen></iframe>

## What It Gives You

- `PaintedPoints` to store painted scatter data
- `AttachedPoints` to keep those points attached to geometry
- `StaticPoints` to bake a final result
- `PaintPointsTool` for interactive painting in the viewer

## Good Uses For It

- painting placement by hand on a surface
- organizing work into layers and strokes
- muting, soloing, or timing layers for shot work
- baking a painted result into a simpler output node
- exporting or validating the stored paint data when needed

Typical examples:

- hand-fixing a grass pass near a hero prop
- placing extra rocks, leaves, or bolts where the eye goes first
- painting breakup on top of a broader procedural pass
- doing quick shot-specific dressing without rebuilding the whole scatter rig

## What You See In Gaffer

After the plugins are loaded, Gaffer adds:

- Scatter nodes in the node menu
- Scatter Paint tools and utility actions in the top menus
- demo setups you can use as starting points
- benchmark scenes for testing heavier paint cases

## Notes

If you want install help, go to `Install`.

If you want the more technical pages, go to `Developer Notes` from the home page.


![gui-left.png](/assets/images/painter/gui-left.png)

![gui-right.png](/assets/images/painter/gui-right.png)
![pane01.png](/assets/images/painter/pane01.png)
![pane02.png](/assets/images/painter/pane02.png)
![pane03.png](/assets/images/painter/pane03.png)
![pane04.png](/assets/images/painter/pane04.png)
