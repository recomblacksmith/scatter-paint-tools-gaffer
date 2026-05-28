# ruff: noqa
from . import _core_shared as _shared

import copy
import time

globals().update(
    {name: value for name, value in vars(_shared).items() if not name.startswith("__")}
)

from ._core_attached_points import AttachedPoints
from ._core_static_points import StaticPoints

_shared.AttachedPoints = AttachedPoints


def __pointChunkSlices(self, points):
    points = list(points or [])
    return [
        points[index : index + CHUNK_POINT_LIMIT]
        for index in range(0, len(points), CHUNK_POINT_LIMIT)
    ] or [[]]


def __strokeFrameActive(self, stroke, current_frame):
    frame_start = int(stroke.get("frameStart", 0))
    frame_end = int(stroke.get("frameEnd", 0))
    if frame_start == 0 and frame_end == 0:
        return True
    if frame_start > frame_end:
        frame_start, frame_end = frame_end, frame_start
    return frame_start <= current_frame <= frame_end


def __layerFrameActive(self, layer, current_frame):
    frame_start = int(layer.get("frameStart", 0))
    frame_end = int(layer.get("frameEnd", 0))
    if frame_start == 0 and frame_end == 0:
        return True
    if frame_start > frame_end:
        frame_start, frame_end = frame_end, frame_start
    if frame_start <= current_frame <= frame_end:
        return True
    return bool(layer.get("holdOutsideRange", True))


def __activeStrokeIdsForFrame(self, store, current_frame):
    visible_layers = [
        layer
        for layer in sorted(
            store.get("layers", []), key=lambda item: item.get("order", 0)
        )
        if layer.get("enabled", True)
        and layer.get("visible", True)
        and not layer.get("mute", False)
        and self._PaintedPoints__layerFrameActive(layer, current_frame)
    ]
    if any(layer.get("solo", False) for layer in visible_layers):
        visible_layers = [layer for layer in visible_layers if layer.get("solo", False)]

    active_stroke_ids = []
    for layer in visible_layers:
        layer_strokes = [
            stroke
            for stroke in sorted(
                store.get("strokes", []), key=lambda item: item.get("order", 0)
            )
            if int(stroke.get("layerId", 0)) == int(layer.get("layerId", 0))
            and self._PaintedPoints__strokeFrameActive(stroke, current_frame)
        ]
        active_stroke_ids.extend(int(stroke["strokeId"]) for stroke in layer_strokes)
    return active_stroke_ids


def __nextId(self, store, key):
    next_id = int(store["nextIds"][key])
    store["nextIds"][key] = next_id + 1
    return next_id


def __layerNames(self, store):
    return [
        layer["name"]
        for layer in sorted(store["layers"], key=lambda item: item.get("order", 0))
    ]


def __selectionSetNames(self, store):
    return [selection_set["name"] for selection_set in store["selectionSets"]]


def __iterStrokes(self, store):
    layers_by_id = {layer["layerId"]: layer for layer in store["layers"]}
    for stroke in sorted(
        store["strokes"],
        key=lambda item: (item.get("layerId", 0), item.get("order", 0)),
    ):
        yield layers_by_id.get(stroke.get("layerId")), stroke


def __iterPoints(self, store):
    for layer, stroke in self._PaintedPoints__iterStrokes(store):
        for point in self._PaintedPoints__strokePoints(store, stroke.get("strokeId")):
            yield layer, stroke, point


def __selectedPointIds(self, store):
    current_selection = dict(store.get("currentSelection", {}))
    selected_stroke_ids = {
        int(stroke_id) for stroke_id in current_selection.get("strokeIds", [])
    }
    selected_point_ids = {
        int(point_id) for point_id in current_selection.get("pointIds", [])
    }
    if not selected_stroke_ids:
        return selected_point_ids

    for point in store.get("points", []):
        if int(point.get("strokeId", 0)) in selected_stroke_ids:
            selected_point_ids.add(int(point.get("pointId", 0)))
    return selected_point_ids


def __pruneSelectionState(self, store):
    remaining_point_ids = {
        int(point.get("pointId", 0)) for point in store.get("points", [])
    }
    remaining_stroke_ids = {
        int(stroke.get("strokeId", 0)) for stroke in store.get("strokes", [])
    }

    current_selection = store.setdefault(
        "currentSelection", {"pointIds": [], "strokeIds": []}
    )
    current_selection["pointIds"] = [
        int(point_id)
        for point_id in current_selection.get("pointIds", [])
        if int(point_id) in remaining_point_ids
    ]
    current_selection["strokeIds"] = [
        int(stroke_id)
        for stroke_id in current_selection.get("strokeIds", [])
        if int(stroke_id) in remaining_stroke_ids
    ]

    for selection_set in store.get("selectionSets", []):
        selection_set["pointIds"] = [
            int(point_id)
            for point_id in selection_set.get("pointIds", [])
            if int(point_id) in remaining_point_ids
        ]
        selection_set["strokeIds"] = [
            int(stroke_id)
            for stroke_id in selection_set.get("strokeIds", [])
            if int(stroke_id) in remaining_stroke_ids
        ]


def __strokePoints(self, store, stroke_id):
    return [
        point
        for point in store["points"]
        if int(point.get("strokeId", 0)) == int(stroke_id)
    ]


def __scenePathFromId(self, store, path_id):
    if not path_id:
        return ""
    scene_paths = list(store.get("scenePaths", []))
    index = int(path_id) - 1
    if index < 0 or index >= len(scene_paths):
        return ""
    return str(scene_paths[index])


def __internScenePath(self, store, scene_path):
    scene_path = str(scene_path or "")
    if not scene_path:
        return 0
    scene_paths = store.setdefault("scenePaths", [])
    try:
        return scene_paths.index(scene_path) + 1
    except ValueError:
        scene_paths.append(scene_path)
        return len(scene_paths)


def __internInstanceSourcePath(self, store, scene_path):
    scene_path = str(scene_path or "")
    if not scene_path:
        return 0
    scene_paths = store.setdefault("instanceSourcePaths", [])
    try:
        return scene_paths.index(scene_path) + 1
    except ValueError:
        scene_paths.append(scene_path)
        return len(scene_paths)


def __applyExpandedPointEdit(self, store, point, edited_point):
    edited_point = dict(edited_point)
    node_metadata = dict(store.get("node", {}))
    layer = next(
        (
            candidate
            for candidate in store.get("layers", [])
            if int(candidate.get("layerId", 0)) == int(point.get("layerId", 0))
        ),
        None,
    )
    stroke = next(
        (
            candidate
            for candidate in store.get("strokes", [])
            if int(candidate.get("strokeId", 0)) == int(point.get("strokeId", 0))
        ),
        None,
    )
    inherited_color = _resolve_authored_color(node_metadata, layer, stroke, None)
    point["targetPathId"] = self._PaintedPoints__internScenePath(
        store, edited_point.get("sourcePath", "")
    )
    point["instanceSourcePathId"] = self._PaintedPoints__internInstanceSourcePath(
        store, edited_point.get("instanceSourcePath", "")
    )
    point["triangleIndex"] = int(
        edited_point.get("triangleIndex", point.get("triangleIndex", 0))
    )
    point["barycentric"] = [
        float(value)
        for value in list(
            edited_point.get("barycentric", point.get("barycentric", [1.0, 0.0, 0.0]))
        )[:3]
    ]
    point["restWorldP"] = [
        float(value)
        for value in list(
            edited_point.get("P", point.get("restWorldP", [0.0, 0.0, 0.0]))
        )[:3]
    ]
    point["restObjectP"] = [
        float(value)
        for value in list(
            edited_point.get(
                "restObjectP", point.get("restObjectP", point["restWorldP"])
            )
        )[:3]
    ]
    point["restUV"] = [
        float(value)
        for value in list(edited_point.get("restUV", point.get("restUV", [0.0, 0.0])))[
            :2
        ]
    ]
    point["restNormal"] = [
        float(value)
        for value in list(
            edited_point.get("N", point.get("restNormal", [0.0, 1.0, 0.0]))
        )[:3]
    ]
    point["restUp"] = [
        float(value)
        for value in list(edited_point.get("up", point.get("restUp", [0.0, 0.0, 1.0])))[
            :3
        ]
    ]
    point["width"] = float(edited_point.get("width", point.get("width", 1.0)))
    point["uniformScale"] = float(
        edited_point.get(
            "scale",
            edited_point.get("uniformScale", point.get("uniformScale", 1.0)),
        )
    )
    point["seed"] = int(edited_point.get("seed", point.get("seed", 0)))
    point["normalSpin"] = float(
        edited_point.get("normalSpin", point.get("normalSpin", 0.0))
    )
    point["tangentRotation"] = [
        float(value)
        for value in list(
            edited_point.get(
                "tangentRotation", point.get("tangentRotation", [0.0, 0.0])
            )
        )[:2]
    ]
    point["pressureDensity"] = float(
        edited_point.get("pressureDensity", point.get("pressureDensity", 1.0))
    )
    point["pressureSoftness"] = float(
        edited_point.get("pressureSoftness", point.get("pressureSoftness", 1.0))
    )
    point["valid"] = bool(edited_point.get("valid", point.get("valid", True)))
    point["lastValidFrame"] = int(
        edited_point.get("lastValidFrame", point.get("lastValidFrame", 0))
    )
    point["anchorModeUsed"] = 0 if edited_point.get("attachmentResolved", True) else 3
    point["topologyGeneration"] = int(
        edited_point.get("topologyGeneration", point.get("topologyGeneration", 0))
    )
    edited_color = _color3f_from_values(
        edited_point.get("color", point.get("color", inherited_color)), inherited_color
    )
    color_enabled = bool(
        edited_point.get("colorEnabled", point.get("colorEnabled", False))
    )
    if "color" in edited_point and "colorEnabled" not in edited_point:
        color_enabled = not _color_equal(edited_color, inherited_color)
    point["colorEnabled"] = color_enabled
    point["color"] = _color_list(edited_color)


def __defaultPointRecord(self, store, layer, stroke, point_data=None):
    point_data = dict(point_data or {})
    inherited_color = _resolve_authored_color(
        store.get("node", {}), layer, stroke, None
    )
    point_color = _color3f_from_values(
        point_data.get("color", inherited_color), inherited_color
    )
    color_enabled = bool(point_data.get("colorEnabled", "color" in point_data))
    barycentric = list(point_data.get("barycentric", [1.0, 0.0, 0.0]))
    if len(barycentric) != 3:
        barycentric = [1.0, 0.0, 0.0]

    world_position = list(
        point_data.get("restWorldP", point_data.get("P", [0.0, 0.0, 0.0]))
    )
    if len(world_position) != 3:
        world_position = [0.0, 0.0, 0.0]
    object_position = list(point_data.get("restObjectP", world_position))
    if len(object_position) != 3:
        object_position = list(world_position)
    rest_uv = list(point_data.get("restUV", [0.0, 0.0]))
    if len(rest_uv) != 2:
        rest_uv = [0.0, 0.0]
    normal = list(point_data.get("N", point_data.get("restNormal", [0.0, 1.0, 0.0])))
    if len(normal) != 3:
        normal = [0.0, 1.0, 0.0]
    up = list(point_data.get("up", point_data.get("restUp", [0.0, 0.0, 1.0])))
    if len(up) != 3:
        up = [0.0, 0.0, 1.0]
    tangent_rotation = list(point_data.get("tangentRotation", [0.0, 0.0]))
    if len(tangent_rotation) != 2:
        tangent_rotation = [0.0, 0.0]

    return {
        "pointId": point_data.get(
            "pointId", self._PaintedPoints__nextId(store, "point")
        ),
        "strokeId": stroke["strokeId"],
        "layerId": layer["layerId"],
        "targetPathId": self._PaintedPoints__internScenePath(
            store, point_data.get("sourcePath", "")
        ),
        "instanceId": int(point_data.get("instanceId", 0)),
        "instanceSourcePathId": self._PaintedPoints__internInstanceSourcePath(
            store, point_data.get("instanceSourcePath", "")
        ),
        "triangleIndex": int(point_data.get("triangleIndex", 0)),
        "barycentric": [float(value) for value in barycentric],
        "restObjectP": [float(value) for value in object_position],
        "restWorldP": [float(value) for value in world_position],
        "restUV": [float(value) for value in rest_uv],
        "restNormal": [float(value) for value in normal],
        "restUp": [float(value) for value in up],
        "width": float(point_data.get("width", 1.0)),
        "uniformScale": float(
            point_data.get("scale", point_data.get("uniformScale", 1.0))
        ),
        "seed": int(point_data.get("seed", 0)),
        "normalSpin": float(point_data.get("normalSpin", 0.0)),
        "tangentRotation": [float(value) for value in tangent_rotation],
        "pressureDensity": float(point_data.get("pressureDensity", 1.0)),
        "pressureSoftness": float(point_data.get("pressureSoftness", 1.0)),
        "valid": bool(point_data.get("valid", True)),
        "lastValidFrame": int(point_data.get("lastValidFrame", 0)),
        "anchorModeUsed": 0 if point_data.get("attachmentResolved", False) else 3,
        "topologyGeneration": int(point_data.get("topologyGeneration", 0)),
        "colorEnabled": color_enabled,
        "color": _color_list(point_color),
    }


def __refreshDerivedData(self, store, changed_stroke_ids=None):
    changed_stroke_ids = {int(value) for value in list(changed_stroke_ids or [])}
    chunk_by_stroke = {
        (int(chunk.get("strokeId", 0)), int(chunk.get("chunkIndex", 0))): chunk
        for chunk in store.get("chunks", [])
    }
    point_start = 0
    new_chunks = []
    for layer in sorted(store["layers"], key=lambda item: item.get("order", 0)):
        layer_strokes = sorted(
            [
                stroke
                for stroke in store["strokes"]
                if int(stroke.get("layerId", 0)) == int(layer.get("layerId", 0))
            ],
            key=lambda item: item.get("order", 0),
        )
        layer["firstStrokeId"] = layer_strokes[0]["strokeId"] if layer_strokes else 0
        layer["lastStrokeId"] = layer_strokes[-1]["strokeId"] if layer_strokes else 0
        for stroke in layer_strokes:
            stroke_points = self._PaintedPoints__strokePoints(store, stroke["strokeId"])
            stroke["pointCount"] = len(stroke_points)
            stroke["targetCount"] = len(
                {
                    int(point.get("targetPathId", 0))
                    for point in stroke_points
                    if int(point.get("targetPathId", 0))
                }
            )
            existing_stroke_chunks = sorted(
                [
                    chunk
                    for chunk in store.get("chunks", [])
                    if int(chunk.get("strokeId", 0)) == int(stroke["strokeId"])
                ],
                key=lambda item: item.get("chunkIndex", 0),
            )
            existing_generation = max(
                [int(chunk.get("generation", 0)) for chunk in existing_stroke_chunks],
                default=0,
            )
            generation = existing_generation
            if int(stroke["strokeId"]) in changed_stroke_ids:
                generation += 1
            stroke_chunks = []
            for chunk_index, chunk_points in enumerate(
                self._PaintedPoints__pointChunkSlices(stroke_points)
            ):
                existing_chunk = chunk_by_stroke.get(
                    (int(stroke["strokeId"]), chunk_index)
                )
                chunk = {
                    "chunkId": (
                        existing_chunk.get(
                            "chunkId", self._PaintedPoints__nextId(store, "chunk")
                        )
                        if existing_chunk
                        else self._PaintedPoints__nextId(store, "chunk")
                    ),
                    "strokeId": stroke["strokeId"],
                    "chunkIndex": chunk_index,
                    "pointStart": point_start,
                    "pointCount": len(chunk_points),
                    "generation": generation,
                    "deleted": False,
                }
                point_start += len(chunk_points)
                stroke_chunks.append(chunk)
                new_chunks.append(chunk)
            stroke["firstChunkId"] = stroke_chunks[0]["chunkId"] if stroke_chunks else 0
            stroke["lastChunkId"] = stroke_chunks[-1]["chunkId"] if stroke_chunks else 0
    store["chunks"] = new_chunks


def __resolveLayer(self, store, layer_identifier):
    for index, layer in enumerate(store["layers"]):
        if layer_identifier == layer.get("layerId") or layer_identifier == layer.get(
            "name"
        ):
            return index, layer
    raise ValueError(f"Unknown layer: {layer_identifier}")


def __resolveStroke(self, store, stroke_identifier):
    layers_by_id = {
        layer["layerId"]: (index, layer) for index, layer in enumerate(store["layers"])
    }
    for stroke_index, stroke in enumerate(store["strokes"]):
        if stroke_identifier == stroke.get(
            "strokeId"
        ) or stroke_identifier == stroke.get("name"):
            layer_index, layer = layers_by_id.get(stroke.get("layerId"), (None, None))
            return layer_index, stroke_index, layer, stroke
    raise ValueError(f"Unknown stroke: {stroke_identifier}")


def __resolveSelectionSet(self, store, selection_identifier):
    for index, selection_set in enumerate(store["selectionSets"]):
        if selection_identifier == selection_set.get(
            "selectionSetId"
        ) or selection_identifier == selection_set.get("name"):
            return index, selection_set
    raise ValueError(f"Unknown selection set: {selection_identifier}")


def __reindexLayers(self, store):
    for index, layer in enumerate(store["layers"]):
        layer["order"] = index


def __reindexStrokes(self, store, layer_identifier):
    ordered_strokes = [
        stroke
        for stroke in store["strokes"]
        if int(stroke.get("layerId", 0)) == int(layer_identifier)
    ]
    ordered_strokes.sort(key=lambda item: item.get("order", 0))
    for index, stroke in enumerate(ordered_strokes):
        stroke["order"] = index


def __uniqueName(self, existing_names, prefix):
    existing = set(existing_names)
    index = 1
    while True:
        candidate = f"{prefix} {index}"
        if candidate not in existing:
            return candidate
        index += 1


def createLayer(self, name=None):
    def mutator(store):
        layer_name = name or self._PaintedPoints__uniqueName(
            self._PaintedPoints__layerNames(store), "Layer"
        )
        layer = {
            "layerId": self._PaintedPoints__nextId(store, "layer"),
            "name": layer_name,
            "order": len(store["layers"]),
            "enabled": True,
            "visible": True,
            "mute": False,
            "solo": False,
            "timeVarying": False,
            "mode": 0,
            "frameStart": 0,
            "frameEnd": 0,
            "holdOutsideRange": True,
            "firstStrokeId": 0,
            "lastStrokeId": 0,
            "colorEnabled": False,
            "color": _color_list(DEFAULT_AUTHORED_COLOR),
        }
        store["layers"].append(layer)
        self._PaintedPoints__reindexLayers(store)
        return layer["layerId"]

    return self.mutateCacheStore(mutator)


def deleteLayer(self, layer_identifier):
    def mutator(store):
        index, layer = self._PaintedPoints__resolveLayer(store, layer_identifier)
        layer_id = int(layer["layerId"])
        stroke_ids = [
            stroke["strokeId"]
            for stroke in store["strokes"]
            if int(stroke.get("layerId", 0)) == layer_id
        ]
        store["strokes"] = [
            stroke
            for stroke in store["strokes"]
            if int(stroke.get("layerId", 0)) != layer_id
        ]
        store["points"] = [
            point
            for point in store["points"]
            if int(point.get("layerId", 0)) != layer_id
        ]
        store["chunks"] = [
            chunk
            for chunk in store["chunks"]
            if int(chunk.get("strokeId", 0)) not in set(stroke_ids)
        ]
        del store["layers"][index]
        self._PaintedPoints__reindexLayers(store)
        return layer["layerId"]

    return self.mutateCacheStore(mutator)


def renameLayer(self, layer_identifier, new_name):
    def mutator(store):
        _index, layer = self._PaintedPoints__resolveLayer(store, layer_identifier)
        layer["name"] = new_name
        return layer["layerId"]

    return self.mutateCacheStore(mutator)


def moveLayer(self, layer_identifier, new_index):
    def mutator(store):
        index, layer = self._PaintedPoints__resolveLayer(store, layer_identifier)
        layer = store["layers"].pop(index)
        store["layers"].insert(max(0, min(new_index, len(store["layers"]))), layer)
        self._PaintedPoints__reindexLayers(store)
        return layer["layerId"]

    return self.mutateCacheStore(mutator)


def mergeLayers(self, source_layer_identifier, destination_layer_identifier):
    def mutator(store):
        source_index, source_layer = self._PaintedPoints__resolveLayer(
            store, source_layer_identifier
        )
        destination_index, destination_layer = self._PaintedPoints__resolveLayer(
            store, destination_layer_identifier
        )
        if source_index == destination_index:
            return destination_layer["layerId"]

        source_layer_id = int(source_layer["layerId"])
        destination_layer_id = int(destination_layer["layerId"])
        for stroke in store["strokes"]:
            if int(stroke.get("layerId", 0)) == source_layer_id:
                stroke["layerId"] = destination_layer_id
        for point in store["points"]:
            if int(point.get("layerId", 0)) == source_layer_id:
                point["layerId"] = destination_layer_id
        del store["layers"][source_index]
        self._PaintedPoints__reindexLayers(store)
        self._PaintedPoints__reindexStrokes(store, destination_layer_id)
        return destination_layer["layerId"]

    return self._PaintedPoints__mutateStore(mutator)


def setLayerVisible(self, layer_identifier, visible):
    def mutator(store):
        _index, layer = self._PaintedPoints__resolveLayer(store, layer_identifier)
        layer["visible"] = bool(visible)
        return layer["layerId"]

    return self._PaintedPoints__mutateStore(mutator)


def setLayerMute(self, layer_identifier, mute):
    def mutator(store):
        _index, layer = self._PaintedPoints__resolveLayer(store, layer_identifier)
        layer["mute"] = bool(mute)
        return layer["layerId"]

    return self._PaintedPoints__mutateStore(mutator)


def setLayerSolo(self, layer_identifier, solo):
    def mutator(store):
        _index, layer = self._PaintedPoints__resolveLayer(store, layer_identifier)
        layer["solo"] = bool(solo)
        return layer["layerId"]

    return self._PaintedPoints__mutateStore(mutator)


def setLayerTimeRange(self, layer_identifier, frame_start, frame_end):
    def mutator(store):
        _index, layer = self._PaintedPoints__resolveLayer(store, layer_identifier)
        layer["frameStart"] = int(frame_start)
        layer["frameEnd"] = int(frame_end)
        return layer["layerId"]

    return self._PaintedPoints__mutateStore(mutator)


def createStroke(self, layer_identifier, name=None):
    def mutator(store):
        _index, layer = self._PaintedPoints__resolveLayer(store, layer_identifier)
        layer_id = int(layer["layerId"])
        existing_names = [
            stroke["name"]
            for stroke in store["strokes"]
            if int(stroke.get("layerId", 0)) == layer_id
        ]
        stroke_name = name or self._PaintedPoints__uniqueName(existing_names, "Stroke")
        stroke = {
            "strokeId": self._PaintedPoints__nextId(store, "stroke"),
            "layerId": layer_id,
            "name": stroke_name,
            "order": len(existing_names),
            "mode": 0,
            "frameStart": 0,
            "frameEnd": 0,
            "createdTimeUnixMicros": _unix_time_micros(),
            "firstChunkId": 0,
            "lastChunkId": 0,
            "pointCount": 0,
            "targetCount": 0,
            "selectionMaskId": 0,
            "colorEnabled": False,
            "color": _color_list(DEFAULT_AUTHORED_COLOR),
        }
        store["strokes"].append(stroke)
        self._PaintedPoints__reindexStrokes(store, layer_id)
        return stroke["strokeId"]

    return self._PaintedPoints__mutateStore(mutator)


def deleteStroke(self, stroke_identifier):
    def mutator(store):
        layer_index, stroke_index, layer, stroke = self._PaintedPoints__resolveStroke(
            store, stroke_identifier
        )
        stroke_id = int(stroke["strokeId"])
        del store["strokes"][stroke_index]
        store["points"] = [
            point
            for point in store["points"]
            if int(point.get("strokeId", 0)) != stroke_id
        ]
        store["chunks"] = [
            chunk
            for chunk in store["chunks"]
            if int(chunk.get("strokeId", 0)) != stroke_id
        ]
        self._PaintedPoints__reindexStrokes(store, layer["layerId"])
        return stroke["strokeId"]

    return self._PaintedPoints__mutateStore(mutator)


def renameStroke(self, stroke_identifier, new_name):
    def mutator(store):
        _layer_index, _stroke_index, _layer, stroke = (
            self._PaintedPoints__resolveStroke(store, stroke_identifier)
        )
        stroke["name"] = new_name
        return stroke["strokeId"]

    return self._PaintedPoints__mutateStore(mutator)


def moveStroke(self, stroke_identifier, new_index):
    def mutator(store):
        layer_index, stroke_index, layer, stroke = self._PaintedPoints__resolveStroke(
            store, stroke_identifier
        )
        layer_strokes = sorted(
            [
                entry
                for entry in store["strokes"]
                if int(entry.get("layerId", 0)) == int(layer["layerId"])
            ],
            key=lambda item: item.get("order", 0),
        )
        layer_strokes.pop(
            next(
                index
                for index, entry in enumerate(layer_strokes)
                if entry["strokeId"] == stroke["strokeId"]
            )
        )
        layer_strokes.insert(max(0, min(new_index, len(layer_strokes))), stroke)
        for index, entry in enumerate(layer_strokes):
            entry["order"] = index
        return stroke["strokeId"]

    return self._PaintedPoints__mutateStore(mutator)


def mergeStrokes(self, source_stroke_identifier, destination_stroke_identifier):
    def mutator(store):
        source_layer_index, source_stroke_index, source_layer, source_stroke = (
            self._PaintedPoints__resolveStroke(store, source_stroke_identifier)
        )
        (
            destination_layer_index,
            destination_stroke_index,
            destination_layer,
            destination_stroke,
        ) = self._PaintedPoints__resolveStroke(store, destination_stroke_identifier)

        if source_stroke_index == destination_stroke_index:
            return destination_stroke["strokeId"]

        source_stroke_id = int(source_stroke["strokeId"])
        destination_stroke_id = int(destination_stroke["strokeId"])
        for point in store["points"]:
            if int(point.get("strokeId", 0)) == source_stroke_id:
                point["strokeId"] = destination_stroke_id
                point["layerId"] = destination_layer["layerId"]
        del store["strokes"][source_stroke_index]
        store["chunks"] = [
            chunk
            for chunk in store["chunks"]
            if int(chunk.get("strokeId", 0)) != source_stroke_id
        ]
        self._PaintedPoints__reindexStrokes(store, source_layer["layerId"])
        if source_layer_index != destination_layer_index:
            self._PaintedPoints__reindexStrokes(store, destination_layer["layerId"])
        self._PaintedPoints__refreshDerivedData(
            store,
            changed_stroke_ids=[destination_stroke_id],
        )
        return destination_stroke["strokeId"]

    return self._PaintedPoints__mutateStore(mutator, refresh=False)


def paintStrokeCommit(self, stroke_identifier, points, append=True):
    def mutator(store):
        _layer_index, _stroke_index, _layer, stroke = (
            self._PaintedPoints__resolveStroke(store, stroke_identifier)
        )
        layer = next(
            layer
            for layer in store["layers"]
            if int(layer.get("layerId", 0)) == int(stroke.get("layerId", 0))
        )
        authored_points = [
            self._PaintedPoints__defaultPointRecord(store, layer, stroke, point_data)
            for point_data in list(points)
        ]

        if append:
            store["points"].extend(authored_points)
        else:
            stroke_id = int(stroke["strokeId"])
            store["points"] = [
                point
                for point in store["points"]
                if int(point.get("strokeId", 0)) != stroke_id
            ]
            store["points"].extend(authored_points)
        self._PaintedPoints__refreshDerivedData(
            store, changed_stroke_ids=[stroke["strokeId"]]
        )
        return len(authored_points)

    return self._PaintedPoints__mutateStore(
        mutator,
        refresh=False,
        mutation_label="paintStrokeCommit",
    )


def eraseCommit(self, stroke_identifier, point_ids=None, fraction=None):
    def mutator(store):
        _layer_index, _stroke_index, _layer, stroke = (
            self._PaintedPoints__resolveStroke(store, stroke_identifier)
        )
        existing_points = self._PaintedPoints__strokePoints(store, stroke["strokeId"])

        if point_ids is not None:
            point_ids_set = {int(point_id) for point_id in point_ids}
            remaining_points = [
                point
                for point in existing_points
                if int(point.get("pointId", -1)) not in point_ids_set
            ]
        elif fraction is not None:
            remove_count = max(
                0,
                min(
                    len(existing_points),
                    int(round(len(existing_points) * float(fraction))),
                ),
            )
            remaining_points = existing_points[remove_count:]
        else:
            remaining_points = []

        removed_count = len(existing_points) - len(remaining_points)
        stroke_id = int(stroke["strokeId"])
        store["points"] = [
            point
            for point in store["points"]
            if int(point.get("strokeId", 0)) != stroke_id
        ]
        store["points"].extend(remaining_points)
        self._PaintedPoints__refreshDerivedData(store, changed_stroke_ids=[stroke_id])
        return removed_count

    return self._PaintedPoints__mutateStore(mutator, refresh=False)


def splitStrokeBySelection(self):
    def mutator(store):
        selected_point_ids = self._PaintedPoints__selectedPointIds(store)
        if not selected_point_ids:
            return "Removed 0 selected points. Stroke selection was unchanged."

        current_selection = store.setdefault(
            "currentSelection", {"pointIds": [], "strokeIds": []}
        )
        selected_stroke_ids = {
            int(stroke_id) for stroke_id in current_selection.get("strokeIds", [])
        }
        affected_stroke_ids = set()
        removed_count = 0
        replacement_strokes = []
        replacement_points = []

        next_order_by_layer = {}
        for stroke in store.get("strokes", []):
            layer_id = int(stroke.get("layerId", 0))
            next_order_by_layer[layer_id] = max(
                next_order_by_layer.get(layer_id, -1), int(stroke.get("order", 0))
            )

        surviving_strokes = []
        for stroke in store.get("strokes", []):
            stroke_id = int(stroke.get("strokeId", 0))
            stroke_points = self._PaintedPoints__strokePoints(store, stroke_id)
            selected_for_stroke = [
                point
                for point in stroke_points
                if int(point.get("pointId", 0)) in selected_point_ids
            ]
            if not selected_for_stroke:
                surviving_strokes.append(stroke)
                continue

            affected_stroke_ids.add(stroke_id)
            removed_count += len(selected_for_stroke)

            surviving_segments = []
            current_segment = []
            for point in stroke_points:
                if int(point.get("pointId", 0)) in selected_point_ids:
                    if current_segment:
                        surviving_segments.append(current_segment)
                        current_segment = []
                    continue
                current_segment.append(point)
            if current_segment:
                surviving_segments.append(current_segment)

            if not surviving_segments:
                continue

            if len(selected_for_stroke) == 0 and stroke_id not in selected_stroke_ids:
                surviving_strokes.append(stroke)
                continue

            for segment in surviving_segments:
                replacement_stroke = copy.deepcopy(stroke)
                replacement_stroke["strokeId"] = self._PaintedPoints__nextId(
                    store, "stroke"
                )
                layer_id = int(stroke.get("layerId", 0))
                next_order_by_layer[layer_id] = (
                    next_order_by_layer.get(layer_id, -1) + 1
                )
                replacement_stroke["order"] = next_order_by_layer[layer_id]
                replacement_strokes.append(replacement_stroke)
                for point in segment:
                    replacement_point = copy.deepcopy(point)
                    replacement_point["strokeId"] = replacement_stroke["strokeId"]
                    replacement_points.append(replacement_point)

        if not affected_stroke_ids:
            return "Removed 0 selected points. Stroke selection was unchanged."

        store["strokes"] = surviving_strokes + replacement_strokes
        store["points"] = [
            point
            for point in store.get("points", [])
            if int(point.get("strokeId", 0)) not in affected_stroke_ids
            and int(point.get("pointId", 0)) not in selected_point_ids
        ]
        store["points"].extend(replacement_points)

        remaining_point_ids = {
            int(point.get("pointId", 0)) for point in store.get("points", [])
        }
        remaining_stroke_ids = {
            int(stroke.get("strokeId", 0)) for stroke in store.get("strokes", [])
        }
        current_selection["pointIds"] = [
            int(point_id)
            for point_id in current_selection.get("pointIds", [])
            if int(point_id) in remaining_point_ids
        ]
        current_selection["strokeIds"] = [
            int(stroke_id)
            for stroke_id in current_selection.get("strokeIds", [])
            if int(stroke_id) in remaining_stroke_ids
        ]
        for selection_set in store.get("selectionSets", []):
            selection_set["pointIds"] = [
                int(point_id)
                for point_id in selection_set.get("pointIds", [])
                if int(point_id) in remaining_point_ids
            ]
            selection_set["strokeIds"] = [
                int(stroke_id)
                for stroke_id in selection_set.get("strokeIds", [])
                if int(stroke_id) in remaining_stroke_ids
            ]

        self._PaintedPoints__refreshDerivedData(
            store, changed_stroke_ids=affected_stroke_ids
        )
        return f"Removed {removed_count} selected points. Stroke selection was updated."

    return self._PaintedPoints__mutateStore(mutator, refresh=False)


def createSelectionSet(self, name=None):
    def mutator(store):
        selection_name = name or self._PaintedPoints__uniqueName(
            self._PaintedPoints__selectionSetNames(store),
            "Selection",
        )
        selection_set = {
            "selectionSetId": self._PaintedPoints__nextId(store, "selectionSet"),
            "name": selection_name,
            "pointIds": [],
            "strokeIds": [],
        }
        store["selectionSets"].append(selection_set)
        return selection_set["selectionSetId"]

    return self._PaintedPoints__mutateStore(mutator)


def renameSelectionSet(self, selection_identifier, new_name):
    def mutator(store):
        _index, selection_set = self._PaintedPoints__resolveSelectionSet(
            store, selection_identifier
        )
        selection_set["name"] = new_name
        return selection_set["selectionSetId"]

    return self._PaintedPoints__mutateStore(mutator)


def deleteSelectionSet(self, selection_identifier):
    def mutator(store):
        index, selection_set = self._PaintedPoints__resolveSelectionSet(
            store, selection_identifier
        )
        del store["selectionSets"][index]
        return selection_set["selectionSetId"]

    return self._PaintedPoints__mutateStore(mutator)


def storeCurrentSelection(self, selection_identifier=None, name=None):
    def mutator(store):
        current_selection = dict(store.get("currentSelection", {}))
        if selection_identifier is None:
            selection_set = {
                "selectionSetId": self._PaintedPoints__nextId(store, "selectionSet"),
                "name": name
                or self._PaintedPoints__uniqueName(
                    self._PaintedPoints__selectionSetNames(store),
                    "Selection",
                ),
                "pointIds": list(current_selection.get("pointIds", [])),
                "strokeIds": list(current_selection.get("strokeIds", [])),
            }
            store["selectionSets"].append(selection_set)
            return selection_set["selectionSetId"]

        _index, selection_set = self._PaintedPoints__resolveSelectionSet(
            store, selection_identifier
        )
        selection_set["pointIds"] = list(current_selection.get("pointIds", []))
        selection_set["strokeIds"] = list(current_selection.get("strokeIds", []))
        if name is not None:
            selection_set["name"] = name
        return selection_set["selectionSetId"]

    return self._PaintedPoints__mutateStore(mutator)


def setCurrentSelection(self, point_ids=None, stroke_ids=None):
    point_ids = [int(point_id) for point_id in (point_ids or [])]
    stroke_ids = [int(stroke_id) for stroke_id in (stroke_ids or [])]

    store, error = self._PaintedPoints__loadStore()
    if error:
        raise RuntimeError(error)

    current_selection = dict(
        store.get("currentSelection", {"pointIds": [], "strokeIds": []})
    )
    current_point_ids = [
        int(point_id) for point_id in current_selection.get("pointIds", [])
    ]
    current_stroke_ids = [
        int(stroke_id) for stroke_id in current_selection.get("strokeIds", [])
    ]
    if current_point_ids == point_ids and current_stroke_ids == stroke_ids:
        return {
            "pointIds": list(current_point_ids),
            "strokeIds": list(current_stroke_ids),
        }

    def mutator(store):
        store["currentSelection"] = {
            "pointIds": list(point_ids),
            "strokeIds": list(stroke_ids),
        }
        return dict(store["currentSelection"])

    return self._PaintedPoints__mutateStore(
        mutator,
        refresh=False,
        mutation_label="setCurrentSelection",
    )


def layerRecords(self):
    return copy.deepcopy(self.cacheSnapshot()["layers"])


def strokeRecords(self):
    return copy.deepcopy(self.cacheSnapshot()["strokes"])


def pointRecords(self):
    store = self.cacheSnapshot()
    return [
        self._PaintedPoints__expandedPointRecord(store, point)
        for point in store.get("points", [])
    ]


def ensureLayer(self, name):
    store, error = self._PaintedPoints__loadStore()
    if error:
        raise RuntimeError(error)
    for layer in store.get("layers", []):
        if layer.get("name") == name:
            return int(layer["layerId"])
    return self.createLayer(name)


def ensureStroke(self, layer_identifier, name):
    store, error = self._PaintedPoints__loadStore()
    if error:
        raise RuntimeError(error)
    _index, layer = self._PaintedPoints__resolveLayer(store, layer_identifier)
    layer_id = int(layer["layerId"])
    for stroke in store.get("strokes", []):
        if int(stroke.get("layerId", 0)) == layer_id and stroke.get("name") == name:
            return int(stroke["strokeId"])
    return self.createStroke(layer_id, name)


def ensureLayerAndStroke(self, layer_name=None, stroke_name=None):
    total_start = time.perf_counter()
    layer_name = str(layer_name or "").strip() or "Layer 1"
    stroke_name = str(stroke_name or "").strip() or "Stroke 1"

    load_store_start = time.perf_counter()
    store, error = self._PaintedPoints__loadStore()
    load_store_ms = (time.perf_counter() - load_store_start) * 1000.0
    if error:
        raise RuntimeError(error)

    layer_lookup_start = time.perf_counter()
    layer = next(
        (
            candidate
            for candidate in store.get("layers", [])
            if candidate.get("name") == layer_name
        ),
        None,
    )
    layer_lookup_ms = (time.perf_counter() - layer_lookup_start) * 1000.0

    layer_created = False
    stroke_created = False
    write_ms = 0.0

    if layer is None:
        layer = {
            "layerId": self._PaintedPoints__nextId(store, "layer"),
            "name": layer_name,
            "order": len(store["layers"]),
            "enabled": True,
            "visible": True,
            "mute": False,
            "solo": False,
            "timeVarying": False,
            "mode": 0,
            "frameStart": 0,
            "frameEnd": 0,
            "holdOutsideRange": True,
            "firstStrokeId": 0,
            "lastStrokeId": 0,
            "colorEnabled": False,
            "color": _color_list(DEFAULT_AUTHORED_COLOR),
        }
        store["layers"].append(layer)
        self._PaintedPoints__reindexLayers(store)
        layer_created = True

    layer_id = int(layer["layerId"])
    stroke_lookup_start = time.perf_counter()
    stroke = next(
        (
            candidate
            for candidate in store.get("strokes", [])
            if int(candidate.get("layerId", 0)) == layer_id
            and candidate.get("name") == stroke_name
        ),
        None,
    )
    stroke_lookup_ms = (time.perf_counter() - stroke_lookup_start) * 1000.0

    if stroke is None:
        existing_strokes = [
            candidate
            for candidate in store.get("strokes", [])
            if int(candidate.get("layerId", 0)) == layer_id
        ]
        stroke = {
            "strokeId": self._PaintedPoints__nextId(store, "stroke"),
            "layerId": layer_id,
            "name": stroke_name,
            "order": len(existing_strokes),
            "mode": 0,
            "frameStart": 0,
            "frameEnd": 0,
            "createdTimeUnixMicros": _unix_time_micros(),
            "firstChunkId": 0,
            "lastChunkId": 0,
            "pointCount": 0,
            "targetCount": 0,
            "selectionMaskId": 0,
            "colorEnabled": False,
            "color": _color_list(DEFAULT_AUTHORED_COLOR),
        }
        store["strokes"].append(stroke)
        self._PaintedPoints__reindexStrokes(store, layer_id)
        stroke_created = True

    if layer_created or stroke_created:
        write_start = time.perf_counter()
        self._PaintedPoints__finalizeStoreMutation(store, refresh=False)
        write_ms = (time.perf_counter() - write_start) * 1000.0

    return {
        "layerId": layer_id,
        "strokeId": int(stroke["strokeId"]),
        "layerCreated": layer_created,
        "strokeCreated": stroke_created,
        "loadStoreMs": load_store_ms,
        "layerLookupMs": layer_lookup_ms,
        "strokeLookupMs": stroke_lookup_ms,
        "writeMs": write_ms,
        "totalMs": (time.perf_counter() - total_start) * 1000.0,
    }


def seedBenchmarkStroke(self, point_count, layer_name=None, stroke_name=None):
    point_count = max(0, int(point_count))
    layer_name = str(layer_name or "Benchmark Layer").strip() or "Benchmark Layer"
    stroke_name = (
        str(stroke_name or f"Benchmark Stroke {point_count}").strip()
        or f"Benchmark Stroke {point_count}"
    )

    def mutator(store):
        store["layers"] = []
        store["strokes"] = []
        store["chunks"] = []
        store["points"] = []
        store["selectionSets"] = []
        store["currentSelection"] = {"pointIds": [], "strokeIds": []}
        store["pointBackups"] = {}
        store["upgrades"] = []
        store["scenePaths"] = ["/benchmarkScatter"]
        store["instanceSourcePaths"] = []
        store["diagnostics"] = self._PaintedPoints__emptyDiagnostics()
        store["nextIds"] = {
            "layer": 1,
            "stroke": 1,
            "point": 1,
            "selectionSet": 1,
            "chunk": 1,
        }

        layer_id = self._PaintedPoints__nextId(store, "layer")
        stroke_id = self._PaintedPoints__nextId(store, "stroke")
        target_path_id = 1

        layer = {
            "layerId": layer_id,
            "name": layer_name,
            "order": 0,
            "enabled": True,
            "visible": True,
            "mute": False,
            "solo": False,
            "timeVarying": False,
            "mode": 0,
            "frameStart": 0,
            "frameEnd": 0,
            "holdOutsideRange": True,
            "firstStrokeId": stroke_id,
            "lastStrokeId": stroke_id,
            "colorEnabled": False,
            "color": _color_list(DEFAULT_AUTHORED_COLOR),
        }
        stroke = {
            "strokeId": stroke_id,
            "layerId": layer_id,
            "name": stroke_name,
            "order": 0,
            "mode": 0,
            "frameStart": 0,
            "frameEnd": 0,
            "createdTimeUnixMicros": _unix_time_micros(),
            "firstChunkId": 0,
            "lastChunkId": 0,
            "pointCount": point_count,
            "targetCount": 1 if point_count else 0,
            "selectionMaskId": 0,
            "colorEnabled": False,
            "color": _color_list(DEFAULT_AUTHORED_COLOR),
        }

        store["layers"] = [layer]
        store["strokes"] = [stroke]

        columns = int(max(1, round(point_count**0.5)))
        rows = int((point_count + columns - 1) / columns) if point_count else 1
        usable_columns = max(1, columns - 1)
        usable_rows = max(1, rows - 1)
        margin = 0.04

        points = []
        append_point = points.append
        for index in range(point_count):
            column = index % columns
            row = index // columns
            u = column / float(usable_columns) if usable_columns else 0.5
            v = row / float(usable_rows) if usable_rows else 0.5
            u = margin + (1.0 - margin * 2.0) * u
            v = margin + (1.0 - margin * 2.0) * v
            x = u - 0.5
            z = v - 0.5
            if u + v <= 1.0:
                triangle_index = 0
                barycentric = [1.0 - u - v, u, v]
            else:
                triangle_index = 1
                barycentric = [1.0 - v, u + v - 1.0, 1.0 - u]

            append_point(
                {
                    "pointId": self._PaintedPoints__nextId(store, "point"),
                    "strokeId": stroke_id,
                    "layerId": layer_id,
                    "targetPathId": target_path_id,
                    "instanceId": 0,
                    "instanceSourcePathId": 0,
                    "triangleIndex": triangle_index,
                    "barycentric": [
                        float(barycentric[0]),
                        float(barycentric[1]),
                        float(barycentric[2]),
                    ],
                    "restObjectP": [float(x), 0.01, float(z)],
                    "restWorldP": [float(x), 0.01, float(z)],
                    "restUV": [float(u), float(v)],
                    "restNormal": [0.0, 1.0, 0.0],
                    "restUp": [0.0, 0.0, 1.0],
                    "width": 0.085,
                    "uniformScale": 1.0,
                    "seed": 5000 + index,
                    "normalSpin": 0.0,
                    "tangentRotation": [0.0, 0.0],
                    "pressureDensity": 1.0,
                    "pressureSoftness": 1.0,
                    "valid": True,
                    "lastValidFrame": 0,
                    "anchorModeUsed": 0,
                    "topologyGeneration": 0,
                    "colorEnabled": False,
                    "color": _color_list(DEFAULT_AUTHORED_COLOR),
                }
            )

        store["points"] = points

        chunks = []
        point_start = 0
        chunk_count = (point_count + CHUNK_POINT_LIMIT - 1) // CHUNK_POINT_LIMIT
        for chunk_index in range(chunk_count):
            chunk_point_count = min(CHUNK_POINT_LIMIT, point_count - point_start)
            chunk_id = self._PaintedPoints__nextId(store, "chunk")
            chunks.append(
                {
                    "chunkId": chunk_id,
                    "strokeId": stroke_id,
                    "chunkIndex": chunk_index,
                    "pointStart": point_start,
                    "pointCount": chunk_point_count,
                    "generation": 1,
                    "deleted": False,
                }
            )
            point_start += chunk_point_count

        store["chunks"] = chunks
        stroke["firstChunkId"] = chunks[0]["chunkId"] if chunks else 0
        stroke["lastChunkId"] = chunks[-1]["chunkId"] if chunks else 0

        return {
            "layerId": layer_id,
            "strokeId": stroke_id,
            "pointCount": point_count,
            "chunkCount": len(chunks),
        }

    return self._PaintedPoints__mutateStore(
        mutator,
        refresh=False,
        mutation_label="seedBenchmarkStroke",
        copy_store=False,
    )


def lastStrokeId(self):
    store, error = self._PaintedPoints__loadStore()
    if error:
        raise RuntimeError(error)
    if not store.get("strokes"):
        return None
    ordered_strokes = sorted(
        store["strokes"],
        key=lambda item: (item.get("layerId", 0), item.get("order", 0)),
    )
    return int(ordered_strokes[-1]["strokeId"])


def mutatePoints(self, mutator):
    def mutate(store):
        updated_count = 0
        changed_stroke_ids = set()
        for point in store.get("points", []):
            expanded = self._PaintedPoints__expandedPointRecord(store, point)
            if mutator(expanded):
                self._PaintedPoints__applyExpandedPointEdit(store, point, expanded)
                changed_stroke_ids.add(int(point.get("strokeId", 0)))
                updated_count += 1
        self._PaintedPoints__refreshDerivedData(
            store, changed_stroke_ids=changed_stroke_ids
        )
        return updated_count

    return self._PaintedPoints__mutateStore(mutate, refresh=False)


def totalAuthoredPointCount(self):
    cached_count = getattr(self, "_PaintedPoints__authoredPointCount", None)
    if cached_count is not None:
        return int(cached_count)
    store = self.cacheSnapshot()
    return len(store.get("points", []))
