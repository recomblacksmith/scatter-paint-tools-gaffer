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


def __expandedPointRecord(self, store, point):
    record = copy.deepcopy(point)
    point_id = int(point.get("pointId", 0) or 0)
    point_backups = dict(store.get("pointBackups", {}))
    if point_id:
        backup = point_backups.get(point_id)
        if backup is None:
            backup = point_backups.get(str(point_id))
        if isinstance(backup, dict):
            record["_demoAttachmentBackup"] = {
                "sourcePath": self._PaintedPoints__scenePathFromId(
                    store, backup.get("targetPathId", 0)
                ),
                "triangleIndex": int(backup.get("triangleIndex", 0) or 0),
                "barycentric": list(backup.get("barycentric", [])),
                "attachmentResolved": bool(backup.get("attachmentResolved", False)),
            }
    record["sourcePath"] = self._PaintedPoints__scenePathFromId(
        store, point.get("targetPathId", 0)
    )
    record["instanceSourcePath"] = self._PaintedPoints__scenePathFromId(
        {"scenePaths": store.get("instanceSourcePaths", [])},
        point.get("instanceSourcePathId", 0),
    )
    record["P"] = list(point.get("restWorldP", [0.0, 0.0, 0.0]))
    record["N"] = list(point.get("restNormal", [0.0, 1.0, 0.0]))
    record["up"] = list(point.get("restUp", [0.0, 0.0, 1.0]))
    record["scale"] = float(point.get("uniformScale", 1.0))
    record["attachmentResolved"] = int(point.get("anchorModeUsed", 3)) != 3
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
    authored_color = _resolve_authored_color(
        store.get("node", {}), layer, stroke, point
    )
    record["colorEnabled"] = bool(point.get("colorEnabled", False))
    record["color"] = list(point.get("color", _color_list(authored_color)))
    record["authoredColor"] = _color_list(authored_color)
    record["scatterColor"] = _color_list(authored_color)
    return record


def __authoredBrushPoints(self, store, samples):
    paint_through_mode = int(self["paintThroughMode"].getValue())
    filtered_mode = paint_through_mode == PAINT_THROUGH_MODE_FILTERED_BRUSH_VOLUME
    precomputed_candidates = (
        self._PaintedPoints__surfaceCandidates(store, None) if filtered_mode else None
    )
    authored_points = []
    resolved_count = 0
    fallback_count = 0
    for sample in list(samples or []):
        sample = dict(sample or {})
        surface_points = self._PaintedPoints__brushSampleSurfacePoints(
            store,
            sample,
            filtered=filtered_mode,
            precomputed_candidates=precomputed_candidates,
        )
        if not surface_points:
            authored_points.append(self._PaintedPoints__authoredBrushPoint(sample))
            fallback_count += 1
            continue

        resolved_count += len(surface_points)
        for surface_index, surface_point in enumerate(surface_points):
            authored_points.append(
                self._PaintedPoints__authoredBrushPoint(
                    sample, surface_point, surface_index
                )
            )

    return authored_points, resolved_count, fallback_count


def __surfacePointData(
    self,
    store,
    scene_path,
    world_position,
    instance_id=0,
    instance_source_path="",
    scene_object=None,
    full_transform=None,
    preferred_triangle_index=None,
):
    scene_path = str(scene_path or "").strip()
    if not scene_path:
        return None

    if scene_object is None or full_transform is None:
        if not self["in"].exists(scene_path):
            return None
        try:
            scene_object = self["in"].object(scene_path, _copy=False)
            full_transform = self["in"].fullTransform(scene_path)
        except Exception:
            return None

    runtime_cache = self._PaintedPoints__surfaceRuntimeCache(store)
    triangle_data = runtime_cache["triangleData"].get(scene_path)
    if triangle_data is None:
        triangle_data = _mesh_triangle_data(scene_object)
        runtime_cache["triangleData"][scene_path] = triangle_data
    if not triangle_data:
        return None

    inverse_transform = runtime_cache["inverseTransforms"].get(scene_path)
    if inverse_transform is None:
        try:
            inverse_transform = full_transform.inverse()
        except Exception:
            return None
        runtime_cache["inverseTransforms"][scene_path] = inverse_transform

    object_position = imath.V3f(world_position) * inverse_transform
    best_candidate = None
    best_distance_squared = None
    for triangle in triangle_data["triangles"]:
        try:
            positions = triangle_data["positions"]
            a = positions[triangle[1]]
            b = positions[triangle[2]]
            c = positions[triangle[3]]
        except Exception:
            continue

        closest_object, barycentric = _closest_point_on_triangle(
            object_position, a, b, c
        )
        world_point = closest_object * full_transform
        delta = world_point - world_position
        distance_squared = delta.dot(delta)
        if (
            best_distance_squared is not None
            and distance_squared > best_distance_squared
        ):
            continue
        if (
            best_distance_squared is not None
            and abs(distance_squared - best_distance_squared) <= 1e-9
            and preferred_triangle_index is not None
            and int(triangle[0]) != int(preferred_triangle_index)
            and best_candidate is not None
            and int(best_candidate.get("triangleIndex", -1))
            == int(preferred_triangle_index)
        ):
            continue

        face_normal = _normalized(_cross(b - a, c - a), imath.V3f(0, 1, 0))
        object_normal = _triangle_normal(
            scene_object, triangle, barycentric, face_normal
        )
        object_up = _triangle_up(a, b, c, object_normal)
        world_normal = _normalized(
            full_transform.multDirMatrix(object_normal), imath.V3f(0, 1, 0)
        )
        world_up = _orthogonalized(
            full_transform.multDirMatrix(object_up),
            world_normal,
            imath.V3f(0, 0, 1),
        )
        best_candidate = {
            "sourcePath": scene_path,
            "instanceId": int(instance_id),
            "instanceSourcePath": str(instance_source_path or ""),
            "triangleIndex": int(triangle[0]),
            "barycentric": [float(value) for value in barycentric],
            "P": [
                float(world_point.x),
                float(world_point.y),
                float(world_point.z),
            ],
            "restObjectP": [
                float(closest_object.x),
                float(closest_object.y),
                float(closest_object.z),
            ],
            "restWorldP": [
                float(world_point.x),
                float(world_point.y),
                float(world_point.z),
            ],
            "restUV": [float(barycentric[1]), float(barycentric[2])],
            "N": [
                float(world_normal.x),
                float(world_normal.y),
                float(world_normal.z),
            ],
            "up": [float(world_up.x), float(world_up.y), float(world_up.z)],
            "attachmentResolved": True,
        }
        best_distance_squared = distance_squared

    return best_candidate


def __surfaceCandidates(self, store, point=None):
    allowed_paths = self._PaintedPoints__surfaceAllowedPaths()
    return self._PaintedPoints__surfaceCandidatesWithAllowedPaths(
        store, allowed_paths, point
    )


def __surfaceAllowedPaths(self):
    explicit_filter = _path_filter_paths(self["targetFilter"].getValue())
    set_filter = _set_filter_paths(self["in"], self["targetSetFilter"].getValue())
    return explicit_filter | set_filter


def __surfaceCandidateForPath(
    self, store, scene_path, source_path=None, instance_id=0, instance_source_path=""
):
    scene_path = str(scene_path or "").strip()
    if not scene_path or scene_path == "/" or not self["in"].exists(scene_path):
        return None

    runtime_cache = self._PaintedPoints__surfaceRuntimeCache(store)
    cached = runtime_cache["pathCandidates"].get(scene_path)
    if cached is None:
        try:
            scene_object = self["in"].object(scene_path, _copy=False)
            full_transform = self["in"].fullTransform(scene_path)
        except Exception:
            return None

        if _mesh_triangle_data(scene_object) is None:
            runtime_cache["pathCandidates"][scene_path] = False
            return None

        cached = {
            "object": scene_object,
            "transform": full_transform,
        }
        runtime_cache["pathCandidates"][scene_path] = cached
    elif cached is False:
        return None

    return {
        "sourcePath": str(source_path or scene_path),
        "instanceId": int(instance_id),
        "instanceSourcePath": str(instance_source_path or ""),
        "object": cached["object"],
        "transform": cached["transform"],
    }


def __surfaceRuntimeCache(self, store):
    cache = store.get("_surfaceRuntimeCache")
    if cache is None:
        cache = {
            "pathCandidates": {},
            "triangleData": {},
            "inverseTransforms": {},
        }
        store["_surfaceRuntimeCache"] = cache
    return cache


def __surfaceCandidatesForSamples(self, store, samples):
    allowed_paths = self._PaintedPoints__surfaceAllowedPaths()
    candidates = []
    seen = set()

    for sample in list(samples or []):
        sample = dict(sample or {})
        source_path = str(sample.get("sourcePath", "")).strip()
        instance_source_path = str(sample.get("instanceSourcePath", "") or "")
        instance_id = int(sample.get("instanceId", 0))

        if instance_source_path and instance_id:
            instance_path = f"{instance_source_path}/{instance_id}"
            if not allowed_paths or instance_path in allowed_paths:
                key = (
                    instance_path,
                    source_path or instance_path,
                    instance_id,
                    instance_source_path,
                )
                if key not in seen:
                    seen.add(key)
                    candidate = self._PaintedPoints__surfaceCandidateForPath(
                        store,
                        instance_path,
                        source_path=source_path or instance_path,
                        instance_id=instance_id,
                        instance_source_path=instance_source_path,
                    )
                    if candidate is not None:
                        candidates.append(candidate)

        if source_path and (not allowed_paths or source_path in allowed_paths):
            key = (source_path, source_path, 0, "")
            if key not in seen:
                seen.add(key)
                candidate = self._PaintedPoints__surfaceCandidateForPath(
                    store, source_path
                )
                if candidate is not None:
                    candidates.append(candidate)

    if candidates:
        return candidates

    return self._PaintedPoints__surfaceCandidatesWithAllowedPaths(
        store, allowed_paths, None
    )


def __surfaceCandidatesWithAllowedPaths(self, store, allowed_paths, point=None):
    allowed_paths = set(allowed_paths or [])

    selected_instance_ids = set()
    if point is not None:
        current_selection = dict(store.get("currentSelection", {}))
        selected_point_ids = {
            int(point_id) for point_id in current_selection.get("pointIds", [])
        }
        if len(selected_point_ids) > 1:
            for selected_point in store.get("points", []):
                if int(selected_point.get("pointId", 0)) in selected_point_ids:
                    selected_instance_ids.add(int(selected_point.get("instanceId", 0)))

    candidates = []
    point_instance_source_path = (
        str(point.get("instanceSourcePath", "")) if isinstance(point, dict) else ""
    )
    point_source_path = (
        str(point.get("sourcePath", "")) if isinstance(point, dict) else ""
    )
    for scene_path in _walk_scene_paths(self["in"]):
        scene_path_string = _scene_path_to_string(scene_path)
        if scene_path_string == "/":
            continue

        if point_instance_source_path and scene_path_string.startswith(
            point_instance_source_path + "/"
        ):
            leaf_name = scene_path_string.rsplit("/", 1)[-1]
            try:
                instance_id = int(leaf_name)
            except Exception:
                instance_id = None
            if instance_id is not None:
                if allowed_paths and scene_path_string not in allowed_paths:
                    continue
                if instance_id != int(point.get("instanceId", 0)):
                    if not selected_instance_ids:
                        continue
                    if instance_id not in selected_instance_ids:
                        continue
                candidate = self._PaintedPoints__surfaceCandidateForPath(
                    store,
                    scene_path_string,
                    source_path=point_source_path or scene_path_string,
                    instance_id=instance_id,
                    instance_source_path=point_instance_source_path,
                )
                if candidate is not None:
                    candidates.append(candidate)
                continue

        base_allowed = not allowed_paths or scene_path_string in allowed_paths
        if base_allowed:
            candidate = self._PaintedPoints__surfaceCandidateForPath(
                store, scene_path_string
            )
            if candidate is not None:
                candidates.append(candidate)
    if candidates:
        return candidates

    if point is None:
        return []
    current_path = str(point.get("sourcePath", "")).strip()
    if not current_path or not self["in"].exists(current_path):
        return []

    instance_source_path = str(point.get("instanceSourcePath", "") or "")
    current_instance_id = int(point.get("instanceId", 0))
    if instance_source_path and current_instance_id:
        instance_path = f"{instance_source_path}/{current_instance_id}"
        candidate = self._PaintedPoints__surfaceCandidateForPath(
            store,
            instance_path,
            source_path=current_path,
            instance_id=current_instance_id,
            instance_source_path=instance_source_path,
        )
    else:
        candidate = self._PaintedPoints__surfaceCandidateForPath(store, current_path)
    return [candidate] if candidate is not None else []


def __brushSampleWorldPosition(self, sample):
    return _vector_from_values(
        sample.get("point", sample.get("P", sample.get("restWorldP", [0.0, 0.0, 0.0]))),
        [0.0, 0.0, 0.0],
    )


def __brushSampleSurfacePoints(
    self, store, sample, filtered=False, precomputed_candidates=None
):
    sample = dict(sample or {})
    source_path = str(sample.get("sourcePath", "")).strip()
    if not source_path and not filtered:
        return []

    if precomputed_candidates is not None:
        candidates = precomputed_candidates
    elif filtered:
        candidates = self._PaintedPoints__surfaceCandidates(store, sample)
    else:
        candidates = [
            {
                "sourcePath": source_path,
                "instanceId": int(sample.get("instanceId", 0)),
                "instanceSourcePath": str(sample.get("instanceSourcePath", "") or ""),
                "object": None,
                "transform": None,
            }
        ]

    world_position = self._PaintedPoints__brushSampleWorldPosition(sample)
    preferred_triangle_index = sample.get("triangleIndex", None)
    preferred_source_path = source_path
    results = []
    seen = set()
    for candidate in candidates:
        candidate_source_path = str(candidate.get("sourcePath", "")).strip()
        if not candidate_source_path:
            continue
        surface_point = self._PaintedPoints__surfacePointData(
            store,
            candidate_source_path,
            world_position,
            instance_id=int(candidate.get("instanceId", 0)),
            instance_source_path=str(candidate.get("instanceSourcePath", "") or ""),
            scene_object=candidate.get("object", None),
            full_transform=candidate.get("transform", None),
            preferred_triangle_index=(
                preferred_triangle_index
                if candidate_source_path == preferred_source_path
                else None
            ),
        )
        if surface_point is None:
            continue

        barycentric = list(surface_point.get("barycentric", [1.0, 0.0, 0.0]))[:3]
        key = (
            str(surface_point.get("sourcePath", "")),
            int(surface_point.get("instanceId", 0)),
            str(surface_point.get("instanceSourcePath", "") or ""),
            int(surface_point.get("triangleIndex", 0)),
            tuple(round(float(value), 6) for value in barycentric),
        )
        if key in seen:
            continue
        seen.add(key)
        results.append(surface_point)

    return results


def __authoredBrushPoint(self, sample, surface_point=None, seed_offset=0):
    sample = dict(sample or {})
    authored = dict(surface_point or {})

    width = float(sample.get("width", 1.0))
    scale = float(sample.get("scale", sample.get("uniformScale", 1.0)))
    pressure_density = float(sample.get("pressureDensity", 1.0))
    pressure_softness = float(sample.get("pressureSoftness", 1.0))
    seed = int(sample.get("seed", 0)) + int(seed_offset)
    valid = bool(sample.get("valid", True))

    base_world_position = (
        _vector_from_values(surface_point.get("P", [0.0, 0.0, 0.0]), [0.0, 0.0, 0.0])
        if surface_point is not None
        else self._PaintedPoints__brushSampleWorldPosition(sample)
    )
    normal = _normalized(
        _vector_from_values(
            authored.get("N", sample.get("N", sample.get("normal", [0.0, 1.0, 0.0]))),
            [0.0, 1.0, 0.0],
        ),
        imath.V3f(0.0, 1.0, 0.0),
    )
    display_lift = max(width * 0.1, 0.01)
    lifted_world_position = base_world_position + (normal * display_lift)

    authored["P"] = [
        float(lifted_world_position.x),
        float(lifted_world_position.y),
        float(lifted_world_position.z),
    ]
    authored["width"] = width
    authored["scale"] = scale
    authored["pressureDensity"] = pressure_density
    authored["pressureSoftness"] = pressure_softness
    authored["seed"] = seed
    authored["valid"] = valid
    authored["N"] = [float(normal.x), float(normal.y), float(normal.z)]

    if surface_point is None:
        authored["sourcePath"] = str(sample.get("sourcePath", "") or "")
        authored["instanceId"] = int(sample.get("instanceId", 0))
        authored["instanceSourcePath"] = str(sample.get("instanceSourcePath", "") or "")
        authored["triangleIndex"] = int(sample.get("triangleIndex", 0))
        authored["barycentric"] = [
            float(value)
            for value in list(sample.get("barycentric", [1.0, 0.0, 0.0]))[:3]
        ]
        authored["attachmentResolved"] = bool(sample.get("attachmentResolved", False))
        if "up" in sample:
            authored["up"] = [float(value) for value in list(sample.get("up", []))[:3]]

    return authored


def brushPaintPoints(self, samples):
    store = self.cacheSnapshot()
    authored_points, _resolved_count, _fallback_count = (
        self._PaintedPoints__authoredBrushPoints(store, samples)
    )
    return authored_points


def brushPaintCommit(self, stroke_identifier, samples, append=True):
    def mutator(store):
        authored_build_start = time.perf_counter()
        _layer_index, _stroke_index, _layer, stroke = (
            self._PaintedPoints__resolveStroke(store, stroke_identifier)
        )
        layer = next(
            layer
            for layer in store["layers"]
            if int(layer.get("layerId", 0)) == int(stroke.get("layerId", 0))
        )
        authored_samples, resolved_count, fallback_count = (
            self._PaintedPoints__authoredBrushPoints(store, samples)
        )
        authored_points = [
            self._PaintedPoints__defaultPointRecord(store, layer, stroke, point_data)
            for point_data in authored_samples
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
        return {
            "committed": len(authored_points),
            "resolved": resolved_count,
            "fallback": fallback_count,
            "authoredBuildMs": (time.perf_counter() - authored_build_start) * 1000.0,
        }

    return self._PaintedPoints__mutateStore(
        mutator,
        refresh=False,
        mutation_label="brushPaintCommit",
    )


def brushErasePoints(self, samples, radius, erase_space=0):
    total_start = time.perf_counter()
    radius = max(0.001, float(radius))
    radius_squared = radius * radius
    erase_space = int(erase_space)
    samples = [dict(sample or {}) for sample in list(samples or [])]

    compiled_erase = getattr(self, "_PaintedPoints__compiledBrushErasePoints", None)
    if erase_space == 0 and compiled_erase is not None:
        return compiled_erase(samples, radius, erase_space)

    def grouping_key(source_path, instance_id, instance_source_path):
        return (
            str(source_path or ""),
            int(instance_id or 0),
            str(instance_source_path or ""),
        )

    sample_groups = {}
    for sample in samples:
        key = grouping_key(
            sample.get("sourcePath", ""),
            sample.get("instanceId", 0),
            sample.get("instanceSourcePath", ""),
        )
        sample_groups.setdefault(key, []).append(
            self._PaintedPoints__brushSampleWorldPosition(sample)
        )

    def mutator(store):
        attachment_target_count = 0
        authored_point_count = len(store.get("points", []))
        target_precompute_ms = 0.0
        point_scan_ms = 0.0
        refresh_ms = 0.0
        if not samples:
            IECore.msg(
                IECore.Msg.Level.Info,
                "GafferScatterPaint.brushErasePoints",
                (
                    "totalMs={:.3f} targetPrecomputeMs={:.3f} pointScanMs={:.3f} "
                    "refreshMs={:.3f} sampleCount={} attachmentTargetCount={} "
                    "authoredPointCount={} removedCount=0 strokeCount=0 eraseSpace={}"
                ).format(
                    (time.perf_counter() - total_start) * 1000.0,
                    target_precompute_ms,
                    point_scan_ms,
                    refresh_ms,
                    len(samples),
                    attachment_target_count,
                    authored_point_count,
                    erase_space,
                ),
            )
            return {"removedCount": 0, "strokeCount": 0}

        attachment_targets = []
        attachment_target_groups = {}
        if erase_space != 0:
            target_precompute_start = time.perf_counter()
            precomputed_candidates = self._PaintedPoints__surfaceCandidates(store, None)
            for sample in samples:
                attachment_targets.extend(
                    self._PaintedPoints__brushSampleSurfacePoints(
                        store,
                        sample,
                        filtered=True,
                        precomputed_candidates=precomputed_candidates,
                    )
                )
            for attachment_target in attachment_targets:
                key = grouping_key(
                    attachment_target.get("sourcePath", ""),
                    attachment_target.get("instanceId", 0),
                    attachment_target.get("instanceSourcePath", ""),
                )
                attachment_target_groups.setdefault(key, []).append(
                    _vector_from_values(
                        attachment_target.get(
                            "restObjectP",
                            attachment_target.get("restWorldP", [0.0, 0.0, 0.0]),
                        ),
                        [0.0, 0.0, 0.0],
                    )
                )
            target_precompute_ms = (
                time.perf_counter() - target_precompute_start
            ) * 1000.0
            attachment_target_count = len(attachment_targets)

        scene_paths = list(store.get("scenePaths", []))
        instance_source_paths = list(store.get("instanceSourcePaths", []))
        point_ids_by_stroke = {}
        point_scan_start = time.perf_counter()
        for point in store.get("points", []):
            point_id = int(point.get("pointId", 0))
            stroke_id = int(point.get("strokeId", 0))
            if not point_id or not stroke_id:
                continue

            target_path_id = int(point.get("targetPathId", 0))
            point_source_path = (
                str(scene_paths[target_path_id - 1])
                if 0 < target_path_id <= len(scene_paths)
                else ""
            )
            point_instance_id = int(point.get("instanceId", 0))
            point_instance_source_path_id = int(point.get("instanceSourcePathId", 0))
            point_instance_source_path = (
                str(instance_source_paths[point_instance_source_path_id - 1])
                if 0 < point_instance_source_path_id <= len(instance_source_paths)
                else ""
            )
            point_group_key = grouping_key(
                point_source_path,
                point_instance_id,
                point_instance_source_path,
            )
            matches_brush = False

            if erase_space == 0:
                candidate_positions = sample_groups.get(point_group_key)
                if (
                    not candidate_positions
                    and point_instance_id == 0
                    and not point_instance_source_path
                ):
                    candidate_positions = sample_groups.get(
                        grouping_key(point_source_path, 0, "")
                    )
                if not candidate_positions:
                    continue

                point_position = _vector_from_values(
                    point.get("restWorldP", [0.0, 0.0, 0.0]), [0.0, 0.0, 0.0]
                )
                for sample_position in candidate_positions:
                    delta = point_position - sample_position
                    if delta.dot(delta) <= radius_squared:
                        matches_brush = True
                        break
            else:
                candidate_positions = attachment_target_groups.get(point_group_key)
                if not candidate_positions:
                    continue

                point_attachment_position = _vector_from_values(
                    point.get("restObjectP", point.get("restWorldP", [0.0, 0.0, 0.0])),
                    [0.0, 0.0, 0.0],
                )
                for target_attachment_position in candidate_positions:
                    delta = point_attachment_position - target_attachment_position
                    if delta.dot(delta) <= radius_squared:
                        matches_brush = True
                        break

            if not matches_brush:
                continue

            point_ids_by_stroke.setdefault(stroke_id, set()).add(point_id)

        point_scan_ms = (time.perf_counter() - point_scan_start) * 1000.0

        if not point_ids_by_stroke:
            IECore.msg(
                IECore.Msg.Level.Info,
                "GafferScatterPaint.brushErasePoints",
                (
                    "totalMs={:.3f} targetPrecomputeMs={:.3f} pointScanMs={:.3f} "
                    "refreshMs={:.3f} sampleCount={} attachmentTargetCount={} "
                    "authoredPointCount={} removedCount=0 strokeCount=0 eraseSpace={}"
                ).format(
                    (time.perf_counter() - total_start) * 1000.0,
                    target_precompute_ms,
                    point_scan_ms,
                    refresh_ms,
                    len(samples),
                    attachment_target_count,
                    authored_point_count,
                    erase_space,
                ),
            )
            return {"removedCount": 0, "strokeCount": 0}

        removed_point_ids = {
            point_id
            for point_ids in point_ids_by_stroke.values()
            for point_id in point_ids
        }
        changed_stroke_ids = set(point_ids_by_stroke)
        store["points"] = [
            point
            for point in store.get("points", [])
            if int(point.get("pointId", 0)) not in removed_point_ids
        ]
        refresh_start = time.perf_counter()
        self._PaintedPoints__pruneSelectionState(store)
        store["currentSelection"] = {"pointIds": [], "strokeIds": []}
        self._PaintedPoints__refreshDerivedData(
            store, changed_stroke_ids=changed_stroke_ids
        )
        refresh_ms = (time.perf_counter() - refresh_start) * 1000.0
        removed_count = len(removed_point_ids)
        stroke_count = len(changed_stroke_ids)
        IECore.msg(
            IECore.Msg.Level.Info,
            "GafferScatterPaint.brushErasePoints",
            (
                "totalMs={:.3f} targetPrecomputeMs={:.3f} pointScanMs={:.3f} "
                "refreshMs={:.3f} sampleCount={} attachmentTargetCount={} "
                "authoredPointCount={} removedCount={} strokeCount={} eraseSpace={}"
            ).format(
                (time.perf_counter() - total_start) * 1000.0,
                target_precompute_ms,
                point_scan_ms,
                refresh_ms,
                len(samples),
                attachment_target_count,
                authored_point_count,
                removed_count,
                stroke_count,
                erase_space,
            ),
        )
        return {
            "removedCount": removed_count,
            "strokeCount": stroke_count,
        }

    return self._PaintedPoints__mutateStore(
        mutator,
        refresh=False,
        mutation_label="brushErasePoints",
    )


def relaxSelection(self):
    store, error = self._PaintedPoints__loadStore()
    if error:
        raise RuntimeError(error)

    selected_point_ids = self._PaintedPoints__selectedPointIds(store)
    if not selected_point_ids:
        return 0

    current_frame = _current_frame(self)
    relax_objective = int(self["relaxObjective"].getValue())
    points_by_stroke = {}
    for point in store.get("points", []):
        stroke_id = int(point.get("strokeId", 0))
        points_by_stroke.setdefault(stroke_id, []).append(point)

    updated_count = 0
    changed_stroke_ids = set()
    for stroke_id, stroke_points in points_by_stroke.items():
        stroke_points = list(stroke_points)
        if len(stroke_points) < 2:
            continue
        for index, point in enumerate(stroke_points):
            if int(point.get("pointId", 0)) not in selected_point_ids:
                continue
            previous_point = (
                stroke_points[index - 1] if index > 0 else stroke_points[index]
            )
            next_point = (
                stroke_points[index + 1]
                if index + 1 < len(stroke_points)
                else stroke_points[index]
            )
            current_position = _vector_from_values(
                point.get("restWorldP", [0.0, 0.0, 0.0]), [0.0, 0.0, 0.0]
            )
            previous_position = _vector_from_values(
                previous_point.get("restWorldP", [0.0, 0.0, 0.0]),
                [0.0, 0.0, 0.0],
            )
            next_position = _vector_from_values(
                next_point.get("restWorldP", [0.0, 0.0, 0.0]), [0.0, 0.0, 0.0]
            )
            if (
                relax_objective == RELAX_OBJECTIVE_EVEN_REDISTRIBUTION
                and index > 0
                and index + 1 < len(stroke_points)
            ):
                smoothed_target = (previous_position + next_position) / 2.0
            else:
                smoothed_target = (
                    previous_position + current_position + next_position
                ) / 3.0
            candidate = self._PaintedPoints__surfacePointData(
                store,
                self._PaintedPoints__scenePathFromId(
                    store, point.get("targetPathId", 0)
                ),
                smoothed_target,
                instance_id=int(point.get("instanceId", 0)),
                instance_source_path=self._PaintedPoints__scenePathFromId(
                    {"scenePaths": store.get("instanceSourcePaths", [])},
                    point.get("instanceSourcePathId", 0),
                ),
                preferred_triangle_index=int(point.get("triangleIndex", -1)),
            )
            if candidate is None:
                continue
            candidate["lastValidFrame"] = current_frame
            candidate["topologyGeneration"] = current_frame
            self._PaintedPoints__applyExpandedPointEdit(store, point, candidate)
            point["valid"] = True
            point["anchorModeUsed"] = 0
            updated_count += 1
            changed_stroke_ids.add(stroke_id)

    if not updated_count:
        return 0

    self._PaintedPoints__refreshDerivedData(
        store, changed_stroke_ids=changed_stroke_ids
    )
    self._PaintedPoints__writeStore(store)
    self._PaintedPoints__syncStateFromStore()
    return updated_count


def reprojectSelection(self):
    store, error = self._PaintedPoints__loadStore()
    if error:
        raise RuntimeError(error)

    selected_point_ids = self._PaintedPoints__selectedPointIds(store)
    if not selected_point_ids:
        return 0

    current_frame = _current_frame(self)
    updated_count = 0
    changed_stroke_ids = set()
    for point in store.get("points", []):
        if int(point.get("pointId", 0)) not in selected_point_ids:
            continue
        expanded_point = self._PaintedPoints__expandedPointRecord(store, point)
        world_position = _vector_from_values(
            expanded_point.get("P", [0.0, 0.0, 0.0]), [0.0, 0.0, 0.0]
        )
        best_candidate = None
        best_distance_squared = None
        for candidate in self._PaintedPoints__surfaceCandidates(store, expanded_point):
            surface_point = self._PaintedPoints__surfacePointData(
                store,
                candidate["sourcePath"],
                world_position,
                instance_id=candidate["instanceId"],
                instance_source_path=candidate["instanceSourcePath"],
                scene_object=candidate["object"],
                full_transform=candidate["transform"],
            )
            if surface_point is None:
                continue
            candidate_position = _vector_from_values(
                surface_point.get("P", [0.0, 0.0, 0.0]), [0.0, 0.0, 0.0]
            )
            delta = candidate_position - world_position
            distance_squared = delta.dot(delta)
            if (
                best_distance_squared is not None
                and distance_squared >= best_distance_squared
            ):
                continue
            best_candidate = surface_point
            best_distance_squared = distance_squared

        if best_candidate is None:
            continue

        best_candidate["lastValidFrame"] = current_frame
        best_candidate["topologyGeneration"] = current_frame
        self._PaintedPoints__applyExpandedPointEdit(store, point, best_candidate)
        point["instanceId"] = int(best_candidate.get("instanceId", 0))
        point["valid"] = True
        point["anchorModeUsed"] = 0
        updated_count += 1
        changed_stroke_ids.add(int(point.get("strokeId", 0)))

    if not updated_count:
        return 0

    self._PaintedPoints__refreshDerivedData(
        store, changed_stroke_ids=changed_stroke_ids
    )
    self._PaintedPoints__writeStore(store)
    self._PaintedPoints__syncStateFromStore()
    return updated_count
