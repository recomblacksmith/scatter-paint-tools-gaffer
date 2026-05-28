---
title: Stats
icon: fas fa-gauge-high
order: 9
---

# Scatter Paint Stats

These numbers are a rough performance snapshot from recent real paint and erase runs.

This page is mostly for technical review. If you are just using the tool, the main takeaway is whether paint and erase stay responsive enough for shot work.

Average point counts used for normalization:

- paint committed/resolved points: `36,696.6`
- erase removed points: `31,063.2`
- buffered/densified stroke input points: `110.2`

## Per 10k Points

### Erase

- `totalMs`: `9.87ms / 10k`
- `brushEraseMs`: `7.06ms / 10k`
- `loadSchemaMs`: `2.22ms / 10k`
- `pointScanMs`: `1.44ms / 10k`
- `writeMs`: `2.35ms / 10k`
- `writePackMs`: `0.26ms / 10k`

Main takeaway: erase behavior is in a workable range, but very small updates still pay a noticeable write cost.

### Paint / Write

- `totalMs`: `18.52ms / 10k`
- `sampleBuildMs`: `0.70ms / 10k`
- `brushPaintMs`: `11.46ms / 10k`
- `surfaceResolveMs`: `5.47ms / 10k`
- `writeMs`: `3.62ms / 10k`
- `writePackMs`: `3.59ms / 10k`

Main takeaway: larger paint strokes are no longer dominated by the old blob-set bottleneck, but writing and packing the result still has a measurable cost.
