# ruff: noqa
from . import _core_shared as _shared

import copy

globals().update(
    {name: value for name, value in vars(_shared).items() if not name.startswith("__")}
)

from ._core_attached_points import AttachedPoints
from ._core_static_points import StaticPoints

_shared.AttachedPoints = AttachedPoints


def __exportPayload(self, include_resolved=False, scene=None):
    store, error = self._PaintedPoints__loadStore()
    if error:
        raise RuntimeError(error)
    payload = copy.deepcopy(store)
    payload["schemaVersion"] = store.get("schemaVersion", SCHEMA_VERSION)
    payload["validationSummary"] = self["validationSummary"].getValue()
    diagnostics = copy.deepcopy(store.get("diagnostics", {}))
    diagnostics["validationSummary"] = self["validationSummary"].getValue()
    payload["diagnostics"] = diagnostics
    if include_resolved:
        payload["points"] = self.pointRecords()
    if scene is not None:
        payload["scene"] = scene
    return payload


def __expandedPointRecordsForFrame(self, store, frame, selected_point_ids=None):
    active_stroke_ids = set(
        self._PaintedPoints__activeStrokeIdsForFrame(store, int(frame))
    )
    selected_ids = (
        None
        if selected_point_ids is None
        else {int(point_id) for point_id in selected_point_ids}
    )
    records = []
    for point in store.get("points", []):
        point_id = int(point.get("pointId", 0))
        if selected_ids is not None and point_id not in selected_ids:
            continue
        if int(point.get("strokeId", 0)) not in active_stroke_ids:
            continue
        records.append(self._PaintedPoints__expandedPointRecord(store, point))
    return records


def __frameRecordPayload(self, frames, record_getter):
    return [
        {
            "frame": int(frame),
            "records": [dict(record) for record in record_getter(int(frame))],
        }
        for frame in frames
    ]


def __createStaticBakeNode(self, suffix, output_location):
    parent = self.parent()
    if parent is None:
        raise RuntimeError(
            "PaintedPoints must have a parent to create a StaticPoints bake node"
        )

    node_name = f"{self.getName()}{suffix}"
    index = 1
    while node_name in parent:
        index += 1
        node_name = f"{self.getName()}{suffix}{index}"

    static_points = StaticPoints(node_name)
    parent.addChild(static_points)
    static_points["outputLocation"].setValue(output_location)
    return static_points


def __attachedPointsForOutput(self):
    attached_points = _find_attached_points_for_painted_node(self)
    if attached_points is None:
        raise RuntimeError(
            "Unable to freeze evaluated points: no AttachedPoints node is connected to this PaintedPoints output"
        )
    return attached_points


def __evaluatedPointsPrimitiveForFrame(self, frame):
    attached_points = self._PaintedPoints__attachedPointsForOutput()
    script_node = _script_node(self)
    context = (
        Gaffer.Context(script_node.context())
        if script_node is not None
        else Gaffer.Context()
    )
    context.setFrame(float(frame))
    with context:
        output_location = (
            attached_points["outputLocation"].getValue().strip() or "/scatter"
        )
        if not attached_points["out"].exists(output_location):
            raise RuntimeError(
                f"Unable to export evaluated points: AttachedPoints produced no object at {output_location}"
            )
        primitive = attached_points["out"].object(output_location)
    if not isinstance(primitive, IECoreScene.PointsPrimitive):
        raise RuntimeError(
            f"Unable to freeze evaluated points: AttachedPoints output at {output_location} is not a PointsPrimitive"
        )
    return attached_points, primitive


def exportAuthoredCache(self):
    store, error = self._PaintedPoints__loadStore()
    if error:
        raise RuntimeError(error)
    export_path = self._PaintedPoints__authoredExportPath()
    _write_bytes_file(export_path, _pack_store_blob(store))
    return export_path


def exportEvaluatedPoints(self):
    attached_points = self._PaintedPoints__attachedPointsForOutput()
    output_location = attached_points["outputLocation"].getValue().strip() or "/scatter"
    primitive = attached_points["out"].object(output_location)
    if primitive is None:
        raise RuntimeError(
            "Unable to export evaluated points: AttachedPoints produced no object at "
            + output_location
        )
    if not isinstance(primitive, IECoreScene.PointsPrimitive):
        raise RuntimeError(
            "Unable to export evaluated points: AttachedPoints output at "
            + output_location
            + " is not a PointsPrimitive"
        )
    export_path = _geometry_export_path(self, "_evaluated.cob")
    writer = IECore.Writer.create(primitive.copy(), export_path)
    if writer is None:
        raise RuntimeError(
            f"Unable to create evaluated points writer for: {export_path}"
        )
    writer.write()
    return export_path


def exportGafferScene(self):
    attached_points = self._PaintedPoints__attachedPointsForOutput()
    output_location = attached_points["outputLocation"].getValue().strip() or "/scatter"
    return _write_scene_export(
        attached_points["out"], output_location, _gaffer_scene_export_path(self)
    )


def exportUSD(self):
    attached_points = self._PaintedPoints__attachedPointsForOutput()
    output_location = attached_points["outputLocation"].getValue().strip() or "/scatter"
    return _write_scene_export(
        attached_points["out"], output_location, _usd_export_path(self)
    )


def exportAlembic(self):
    attached_points = self._PaintedPoints__attachedPointsForOutput()
    output_location = attached_points["outputLocation"].getValue().strip() or "/scatter"
    return _write_scene_export(
        attached_points["out"], output_location, _alembic_export_path(self)
    )


def exportInterchange(self):
    payload = self._PaintedPoints__exportPayload(include_resolved=False)
    export_path = self._PaintedPoints__interchangeExportPath()
    _write_bytes_file(export_path, _pack_store_blob(payload))
    return export_path


def exportDiagnostics(self):
    diagnostics = [
        f"validationSummary: {self['validationSummary'].getValue()}",
        "validationCategories: " + ", ".join(self["validationCategories"].getValue()),
        f"invalidPointCount: {self['invalidPointCount'].getValue()}",
        f"invalidStrokeCount: {self['invalidStrokeCount'].getValue()}",
        f"failingFrame: {self['failingFrame'].getValue()}",
        f"topologyMismatchCount: {self['topologyMismatchCount'].getValue()}",
        f"failingTargetPaths: {', '.join(self['failingTargetPaths'].getValue())}",
        f"lastErrorMessage: {self['lastErrorMessage'].getValue()}",
        f"cacheResolvedPath: {self['cacheResolvedPath'].getValue()}",
        f"cacheVersion: {self['cacheVersion'].getValue()}",
        f"cacheLockedBy: {self['cacheLockedBy'].getValue()}",
        f"cacheLockedHost: {self['cacheLockedHost'].getValue()}",
        f"cacheLockedTime: {self['cacheLockedTime'].getValue()}",
        f"cacheLockedScript: {self['cacheLockedScript'].getValue()}",
    ]
    export_path = self._PaintedPoints__diagnosticsExportPath()
    _write_bytes_file(export_path, "\n".join(diagnostics).encode("utf-8"))
    return export_path


def freezeBakeSelection(self, startFrame=None, endFrame=None):
    store, error = self._PaintedPoints__loadStore()
    if error:
        raise RuntimeError(error)

    frames = _bake_frames(startFrame, endFrame, self)
    selected_point_ids = self._PaintedPoints__selectedPointIds(store)
    if not selected_point_ids:
        raise RuntimeError("Unable to bake selection: no points are currently selected")

    static_points = self._PaintedPoints__createStaticBakeNode(
        "SelectionStaticBake", "/scatterBakeSelection"
    )
    if frames:
        static_points.setFramePointRecords(
            self._PaintedPoints__frameRecordPayload(
                frames,
                lambda frame: self._PaintedPoints__expandedPointRecordsForFrame(
                    store, frame, selected_point_ids
                ),
            )
        )
    else:
        static_points.setPointRecords(
            self._PaintedPoints__expandedPointRecordsForFrame(
                store, _current_frame(self), selected_point_ids
            )
        )
    return static_points.getName()


def freezeBakeToStaticNode(self, startFrame=None, endFrame=None):
    store, error = self._PaintedPoints__loadStore()
    if error:
        raise RuntimeError(error)

    frames = _bake_frames(startFrame, endFrame, self)
    static_points = self._PaintedPoints__createStaticBakeNode(
        "StaticBake", "/scatterBake"
    )
    if frames:
        static_points.setFramePointRecords(
            self._PaintedPoints__frameRecordPayload(
                frames,
                lambda frame: self._PaintedPoints__expandedPointRecordsForFrame(
                    store, frame
                ),
            )
        )
    else:
        static_points.setPointRecords(
            self._PaintedPoints__expandedPointRecordsForFrame(
                store, _current_frame(self)
            )
        )
    return static_points.getName()


def freezeBakeEvaluatedToStaticNode(self, startFrame=None, endFrame=None):
    frames = _bake_frames(startFrame, endFrame, self)
    static_points = self._PaintedPoints__createStaticBakeNode(
        "EvaluatedStaticBake", "/scatterBake"
    )

    if frames:
        frame_payload = []
        attached_points = None
        for frame in frames:
            attached_points, primitive = (
                self._PaintedPoints__evaluatedPointsPrimitiveForFrame(frame)
            )
            frame_payload.append(
                {
                    "frame": int(frame),
                    "pointsPrimitive": primitive.copy(),
                    "pointCount": IECore.IntData(int(primitive.numPoints)),
                }
            )
        static_points["pointType"].setValue(attached_points["pointType"].getValue())
        static_points.setFramePointData(frame_payload)
    else:
        attached_points, primitive = (
            self._PaintedPoints__evaluatedPointsPrimitiveForFrame(_current_frame(self))
        )
        static_points["pointType"].setValue(attached_points["pointType"].getValue())
        static_points["pointData"].setValue(
            IECore.CompoundObject(
                {
                    "pointCount": IECore.IntData(int(primitive.numPoints)),
                    "pointsPrimitive": primitive.copy(),
                }
            )
        )
    return static_points.getName()
