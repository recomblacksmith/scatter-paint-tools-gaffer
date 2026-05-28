import getpass
import math
import os
import pathlib
import socket
import struct
import sys
from datetime import datetime

import Gaffer
import GafferScene
import IECore
import IECoreScene
import imath

SCHEMA_VERSION = 2
SCHEMA_DESCRIPTION = "Gaffer Scatter Paint cache schema v2"
SCHEMA_SUMMARY = (
    "Storage: embedded/external, records: layers/strokes/chunks/points/selections, "
    "locking: session-aware, upgrade: explicit and versioned"
)

CACHE_MODE_EMBEDDED = 0
CACHE_MODE_EXTERNAL = 1

CACHE_PATH_MODE_ABSOLUTE = 0
CACHE_PATH_MODE_RELATIVE_TO_SCRIPT = 1
CACHE_PATH_MODE_RELATIVE_TO_PROJECT = 2

LOCK_MODE_SESSION_AWARE = 0

SURFACE_MODE_VIEWPORT_MESH = 0
SURFACE_MODE_PRE_SUBDIV_CAGE = 1

PAINT_THROUGH_MODE_FRONT_MOST = 0
PAINT_THROUGH_MODE_FILTERED_BRUSH_VOLUME = 1

MODE_PRECEDENCE_STROKE_WINS = 0
MODE_PRECEDENCE_LAYER_WINS = 1

RELAX_OBJECTIVE_PRESERVE_SILHOUETTE = 0
RELAX_OBJECTIVE_EVEN_REDISTRIBUTION = 1

PRESSURE_MAPPING_DIRECT = 0
PRESSURE_MAPPING_CURVE = 1

BACKUP_POLICY_OFF = 0
BACKUP_POLICY_ON = 1

COMPACTION_MODE_IMMEDIATE_BLOCKING = 0

CACHE_MAGIC = b"GSPAINT\0"
ENDIAN_MARKER = 0x01020304
PLUGIN_VERSION_MAJOR = 0
PLUGIN_VERSION_MINOR = 5
PLUGIN_VERSION_PATCH = 8
CONTENT_FLAG_DIAGNOSTICS = 1 << 0
CONTENT_FLAG_UPGRADES = 1 << 1
CONTENT_FLAG_EXTERNAL = 1 << 2
LOCK_MAGIC = b"GSPLOCK\0"
BACKUP_SUFFIX = ".bak"
CHUNK_POINT_LIMIT = 8192
LOCK_STALE_SECONDS = 6 * 60 * 60
VALIDATION_CATEGORY_NAMES = {
    0: "cache",
    1: "lock",
    2: "topology",
    3: "attachment",
    4: "exportReadiness",
    5: "upgradeState",
    6: "diagnostics",
}

EXPORT_PRESET_MINIMAL = 0
EXPORT_PRESET_FULL = 1
EXPORT_PRESET_CUSTOM = 2

DEFAULT_AUTHORED_COLOR = imath.Color3f(0.0, 0.0, 120.0 / 255.0)
DEBUG_RESOLVED_COLOR = imath.Color3f(0.15, 0.9, 0.25)
DEBUG_UNRESOLVED_COLOR = imath.Color3f(1.0, 0.45, 0.0)


def _out_int(default_value=0):
    return Gaffer.IntPlug(
        defaultValue=default_value,
        flags=Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Serialisable,
    )


def _out_string(default_value=""):
    return Gaffer.StringPlug(
        defaultValue=default_value,
        flags=Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Serialisable,
    )


class _AttachedPointsOutputGate(Gaffer.ComputeNode):
    def __init__(self, name="AttachedPointsOutputGate"):
        Gaffer.ComputeNode.__init__(self, name)

        self.addChild(Gaffer.ObjectPlug("inObject", defaultValue=IECore.NullObject()))
        self.addChild(Gaffer.StringPlug("solveStatus", defaultValue=""))
        self.addChild(Gaffer.BoolPlug("strictUnresolved", defaultValue=True))
        self.addChild(Gaffer.IntPlug("unresolvedPointCount", defaultValue=0))
        self.addChild(
            Gaffer.ObjectPlug(
                "out",
                direction=Gaffer.Plug.Direction.Out,
                defaultValue=IECore.NullObject(),
            )
        )

    def affects(self, input_plug):
        outputs = Gaffer.ComputeNode.affects(self, input_plug)
        if input_plug.isSame(self["inObject"]):
            outputs.append(self["out"])
        elif input_plug.isSame(self["solveStatus"]):
            outputs.append(self["out"])
        elif input_plug.isSame(self["strictUnresolved"]):
            outputs.append(self["out"])
        elif input_plug.isSame(self["unresolvedPointCount"]):
            outputs.append(self["out"])
        return outputs

    def hash(self, output, context, h):
        assert output.isSame(self["out"])
        self["inObject"].hash(h)
        self["solveStatus"].hash(h)
        self["strictUnresolved"].hash(h)
        self["unresolvedPointCount"].hash(h)

    def compute(self, plug, context):
        assert plug.isSame(self["out"])
        if (
            self["strictUnresolved"].getValue()
            and self["unresolvedPointCount"].getValue() > 0
        ):
            raise RuntimeError(self["solveStatus"].getValue())
        plug.setValue(self["inObject"].getValue())


IECore.registerRunTimeTyped(
    _AttachedPointsOutputGate, typeName="GafferScatterPaint::_AttachedPointsOutputGate"
)


def _should_register_public_fallback_types():
    package = sys.modules.get(__package__)
    if package is None:
        return True
    return getattr(package, "_GafferScatterPaint", None) is None


def _out_string_vector(default_value=None):
    if default_value is None:
        default_value = IECore.StringVectorData()
    return Gaffer.StringVectorDataPlug(
        defaultValue=default_value,
        flags=Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Serialisable,
    )


def _out_object(default_value=None):
    if default_value is None:
        default_value = IECore.UCharVectorData()
    return Gaffer.ObjectPlug(
        defaultValue=default_value,
        flags=Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Serialisable,
    )


def _color3f_from_values(values, default=None):
    if default is None:
        default = DEFAULT_AUTHORED_COLOR
    if isinstance(values, imath.Color3f):
        return imath.Color3f(float(values[0]), float(values[1]), float(values[2]))
    values = list(values or [default[0], default[1], default[2]])
    if len(values) != 3:
        values = [default[0], default[1], default[2]]
    return imath.Color3f(float(values[0]), float(values[1]), float(values[2]))


def _color_list(values, default=None):
    color = _color3f_from_values(values, default)
    return [float(color[0]), float(color[1]), float(color[2])]


def _color_equal(a, b, tolerance=1e-6):
    color_a = _color3f_from_values(a)
    color_b = _color3f_from_values(b)
    return (
        abs(float(color_a[0]) - float(color_b[0])) <= tolerance
        and abs(float(color_a[1]) - float(color_b[1])) <= tolerance
        and abs(float(color_a[2]) - float(color_b[2])) <= tolerance
    )


def _override_color(record):
    if not isinstance(record, dict) or not bool(record.get("colorEnabled", False)):
        return None
    return _color3f_from_values(record.get("color", DEFAULT_AUTHORED_COLOR))


def _node_default_color(node_metadata):
    if not isinstance(node_metadata, dict):
        return imath.Color3f(DEFAULT_AUTHORED_COLOR)
    return _color3f_from_values(
        node_metadata.get("defaultColor", DEFAULT_AUTHORED_COLOR)
    )


def _resolve_authored_color(node_metadata=None, layer=None, stroke=None, point=None):
    for record in (point, stroke, layer):
        color = _override_color(record)
        if color is not None:
            return color
    return _node_default_color(node_metadata)


def _debug_color_for_point(point):
    return (
        imath.Color3f(DEBUG_RESOLVED_COLOR)
        if point.get("attachmentResolved", False)
        else imath.Color3f(DEBUG_UNRESOLVED_COLOR)
    )


def _primitive_with_display_color(primitive, debug_color=False):
    if not isinstance(primitive, IECoreScene.PointsPrimitive):
        return primitive
    primitive = primitive.copy()
    scatter = primitive.get("scatterColor")
    if scatter is None:
        return primitive
    display_data = scatter.data if not debug_color else None
    if debug_color:
        attachment = primitive.get("attachmentResolved")
        attachment_values = []
        if attachment is not None:
            attachment_values = list(attachment.data)
        point_count = int(primitive.numPoints)
        display_data = IECore.Color3fVectorData(
            [
                imath.Color3f(DEBUG_RESOLVED_COLOR)
                if index < len(attachment_values) and int(attachment_values[index])
                else imath.Color3f(DEBUG_UNRESOLVED_COLOR)
                for index in range(point_count)
            ]
        )
    primitive["Cs"] = IECoreScene.PrimitiveVariable(
        IECoreScene.PrimitiveVariable.Interpolation.Vertex,
        display_data,
    )
    return primitive


def _utc_timestamp():
    return datetime.utcnow().replace(microsecond=0).isoformat() + "Z"


def _parse_utc_timestamp(timestamp):
    if not timestamp:
        return None
    try:
        return datetime.fromisoformat(str(timestamp).replace("Z", "+00:00"))
    except Exception:
        return None


def _unix_time_micros():
    return int(datetime.utcnow().timestamp() * 1000000)


def _current_frame(component=None):
    script_node = _script_node(component) if component is not None else None
    if script_node is not None:
        try:
            return int(round(float(script_node.context().getFrame())))
        except Exception:
            pass
    try:
        context = Gaffer.Context.current()
        return int(round(float(context.getFrame())))
    except Exception:
        return 0


def _script_node(component):
    try:
        return component.ancestor(Gaffer.ScriptNode)
    except Exception:
        return None


def _iter_graph_components(parent):
    if parent is None:
        return

    try:
        children = parent.children()
    except Exception:
        return

    for child in children:
        yield child
        if isinstance(child, Gaffer.GraphComponent):
            for descendant in _iter_graph_components(child):
                yield descendant


def _is_attached_points_node(node):
    try:
        return str(node.typeName()).endswith("AttachedPoints")
    except Exception:
        return node.__class__.__name__ == "AttachedPoints"


def _find_attached_points_for_painted_node(node):
    if node is None:
        return None

    root = _script_node(node)
    if root is None:
        try:
            root = node.parent()
        except Exception:
            root = None
    if root is None:
        return None

    out_plug = node["out"]
    for child in _iter_graph_components(root):
        if _is_attached_points_node(child):
            try:
                points_source = child["points"].source()
            except Exception:
                points_source = None
            if points_source is out_plug:
                return child

    return None


def _script_file_path(component):
    script = _script_node(component)
    if script is None or "fileName" not in script:
        return ""
    return script["fileName"].getValue().strip()


def _bake_frames(start_frame=None, end_frame=None, component=None):
    if start_frame is None and end_frame is None:
        return []

    default_frame = _current_frame(component)
    start_value = default_frame if start_frame is None else int(start_frame)
    end_value = start_value if end_frame is None else int(end_frame)
    if start_value > end_value:
        start_value, end_value = end_value, start_value
    return list(range(start_value, end_value + 1))


def _sampled_point_primitive(data, frame):
    if not isinstance(data, IECore.CompoundObject):
        return None

    members = getattr(data, "members", lambda: {})()
    frame_numbers = members.get("frameNumbers")
    frame_samples = members.get("frameSamples")
    if isinstance(frame_numbers, IECore.IntVectorData) and isinstance(
        frame_samples, IECore.CompoundObject
    ):
        frames = list(frame_numbers)
        if frames:
            sample_frame = frames[-1]
            for candidate in frames:
                if frame <= candidate:
                    sample_frame = candidate
                    break
            return frame_samples.members().get(str(sample_frame))

    primitive = members.get("pointsPrimitive")
    if isinstance(primitive, IECoreScene.PointsPrimitive):
        return primitive
    return None


def _geometry_export_path(component, suffix):
    cache_path = component._PaintedPoints__resolvedCachePath()
    if cache_path:
        return _cache_text_path(cache_path, suffix)
    script_path = _script_file_path(component)
    if script_path:
        return os.path.join(
            os.path.dirname(script_path), f"{component.getName()}{suffix}"
        )
    return os.path.join(os.getcwd(), f"{component.getName()}{suffix}")


def _gaffer_scene_export_path(component):
    return _geometry_export_path(component, "_scene.scc")


def _usd_export_path(component):
    return _geometry_export_path(component, "_scene.usda")


def _alembic_export_path(component):
    return _geometry_export_path(component, "_scene.abc")


def _write_scene_export(scene_plug, output_location, export_path):
    scene_path = GafferScene.ScenePlug.stringToPath(output_location)
    if not scene_plug.exists(scene_path):
        raise RuntimeError(
            "Unable to export evaluated scene: AttachedPoints output location does not exist at "
            + output_location
        )

    primitive = scene_plug.object(scene_path)
    if not isinstance(primitive, IECoreScene.PointsPrimitive):
        raise RuntimeError(
            "Unable to export evaluated scene: AttachedPoints output at "
            + output_location
            + " is not a PointsPrimitive"
        )

    export_path = pathlib.Path(export_path)
    export_path.parent.mkdir(parents=True, exist_ok=True)

    suffix = export_path.suffix.lower()
    if suffix in {".usd", ".usda", ".usdc", ".usdz"}:
        __import__("IECoreUSD")
        __import__("GafferUSD")
    elif suffix == ".abc":
        __import__("IECoreAlembic")

    point_type = "gl:point"
    if "type" in primitive:
        try:
            point_type = primitive["type"].data.value
        except Exception:
            point_type = "gl:point"
    export_primitive = primitive.copy()
    export_primitive["type"] = IECoreScene.PrimitiveVariable(
        IECoreScene.PrimitiveVariable.Interpolation.Constant,
        IECore.StringData(point_type),
    )

    path_components = [str(component) for component in scene_path]
    if not path_components:
        raise RuntimeError(
            "Unable to export evaluated scene: output location must not be the scene root"
        )

    object_to_scene = GafferScene.ObjectToScene("ScatterPaintSceneObject")
    object_to_scene["name"].setValue(path_components[-1])
    object_to_scene["object"].setValue(export_primitive)

    export_scene = object_to_scene["out"]
    for component in reversed(path_components[:-1]):
        group = GafferScene.Group(f"ScatterPaintSceneGroup_{component}")
        group["name"].setValue(component)
        group["in"][0].setInput(export_scene)
        export_scene = group["out"]

    writer = GafferScene.SceneWriter("ScatterPaintSceneWriter")
    writer["in"].setInput(export_scene)
    writer["fileName"].setValue(str(export_path))
    writer["task"].execute()
    return str(export_path)


def _session_metadata(component):
    script_path = _script_file_path(component)
    return {
        "mode": LOCK_MODE_SESSION_AWARE,
        "user": getpass.getuser(),
        "host": socket.gethostname(),
        "timestampUtc": _utc_timestamp(),
        "scriptPath": script_path,
        "projectPath": os.path.dirname(script_path) if script_path else "",
        "sessionId": f"{os.getpid()}:{id(_script_node(component))}",
    }


def _write_u32(buffer, value):
    buffer.extend(struct.pack("<I", int(value)))


def _write_u64(buffer, value):
    buffer.extend(struct.pack("<Q", int(value)))


def _write_i32(buffer, value):
    buffer.extend(struct.pack("<i", int(value)))


def _write_f32(buffer, value):
    buffer.extend(struct.pack("<f", float(value)))


def _write_bool(buffer, value):
    buffer.extend(struct.pack("<?", bool(value)))


def _write_string(buffer, value):
    encoded = str(value or "").encode("utf-8")
    _write_u32(buffer, len(encoded))
    buffer.extend(encoded)


def _write_u64_list(buffer, values):
    values = list(values or [])
    _write_u32(buffer, len(values))
    for value in values:
        _write_u64(buffer, value)


def _write_string_list(buffer, values):
    values = list(values or [])
    _write_u32(buffer, len(values))
    for value in values:
        _write_string(buffer, value)


def _write_f32_list(buffer, values, expected_count):
    values = list(values or [])
    if len(values) != expected_count:
        values = [0.0] * expected_count
    for value in values:
        _write_f32(buffer, value)


def _read_exact(blob, offset, size):
    end = offset + size
    if end > len(blob):
        raise ValueError("Unexpected end of scatter paint blob")
    return blob[offset:end], end


def _read_u32(blob, offset):
    chunk, offset = _read_exact(blob, offset, 4)
    return struct.unpack("<I", chunk)[0], offset


def _read_u64(blob, offset):
    chunk, offset = _read_exact(blob, offset, 8)
    return struct.unpack("<Q", chunk)[0], offset


def _read_i32(blob, offset):
    chunk, offset = _read_exact(blob, offset, 4)
    return struct.unpack("<i", chunk)[0], offset


def _read_f32(blob, offset):
    chunk, offset = _read_exact(blob, offset, 4)
    return struct.unpack("<f", chunk)[0], offset


def _read_bool(blob, offset):
    chunk, offset = _read_exact(blob, offset, 1)
    return struct.unpack("<?", chunk)[0], offset


def _read_string(blob, offset):
    size, offset = _read_u32(blob, offset)
    chunk, offset = _read_exact(blob, offset, size)
    return chunk.decode("utf-8"), offset


def _read_u64_list(blob, offset):
    count, offset = _read_u32(blob, offset)
    values = []
    for _index in range(count):
        value, offset = _read_u64(blob, offset)
        values.append(value)
    return values, offset


def _read_string_list(blob, offset):
    count, offset = _read_u32(blob, offset)
    values = []
    for _index in range(count):
        value, offset = _read_string(blob, offset)
        values.append(value)
    return values, offset


def _read_f32_list(blob, offset, count):
    values = []
    for _index in range(count):
        value, offset = _read_f32(blob, offset)
        values.append(value)
    return values, offset


def _blob_object_from_bytes(blob_bytes):
    return IECore.UCharVectorData(list(blob_bytes))


def _blob_bytes_from_object(blob_object):
    if isinstance(blob_object, IECore.UCharVectorData):
        return bytes(int(value) & 0xFF for value in blob_object)
    if blob_object is None:
        return b""
    raise TypeError(f"Unsupported scatter paint blob object: {type(blob_object)}")


def _checksum64(blob_bytes):
    checksum = 1469598103934665603
    for byte in blob_bytes:
        checksum ^= int(byte)
        checksum = (checksum * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return checksum


def _lock_file_path(cache_path):
    return cache_path + ".lock"


def _read_bytes_file(file_path):
    with open(file_path, "rb") as stream:
        return stream.read()


def _write_bytes_file(file_path, blob_bytes):
    parent_dir = os.path.dirname(file_path)
    if parent_dir:
        os.makedirs(parent_dir, exist_ok=True)
    temp_path = file_path + ".tmp"
    with open(temp_path, "wb") as stream:
        stream.write(blob_bytes)
    os.replace(temp_path, file_path)


def _copy_file(source_path, destination_path):
    _write_bytes_file(destination_path, _read_bytes_file(source_path))


def _pack_lock_blob(lock_metadata):
    payload = bytearray()
    _write_u32(payload, int(lock_metadata.get("mode", LOCK_MODE_SESSION_AWARE)))
    _write_string(payload, lock_metadata.get("user", ""))
    _write_string(payload, lock_metadata.get("host", ""))
    _write_string(payload, lock_metadata.get("timestampUtc", ""))
    _write_string(payload, lock_metadata.get("scriptPath", ""))
    _write_string(payload, lock_metadata.get("projectPath", ""))
    _write_string(payload, lock_metadata.get("sessionId", ""))

    buffer = bytearray()
    buffer.extend(LOCK_MAGIC)
    _write_u64(buffer, _checksum64(payload))
    buffer.extend(payload)
    return bytes(buffer)


def _unpack_lock_blob(blob_bytes):
    if not blob_bytes.startswith(LOCK_MAGIC):
        raise ValueError("Invalid scatter paint lock: bad magic")
    offset = len(LOCK_MAGIC)
    checksum, offset = _read_u64(blob_bytes, offset)
    payload = blob_bytes[offset:]
    if checksum != _checksum64(payload):
        raise ValueError("Invalid scatter paint lock: checksum mismatch")

    offset = 0
    metadata = {}
    metadata["mode"], offset = _read_u32(payload, offset)
    metadata["user"], offset = _read_string(payload, offset)
    metadata["host"], offset = _read_string(payload, offset)
    metadata["timestampUtc"], offset = _read_string(payload, offset)
    metadata["scriptPath"], offset = _read_string(payload, offset)
    metadata["projectPath"], offset = _read_string(payload, offset)
    metadata["sessionId"], offset = _read_string(payload, offset)
    return metadata


def _is_stale_lock(lock_metadata):
    timestamp = _parse_utc_timestamp(lock_metadata.get("timestampUtc", ""))
    if timestamp is None:
        return True
    return (
        abs((datetime.now(timestamp.tzinfo) - timestamp).total_seconds())
        > LOCK_STALE_SECONDS
    )


def _validation_category_strings(category_ids):
    return IECore.StringVectorData(
        [
            (
                category_id
                if isinstance(category_id, str)
                else VALIDATION_CATEGORY_NAMES.get(
                    int(category_id), f"unknown:{category_id}"
                )
            )
            for category_id in category_ids
        ]
    )


def _cache_text_path(base_path, suffix):
    root, ext = os.path.splitext(base_path)
    if ext:
        return root + suffix
    return base_path + suffix


def _content_flags(store):
    flags = 0
    diagnostics = dict(store.get("diagnostics", {}))
    if (
        int(diagnostics.get("invalidPointCount", 0)) > 0
        or int(diagnostics.get("invalidStrokeCount", 0)) > 0
        or int(diagnostics.get("topologyMismatchCount", 0)) > 0
    ):
        flags |= CONTENT_FLAG_DIAGNOSTICS
    if store.get("upgrades"):
        flags |= CONTENT_FLAG_UPGRADES
    if (
        int(store.get("node", {}).get("storageMode", CACHE_MODE_EMBEDDED))
        == CACHE_MODE_EXTERNAL
    ):
        flags |= CONTENT_FLAG_EXTERNAL
    return flags


def _pack_store_blob(store):
    payload = bytearray()
    _write_u32(payload, store.get("schemaVersion", SCHEMA_VERSION))

    next_ids = dict(store.get("nextIds", {}))
    for key in ("layer", "stroke", "point", "selectionSet", "chunk"):
        _write_u64(payload, next_ids.get(key, 1))

    node = dict(store.get("node", {}))
    _write_u32(payload, node.get("storageMode", CACHE_MODE_EMBEDDED))
    _write_u32(payload, node.get("pathMode", CACHE_PATH_MODE_RELATIVE_TO_SCRIPT))
    _write_string(payload, node.get("projectRoot", ""))
    _write_string(payload, node.get("cachePath", ""))
    _write_string(payload, node.get("exportPreset", ""))
    _write_bool(payload, node.get("backupEnabled", False))
    _write_bool(payload, node.get("diagnosticsSnapshotEnabled", True))
    _write_f32_list(payload, node.get("defaultColor", [1.0, 1.0, 1.0]), 3)

    lock = dict(store.get("lock", {}))
    _write_u32(payload, lock.get("mode", LOCK_MODE_SESSION_AWARE))
    _write_string(payload, lock.get("user", ""))
    _write_string(payload, lock.get("host", ""))
    _write_string(payload, lock.get("timestampUtc", ""))
    _write_string(payload, lock.get("scriptPath", ""))
    _write_string(payload, lock.get("projectPath", ""))
    _write_string(payload, lock.get("sessionId", ""))

    _write_string_list(payload, store.get("scenePaths", []))
    _write_string_list(payload, store.get("instanceSourcePaths", []))

    layers = list(store.get("layers", []))
    _write_u32(payload, len(layers))
    for layer in layers:
        _write_u64(payload, layer.get("layerId", 0))
        _write_string(payload, layer.get("name", ""))
        _write_i32(payload, layer.get("order", 0))
        _write_bool(payload, layer.get("enabled", True))
        _write_bool(payload, layer.get("visible", True))
        _write_bool(payload, layer.get("mute", False))
        _write_bool(payload, layer.get("solo", False))
        _write_bool(payload, layer.get("timeVarying", False))
        _write_u32(payload, layer.get("mode", 0))
        _write_i32(payload, layer.get("frameStart", 0))
        _write_i32(payload, layer.get("frameEnd", 0))
        _write_bool(payload, layer.get("holdOutsideRange", True))
        _write_u64(payload, layer.get("firstStrokeId", 0))
        _write_u64(payload, layer.get("lastStrokeId", 0))
        _write_bool(payload, layer.get("colorEnabled", False))
        _write_f32_list(payload, layer.get("color", [1.0, 1.0, 1.0]), 3)

    strokes = list(store.get("strokes", []))
    _write_u32(payload, len(strokes))
    for stroke in strokes:
        _write_u64(payload, stroke.get("strokeId", 0))
        _write_u64(payload, stroke.get("layerId", 0))
        _write_string(payload, stroke.get("name", ""))
        _write_i32(payload, stroke.get("order", 0))
        _write_u32(payload, stroke.get("mode", 0))
        _write_i32(payload, stroke.get("frameStart", 0))
        _write_i32(payload, stroke.get("frameEnd", 0))
        _write_u64(payload, stroke.get("createdTimeUnixMicros", 0))
        _write_u64(payload, stroke.get("firstChunkId", 0))
        _write_u64(payload, stroke.get("lastChunkId", 0))
        _write_u32(payload, stroke.get("pointCount", 0))
        _write_u32(payload, stroke.get("targetCount", 0))
        _write_u64(payload, stroke.get("selectionMaskId", 0))
        _write_bool(payload, stroke.get("colorEnabled", False))
        _write_f32_list(payload, stroke.get("color", [1.0, 1.0, 1.0]), 3)

    chunks = list(store.get("chunks", []))
    _write_u32(payload, len(chunks))
    for chunk in chunks:
        _write_u64(payload, chunk.get("chunkId", 0))
        _write_u64(payload, chunk.get("strokeId", 0))
        _write_u32(payload, chunk.get("chunkIndex", 0))
        _write_u32(payload, chunk.get("pointStart", 0))
        _write_u32(payload, chunk.get("pointCount", 0))
        _write_u32(payload, chunk.get("generation", 0))
        _write_bool(payload, chunk.get("deleted", False))

    points = list(store.get("points", []))
    _write_u32(payload, len(points))
    for point in points:
        _write_u64(payload, point.get("pointId", 0))
        _write_u64(payload, point.get("strokeId", 0))
        _write_u64(payload, point.get("layerId", 0))
        _write_u32(payload, point.get("targetPathId", 0))
        _write_u32(payload, point.get("instanceId", 0))
        _write_u32(payload, point.get("instanceSourcePathId", 0))
        _write_u32(payload, point.get("triangleIndex", 0))
        _write_f32_list(payload, point.get("barycentric", [0.0, 0.0, 0.0]), 3)
        _write_f32_list(payload, point.get("restObjectP", [0.0, 0.0, 0.0]), 3)
        _write_f32_list(payload, point.get("restWorldP", [0.0, 0.0, 0.0]), 3)
        _write_f32_list(payload, point.get("restUV", [0.0, 0.0]), 2)
        _write_f32_list(payload, point.get("restNormal", [0.0, 1.0, 0.0]), 3)
        _write_f32_list(payload, point.get("restUp", [0.0, 0.0, 1.0]), 3)
        _write_f32(payload, point.get("width", 1.0))
        _write_f32(payload, point.get("uniformScale", 1.0))
        _write_u32(payload, point.get("seed", 0))
        _write_f32(payload, point.get("normalSpin", 0.0))
        _write_f32_list(payload, point.get("tangentRotation", [0.0, 0.0]), 2)
        _write_f32(payload, point.get("pressureDensity", 1.0))
        _write_f32(payload, point.get("pressureSoftness", 1.0))
        _write_bool(payload, point.get("valid", True))
        _write_i32(payload, point.get("lastValidFrame", 0))
        _write_u32(payload, point.get("anchorModeUsed", 0))
        _write_u32(payload, point.get("topologyGeneration", 0))
        _write_bool(payload, point.get("colorEnabled", False))
        _write_f32_list(payload, point.get("color", [1.0, 1.0, 1.0]), 3)

    selection_sets = list(store.get("selectionSets", []))
    _write_u32(payload, len(selection_sets))
    for selection_set in selection_sets:
        _write_u64(payload, selection_set.get("selectionSetId", 0))
        _write_string(payload, selection_set.get("name", ""))
        _write_u64_list(payload, selection_set.get("pointIds", []))
        _write_u64_list(payload, selection_set.get("strokeIds", []))

    current_selection = dict(store.get("currentSelection", {}))
    _write_u64_list(payload, current_selection.get("pointIds", []))
    _write_u64_list(payload, current_selection.get("strokeIds", []))

    diagnostics = dict(store.get("diagnostics", {}))
    _write_u64(payload, diagnostics.get("invalidPointCount", 0))
    _write_u64(payload, diagnostics.get("invalidStrokeCount", 0))
    _write_u64(payload, diagnostics.get("topologyMismatchCount", 0))
    _write_i32(payload, diagnostics.get("failingFrame", 0))
    _write_string_list(payload, diagnostics.get("failingTargetPaths", []))
    _write_string(payload, diagnostics.get("lastErrorMessage", ""))
    _write_string(payload, diagnostics.get("validationSummary", ""))
    categories = list(diagnostics.get("categories", []))
    _write_u32(payload, len(categories))
    for category in categories:
        _write_u32(payload, category)

    upgrades = list(store.get("upgrades", []))
    _write_u32(payload, len(upgrades))
    for upgrade in upgrades:
        _write_u32(payload, upgrade.get("sourceSchemaVersion", 0))
        _write_u32(payload, upgrade.get("upgradedSchemaVersion", SCHEMA_VERSION))
        _write_string(payload, upgrade.get("timestampUtc", ""))
        _write_string(payload, upgrade.get("report", ""))

    point_backups = dict(store.get("pointBackups", {}))
    backup_ids = []
    for key in point_backups.keys():
        try:
            point_id = int(key)
        except Exception:
            continue
        backup_ids.append(point_id)
    backup_ids.sort()
    _write_u32(payload, len(backup_ids))
    for point_id in backup_ids:
        backup = dict(
            point_backups.get(point_id, point_backups.get(str(point_id), {})) or {}
        )
        _write_u64(payload, point_id)
        _write_u32(payload, backup.get("targetPathId", 0))
        _write_u32(payload, backup.get("triangleIndex", 0))
        _write_f32_list(payload, backup.get("barycentric", [1.0, 0.0, 0.0]), 3)
        _write_bool(payload, backup.get("attachmentResolved", False))

    buffer = bytearray()
    buffer.extend(CACHE_MAGIC)
    _write_u32(buffer, ENDIAN_MARKER)
    _write_u32(buffer, PLUGIN_VERSION_MAJOR)
    _write_u32(buffer, PLUGIN_VERSION_MINOR)
    _write_u32(buffer, PLUGIN_VERSION_PATCH)
    _write_u32(buffer, _content_flags(store))
    _write_u64(buffer, _checksum64(payload))
    buffer.extend(payload)
    return bytes(buffer)


def _unpack_store_blob(blob_bytes):
    if not blob_bytes:
        return None, ""
    if not blob_bytes.startswith(CACHE_MAGIC):
        return None, "Invalid scatter paint blob: bad magic"

    offset = len(CACHE_MAGIC)
    try:
        endian_marker, offset = _read_u32(blob_bytes, offset)
        plugin_version_major, offset = _read_u32(blob_bytes, offset)
        plugin_version_minor, offset = _read_u32(blob_bytes, offset)
        plugin_version_patch, offset = _read_u32(blob_bytes, offset)
        content_flags, offset = _read_u32(blob_bytes, offset)
        checksum, offset = _read_u64(blob_bytes, offset)

        payload = blob_bytes[offset:]
        if endian_marker != ENDIAN_MARKER:
            return None, "Invalid scatter paint blob: unsupported endian marker"
        if checksum != _checksum64(payload):
            return None, "Invalid scatter paint blob: checksum mismatch"

        offset = 0
        schema_version, offset = _read_u32(payload, offset)
        if schema_version != SCHEMA_VERSION:
            return (
                None,
                f"Invalid scatter paint blob: unsupported schema version {schema_version}",
            )

        next_ids = {}
        for key in ("layer", "stroke", "point", "selectionSet", "chunk"):
            next_ids[key], offset = _read_u64(payload, offset)

        node = {}
        node["storageMode"], offset = _read_u32(payload, offset)
        node["pathMode"], offset = _read_u32(payload, offset)
        node["projectRoot"], offset = _read_string(payload, offset)
        node["cachePath"], offset = _read_string(payload, offset)
        node["exportPreset"], offset = _read_string(payload, offset)
        node["backupEnabled"], offset = _read_bool(payload, offset)
        node["diagnosticsSnapshotEnabled"], offset = _read_bool(payload, offset)
        node["defaultColor"], offset = _read_f32_list(payload, offset, 3)
        node["contentFlags"] = content_flags
        node["pluginVersionMajor"] = plugin_version_major
        node["pluginVersionMinor"] = plugin_version_minor
        node["pluginVersionPatch"] = plugin_version_patch

        lock = {}
        lock["mode"], offset = _read_u32(payload, offset)
        lock["user"], offset = _read_string(payload, offset)
        lock["host"], offset = _read_string(payload, offset)
        lock["timestampUtc"], offset = _read_string(payload, offset)
        lock["scriptPath"], offset = _read_string(payload, offset)
        lock["projectPath"], offset = _read_string(payload, offset)
        lock["sessionId"], offset = _read_string(payload, offset)

        scene_paths, offset = _read_string_list(payload, offset)
        instance_source_paths, offset = _read_string_list(payload, offset)

        layer_count, offset = _read_u32(payload, offset)
        layers = []
        for _index in range(layer_count):
            layer = {}
            layer["layerId"], offset = _read_u64(payload, offset)
            layer["name"], offset = _read_string(payload, offset)
            layer["order"], offset = _read_i32(payload, offset)
            layer["enabled"], offset = _read_bool(payload, offset)
            layer["visible"], offset = _read_bool(payload, offset)
            layer["mute"], offset = _read_bool(payload, offset)
            layer["solo"], offset = _read_bool(payload, offset)
            layer["timeVarying"], offset = _read_bool(payload, offset)
            layer["mode"], offset = _read_u32(payload, offset)
            layer["frameStart"], offset = _read_i32(payload, offset)
            layer["frameEnd"], offset = _read_i32(payload, offset)
            layer["holdOutsideRange"], offset = _read_bool(payload, offset)
            layer["firstStrokeId"], offset = _read_u64(payload, offset)
            layer["lastStrokeId"], offset = _read_u64(payload, offset)
            layer["colorEnabled"], offset = _read_bool(payload, offset)
            layer["color"], offset = _read_f32_list(payload, offset, 3)
            layers.append(layer)

        stroke_count, offset = _read_u32(payload, offset)
        strokes = []
        for _index in range(stroke_count):
            stroke = {}
            stroke["strokeId"], offset = _read_u64(payload, offset)
            stroke["layerId"], offset = _read_u64(payload, offset)
            stroke["name"], offset = _read_string(payload, offset)
            stroke["order"], offset = _read_i32(payload, offset)
            stroke["mode"], offset = _read_u32(payload, offset)
            stroke["frameStart"], offset = _read_i32(payload, offset)
            stroke["frameEnd"], offset = _read_i32(payload, offset)
            stroke["createdTimeUnixMicros"], offset = _read_u64(payload, offset)
            stroke["firstChunkId"], offset = _read_u64(payload, offset)
            stroke["lastChunkId"], offset = _read_u64(payload, offset)
            stroke["pointCount"], offset = _read_u32(payload, offset)
            stroke["targetCount"], offset = _read_u32(payload, offset)
            stroke["selectionMaskId"], offset = _read_u64(payload, offset)
            stroke["colorEnabled"], offset = _read_bool(payload, offset)
            stroke["color"], offset = _read_f32_list(payload, offset, 3)
            strokes.append(stroke)

        chunk_count, offset = _read_u32(payload, offset)
        chunks = []
        for _index in range(chunk_count):
            chunk = {}
            chunk["chunkId"], offset = _read_u64(payload, offset)
            chunk["strokeId"], offset = _read_u64(payload, offset)
            chunk["chunkIndex"], offset = _read_u32(payload, offset)
            chunk["pointStart"], offset = _read_u32(payload, offset)
            chunk["pointCount"], offset = _read_u32(payload, offset)
            chunk["generation"], offset = _read_u32(payload, offset)
            chunk["deleted"], offset = _read_bool(payload, offset)
            chunks.append(chunk)

        point_count, offset = _read_u32(payload, offset)
        points = []
        for _index in range(point_count):
            point = {}
            point["pointId"], offset = _read_u64(payload, offset)
            point["strokeId"], offset = _read_u64(payload, offset)
            point["layerId"], offset = _read_u64(payload, offset)
            point["targetPathId"], offset = _read_u32(payload, offset)
            point["instanceId"], offset = _read_u32(payload, offset)
            point["instanceSourcePathId"], offset = _read_u32(payload, offset)
            point["triangleIndex"], offset = _read_u32(payload, offset)
            point["barycentric"], offset = _read_f32_list(payload, offset, 3)
            point["restObjectP"], offset = _read_f32_list(payload, offset, 3)
            point["restWorldP"], offset = _read_f32_list(payload, offset, 3)
            point["restUV"], offset = _read_f32_list(payload, offset, 2)
            point["restNormal"], offset = _read_f32_list(payload, offset, 3)
            point["restUp"], offset = _read_f32_list(payload, offset, 3)
            point["width"], offset = _read_f32(payload, offset)
            point["uniformScale"], offset = _read_f32(payload, offset)
            point["seed"], offset = _read_u32(payload, offset)
            point["normalSpin"], offset = _read_f32(payload, offset)
            point["tangentRotation"], offset = _read_f32_list(payload, offset, 2)
            point["pressureDensity"], offset = _read_f32(payload, offset)
            point["pressureSoftness"], offset = _read_f32(payload, offset)
            point["valid"], offset = _read_bool(payload, offset)
            point["lastValidFrame"], offset = _read_i32(payload, offset)
            point["anchorModeUsed"], offset = _read_u32(payload, offset)
            point["topologyGeneration"], offset = _read_u32(payload, offset)
            point["colorEnabled"], offset = _read_bool(payload, offset)
            point["color"], offset = _read_f32_list(payload, offset, 3)
            points.append(point)

        selection_set_count, offset = _read_u32(payload, offset)
        selection_sets = []
        for _index in range(selection_set_count):
            selection_set = {}
            selection_set["selectionSetId"], offset = _read_u64(payload, offset)
            selection_set["name"], offset = _read_string(payload, offset)
            selection_set["pointIds"], offset = _read_u64_list(payload, offset)
            selection_set["strokeIds"], offset = _read_u64_list(payload, offset)
            selection_sets.append(selection_set)

        current_point_ids, offset = _read_u64_list(payload, offset)
        current_stroke_ids, offset = _read_u64_list(payload, offset)

        diagnostics = {}
        diagnostics["invalidPointCount"], offset = _read_u64(payload, offset)
        diagnostics["invalidStrokeCount"], offset = _read_u64(payload, offset)
        diagnostics["topologyMismatchCount"], offset = _read_u64(payload, offset)
        diagnostics["failingFrame"], offset = _read_i32(payload, offset)
        diagnostics["failingTargetPaths"], offset = _read_string_list(payload, offset)
        diagnostics["lastErrorMessage"], offset = _read_string(payload, offset)
        diagnostics["validationSummary"], offset = _read_string(payload, offset)
        category_count, offset = _read_u32(payload, offset)
        diagnostics["categories"] = []
        for _index in range(category_count):
            category, offset = _read_u32(payload, offset)
            diagnostics["categories"].append(category)

        upgrade_count, offset = _read_u32(payload, offset)
        upgrades = []
        for _index in range(upgrade_count):
            upgrade = {}
            upgrade["sourceSchemaVersion"], offset = _read_u32(payload, offset)
            upgrade["upgradedSchemaVersion"], offset = _read_u32(payload, offset)
            upgrade["timestampUtc"], offset = _read_string(payload, offset)
            upgrade["report"], offset = _read_string(payload, offset)
            upgrades.append(upgrade)

        point_backups = {}
        if offset < len(payload):
            backup_count, offset = _read_u32(payload, offset)
            for _index in range(backup_count):
                point_id, offset = _read_u64(payload, offset)
                target_path_id, offset = _read_u32(payload, offset)
                triangle_index, offset = _read_u32(payload, offset)
                barycentric, offset = _read_f32_list(payload, offset, 3)
                attachment_resolved, offset = _read_bool(payload, offset)
                point_backups[point_id] = {
                    "targetPathId": target_path_id,
                    "triangleIndex": triangle_index,
                    "barycentric": barycentric,
                    "attachmentResolved": attachment_resolved,
                }
        else:
            point_backups = {}

    except Exception as exc:
        return None, f"Invalid scatter paint blob: {exc}"

    return {
        "schemaVersion": schema_version,
        "nextIds": next_ids,
        "node": node,
        "lock": lock,
        "scenePaths": scene_paths,
        "instanceSourcePaths": instance_source_paths,
        "layers": layers,
        "strokes": strokes,
        "chunks": chunks,
        "points": points,
        "selectionSets": selection_sets,
        "currentSelection": {
            "pointIds": current_point_ids,
            "strokeIds": current_stroke_ids,
        },
        "diagnostics": diagnostics,
        "upgrades": upgrades,
        "pointBackups": point_backups,
    }, ""


def _triangle_point(a, b, c, barycentric):
    u, v, w = [float(value) for value in barycentric]
    return (a * u) + (b * v) + (c * w)


def _vector_from_values(values, default):
    values = list(values or default)
    if len(values) != 3:
        values = list(default)
    return imath.V3f(float(values[0]), float(values[1]), float(values[2]))


def _cross(a, b):
    return imath.V3f(
        (a.y * b.z) - (a.z * b.y),
        (a.z * b.x) - (a.x * b.z),
        (a.x * b.y) - (a.y * b.x),
    )


def _normalized(vector, default):
    length_squared = vector.dot(vector)
    if length_squared <= 0.0:
        return default
    return vector / math.sqrt(length_squared)


def _orthogonalized(vector, normal, default):
    projected = vector - (normal * vector.dot(normal))
    return _normalized(projected, default)


def _mesh_triangle_data(mesh):
    if not hasattr(mesh, "verticesPerFace") or "P" not in mesh:
        return None

    try:
        positions = mesh["P"].data
        vertices_per_face = mesh.verticesPerFace
        vertex_ids = mesh.vertexIds
    except Exception:
        return None

    triangles = []
    offset = 0
    triangle_index = 0
    for face_vertex_count in vertices_per_face:
        face_vertex_count = int(face_vertex_count)
        if face_vertex_count < 3:
            offset += face_vertex_count
            continue

        face_indices = [int(vertex_ids[offset + i]) for i in range(face_vertex_count)]
        anchor = face_indices[0]
        for face_offset in range(1, face_vertex_count - 1):
            triangles.append(
                (
                    triangle_index,
                    anchor,
                    face_indices[face_offset],
                    face_indices[face_offset + 1],
                )
            )
            triangle_index += 1

        offset += face_vertex_count

    return {
        "positions": positions,
        "triangles": triangles,
    }


def _indexed_value(data, index, default):
    try:
        return data[int(index)]
    except Exception:
        return default


def _triangle_normal(mesh, triangle, barycentric, fallback_normal):
    if "N" not in mesh:
        return fallback_normal

    normal_primvar = mesh["N"]
    interpolation = normal_primvar.interpolation
    normal_data = normal_primvar.data
    normal_indices = getattr(normal_primvar, "indices", None)
    default = fallback_normal

    if interpolation in (
        IECoreScene.PrimitiveVariable.Interpolation.Vertex,
        IECoreScene.PrimitiveVariable.Interpolation.Varying,
    ):
        values = []
        for vertex_id in triangle[1:]:
            normal_index = vertex_id
            if normal_indices is not None:
                normal_index = _indexed_value(normal_indices, vertex_id, vertex_id)
            values.append(
                _vector_from_values(
                    _indexed_value(normal_data, normal_index, default), default
                )
            )
        return _normalized(
            _triangle_point(values[0], values[1], values[2], barycentric),
            fallback_normal,
        )

    if interpolation == IECoreScene.PrimitiveVariable.Interpolation.FaceVarying:
        expanded = normal_primvar.expandedData()
        face_varying_index = triangle[0] * 3
        values = [
            _vector_from_values(
                _indexed_value(expanded, face_varying_index + offset, default), default
            )
            for offset in range(3)
        ]
        return _normalized(
            _triangle_point(values[0], values[1], values[2], barycentric),
            fallback_normal,
        )

    if interpolation == IECoreScene.PrimitiveVariable.Interpolation.Uniform:
        return _normalized(
            _vector_from_values(
                _indexed_value(normal_data, triangle[0], default), default
            ),
            fallback_normal,
        )

    return fallback_normal


def _triangle_up(a, b, c, normal):
    edge_ab = _orthogonalized(b - a, normal, imath.V3f(0, 0, 1))
    if edge_ab.dot(edge_ab) > 0.0:
        return edge_ab

    edge_ac = _orthogonalized(c - a, normal, imath.V3f(0, 0, 1))
    if edge_ac.dot(edge_ac) > 0.0:
        return edge_ac

    fallback_axis = imath.V3f(0, 1, 0)
    if abs(normal.dot(fallback_axis)) > 0.999:
        fallback_axis = imath.V3f(1, 0, 0)
    return _orthogonalized(_cross(fallback_axis, normal), normal, imath.V3f(0, 0, 1))


def _scene_path_to_string(scene_path):
    if isinstance(scene_path, str):
        return scene_path if scene_path.startswith("/") else f"/{scene_path}"
    parts = [str(component) for component in list(scene_path or [])]
    return "/" + "/".join(parts) if parts else "/"


def _walk_scene_paths(scene_plug, scene_path=None):
    scene_path = list(scene_path or [])
    yield list(scene_path)
    try:
        child_names = scene_plug.childNames(scene_path)
    except Exception:
        return
    for child_name in child_names:
        child_path = list(scene_path)
        child_path.append(child_name)
        for descendant in _walk_scene_paths(scene_plug, child_path):
            yield descendant


def _path_filter_paths(filter_string):
    return {
        token.strip() for token in str(filter_string or "").split() if token.strip()
    }


def _set_filter_paths(scene_plug, set_name):
    set_name = str(set_name or "").strip()
    if not set_name:
        return set()
    try:
        path_matcher_data = scene_plug.set(set_name)
    except Exception:
        return set()
    if path_matcher_data is None:
        return set()
    try:
        return {str(path) for path in path_matcher_data.value.paths()}
    except Exception:
        return set()


def _closest_point_on_triangle(point, a, b, c):
    ab = b - a
    ac = c - a
    ap = point - a
    d1 = ab.dot(ap)
    d2 = ac.dot(ap)
    if d1 <= 0.0 and d2 <= 0.0:
        return a, [1.0, 0.0, 0.0]

    bp = point - b
    d3 = ab.dot(bp)
    d4 = ac.dot(bp)
    if d3 >= 0.0 and d4 <= d3:
        return b, [0.0, 1.0, 0.0]

    vc = (d1 * d4) - (d3 * d2)
    if vc <= 0.0 and d1 >= 0.0 and d3 <= 0.0:
        v = d1 / (d1 - d3)
        return a + (ab * v), [1.0 - v, v, 0.0]

    cp = point - c
    d5 = ab.dot(cp)
    d6 = ac.dot(cp)
    if d6 >= 0.0 and d5 <= d6:
        return c, [0.0, 0.0, 1.0]

    vb = (d5 * d2) - (d1 * d6)
    if vb <= 0.0 and d2 >= 0.0 and d6 <= 0.0:
        w = d2 / (d2 - d6)
        return a + (ac * w), [1.0 - w, 0.0, w]

    va = (d3 * d6) - (d5 * d4)
    if va <= 0.0 and (d4 - d3) >= 0.0 and (d5 - d6) >= 0.0:
        w = (d4 - d3) / ((d4 - d3) + (d5 - d6))
        return b + ((c - b) * w), [0.0, 1.0 - w, w]

    denominator = 1.0 / (va + vb + vc)
    v = vb * denominator
    w = vc * denominator
    u = 1.0 - v - w
    return a + (ab * v) + (ac * w), [u, v, w]


def _quat_from_axes(x_axis, y_axis, z_axis):
    m00 = float(x_axis.x)
    m01 = float(y_axis.x)
    m02 = float(z_axis.x)
    m10 = float(x_axis.y)
    m11 = float(y_axis.y)
    m12 = float(z_axis.y)
    m20 = float(x_axis.z)
    m21 = float(y_axis.z)
    m22 = float(z_axis.z)

    trace = m00 + m11 + m22
    if trace > 0.0:
        s = math.sqrt(trace + 1.0) * 2.0
        return imath.Quatf(
            0.25 * s,
            imath.V3f(
                (m21 - m12) / s,
                (m02 - m20) / s,
                (m10 - m01) / s,
            ),
        )

    if m00 > m11 and m00 > m22:
        s = math.sqrt(1.0 + m00 - m11 - m22) * 2.0
        return imath.Quatf(
            (m21 - m12) / s,
            imath.V3f(
                0.25 * s,
                (m01 + m10) / s,
                (m02 + m20) / s,
            ),
        )

    if m11 > m22:
        s = math.sqrt(1.0 + m11 - m00 - m22) * 2.0
        return imath.Quatf(
            (m02 - m20) / s,
            imath.V3f(
                (m01 + m10) / s,
                0.25 * s,
                (m12 + m21) / s,
            ),
        )

    s = math.sqrt(1.0 + m22 - m00 - m11) * 2.0
    return imath.Quatf(
        (m10 - m01) / s,
        imath.V3f(
            (m02 + m20) / s,
            (m12 + m21) / s,
            0.25 * s,
        ),
    )


def _orient_from_normal_up(normal, up):
    z_axis = _normalized(normal, imath.V3f(0, 0, 1))
    y_axis = _orthogonalized(up, z_axis, imath.V3f(0, 1, 0))
    x_axis = _normalized(_cross(y_axis, z_axis), imath.V3f(1, 0, 0))
    y_axis = _normalized(_cross(z_axis, x_axis), imath.V3f(0, 1, 0))
    return _quat_from_axes(x_axis, y_axis, z_axis)


def _parse_attribute_filter(filter_string):
    return {
        token.strip() for token in str(filter_string or "").split(",") if token.strip()
    }


def _include_optional_attribute(export_preset, custom_attributes, attribute_name):
    if export_preset == EXPORT_PRESET_FULL:
        return True
    if export_preset == EXPORT_PRESET_CUSTOM:
        return attribute_name in custom_attributes
    return False


def _points_primitive_from_records(
    point_records,
    point_type="gl:point",
    export_preset=EXPORT_PRESET_FULL,
    include_attributes="",
    debug_color=False,
):
    custom_attributes = _parse_attribute_filter(include_attributes)
    positions = IECore.V3fVectorData(
        [
            imath.V3f(
                float(point.get("P", [0.0, 0.0, 0.0])[0]),
                float(point.get("P", [0.0, 0.0, 0.0])[1]),
                float(point.get("P", [0.0, 0.0, 0.0])[2]),
            )
            for point in point_records
        ],
        IECore.GeometricData.Interpretation.Point,
    )
    primitive = IECoreScene.PointsPrimitive(positions)

    primitive["type"] = IECoreScene.PrimitiveVariable(
        IECoreScene.PrimitiveVariable.Interpolation.Constant,
        IECore.StringData(point_type),
    )
    primitive["width"] = IECoreScene.PrimitiveVariable(
        IECoreScene.PrimitiveVariable.Interpolation.Vertex,
        IECore.FloatVectorData(
            [float(point.get("width", 1.0)) for point in point_records]
        ),
    )
    primitive["scale"] = IECoreScene.PrimitiveVariable(
        IECoreScene.PrimitiveVariable.Interpolation.Vertex,
        IECore.FloatVectorData(
            [float(point.get("scale", 1.0)) for point in point_records]
        ),
    )
    primitive["id"] = IECoreScene.PrimitiveVariable(
        IECoreScene.PrimitiveVariable.Interpolation.Vertex,
        IECore.Int64VectorData(
            [int(point.get("pointId", 0)) for point in point_records]
        ),
    )
    primitive["seed"] = IECoreScene.PrimitiveVariable(
        IECoreScene.PrimitiveVariable.Interpolation.Vertex,
        IECore.IntVectorData([int(point.get("seed", 0)) for point in point_records]),
    )
    primitive["strokeId"] = IECoreScene.PrimitiveVariable(
        IECoreScene.PrimitiveVariable.Interpolation.Vertex,
        IECore.Int64VectorData(
            [int(point.get("strokeId", 0)) for point in point_records]
        ),
    )
    authored_colors = [
        _color3f_from_values(
            point.get("scatterColor", point.get("authoredColor", point.get("color")))
        )
        for point in point_records
    ]
    primitive["scatterColor"] = IECoreScene.PrimitiveVariable(
        IECoreScene.PrimitiveVariable.Interpolation.Vertex,
        IECore.Color3fVectorData(authored_colors),
    )
    primitive["Cs"] = IECoreScene.PrimitiveVariable(
        IECoreScene.PrimitiveVariable.Interpolation.Vertex,
        IECore.Color3fVectorData(
            [
                _debug_color_for_point(point) if debug_color else authored_colors[index]
                for index, point in enumerate(point_records)
            ]
        ),
    )
    primitive["sourcePath"] = IECoreScene.PrimitiveVariable(
        IECoreScene.PrimitiveVariable.Interpolation.Vertex,
        IECore.StringVectorData(
            [str(point.get("sourcePath", "")) for point in point_records]
        ),
    )
    primitive["N"] = IECoreScene.PrimitiveVariable(
        IECoreScene.PrimitiveVariable.Interpolation.Vertex,
        IECore.V3fVectorData(
            [
                _vector_from_values(point.get("N"), [0.0, 1.0, 0.0])
                for point in point_records
            ],
            IECore.GeometricData.Interpretation.Normal,
        ),
    )
    primitive["up"] = IECoreScene.PrimitiveVariable(
        IECoreScene.PrimitiveVariable.Interpolation.Vertex,
        IECore.V3fVectorData(
            [
                _vector_from_values(point.get("up"), [0.0, 0.0, 1.0])
                for point in point_records
            ],
            IECore.GeometricData.Interpretation.Vector,
        ),
    )
    primitive["orient"] = IECoreScene.PrimitiveVariable(
        IECoreScene.PrimitiveVariable.Interpolation.Vertex,
        IECore.QuatfVectorData(
            [
                point.get(
                    "orient",
                    _orient_from_normal_up(
                        _vector_from_values(point.get("N"), [0.0, 1.0, 0.0]),
                        _vector_from_values(point.get("up"), [0.0, 0.0, 1.0]),
                    ),
                )
                for point in point_records
            ]
        ),
    )

    if _include_optional_attribute(export_preset, custom_attributes, "triangleIndex"):
        primitive["triangleIndex"] = IECoreScene.PrimitiveVariable(
            IECoreScene.PrimitiveVariable.Interpolation.Vertex,
            IECore.IntVectorData(
                [int(point.get("triangleIndex", 0)) for point in point_records]
            ),
        )

    if _include_optional_attribute(export_preset, custom_attributes, "barycentric"):
        primitive["barycentric"] = IECoreScene.PrimitiveVariable(
            IECoreScene.PrimitiveVariable.Interpolation.Vertex,
            IECore.V3fVectorData(
                [
                    imath.V3f(
                        *[
                            float(value)
                            for value in point.get("barycentric", [1.0, 0.0, 0.0])
                        ]
                    )
                    for point in point_records
                ],
                IECore.GeometricData.Interpretation.Vector,
            ),
        )

    if _include_optional_attribute(
        export_preset, custom_attributes, "attachmentResolved"
    ):
        primitive["attachmentResolved"] = IECoreScene.PrimitiveVariable(
            IECoreScene.PrimitiveVariable.Interpolation.Vertex,
            IECore.IntVectorData(
                [
                    1 if point.get("attachmentResolved", False) else 0
                    for point in point_records
                ]
            ),
        )

    return primitive
