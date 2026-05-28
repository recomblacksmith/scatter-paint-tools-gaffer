---
layout: page
title: Status Details
permalink: /developer-notes/status-details/
---

# Scatter Paint Status Details

## Current State

- `PaintedPoints` stores painted scatter data and the related utility actions
- `AttachedPoints` turns that stored data into output geometry
- `StaticPoints` is the bake target for frozen results
- `PaintPointsTool` gives you the viewport painting workflow

## Completed

1. The painted data path is working end to end.
2. Cache save/load behavior is covered by regression tests.
3. Attachment solve behavior is covered by regression tests.
4. Bake and export helpers are present.
5. Authored color overrides are supported.
6. The packaged-runtime test path is green for the main Scatter Paint suites.

## Things To Be Aware Of

1. A Python compatibility path still exists alongside the compiled path.
2. Some startup compatibility shims are still part of the launch path.
3. Some runtime edit helpers still use older internal data forms even though cache IO itself is native.

## Current Boundaries

1. The Python compatibility path is still part of the shipped tool.
2. Internal dict/schema conversion is still part of the current implementation.
3. Partial cache rewrite optimization is not part of the current shipped behavior.
