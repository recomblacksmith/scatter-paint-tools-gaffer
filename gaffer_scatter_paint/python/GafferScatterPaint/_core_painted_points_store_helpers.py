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


def __defaultStore(self):
    return {
        "schemaVersion": SCHEMA_VERSION,
        "nextIds": {
            "layer": 1,
            "stroke": 1,
            "point": 1,
            "selectionSet": 1,
            "chunk": 1,
        },
        "node": self._PaintedPoints__nodeMetadata(),
        "lock": {},
        "scenePaths": [],
        "instanceSourcePaths": [],
        "layers": [],
        "strokes": [],
        "chunks": [],
        "points": [],
        "selectionSets": [],
        "currentSelection": {
            "pointIds": [],
            "strokeIds": [],
        },
        "diagnostics": self._PaintedPoints__emptyDiagnostics(),
        "upgrades": [],
        "pointBackups": {},
    }


def __emptyDiagnostics(self):
    return {
        "invalidPointCount": 0,
        "invalidStrokeCount": 0,
        "topologyMismatchCount": 0,
        "failingFrame": 0,
        "failingTargetPaths": [],
        "lastErrorMessage": "",
        "validationSummary": "Empty scatter paint cache",
        "categories": [0, 1, 2, 3, 4, 5, 6],
        "validationCategories": [0, 1, 2, 3, 4, 5, 6],
    }


def __nodeMetadata(self):
    return {
        "storageMode": int(self["cacheMode"].getValue()),
        "pathMode": int(self["cachePathMode"].getValue()),
        "projectRoot": self["projectRoot"].getValue().strip(),
        "cachePath": self["cachePath"].getValue().strip(),
        "relaxObjective": int(self["relaxObjective"].getValue()),
        "defaultColor": _color_list(self["defaultColor"].getValue()),
        "exportPreset": "",
        "backupEnabled": bool(self["backupEnabled"].getValue()),
        "diagnosticsSnapshotEnabled": True,
    }


def __activeLockMetadata(self):
    return _session_metadata(self)


def __lockFilePath(self):
    cache_path = self._PaintedPoints__resolvedCachePath()
    if not cache_path:
        return ""
    return _lock_file_path(cache_path)


def __readExternalLock(self):
    lock_path = self._PaintedPoints__lockFilePath()
    if not lock_path or not os.path.exists(lock_path):
        return None
    return _unpack_lock_blob(_read_bytes_file(lock_path))


def __sameSessionLock(self, lock_metadata):
    active_lock = self._PaintedPoints__activeLockMetadata()
    return (
        str(lock_metadata.get("sessionId", "")) == str(active_lock.get("sessionId", ""))
        and str(lock_metadata.get("host", "")) == str(active_lock.get("host", ""))
        and str(lock_metadata.get("user", "")) == str(active_lock.get("user", ""))
    )


def __ensureWriteLock(self):
    if self["cacheMode"].getValue() != CACHE_MODE_EXTERNAL:
        return self._PaintedPoints__activeLockMetadata()

    existing_lock = None
    try:
        existing_lock = self._PaintedPoints__readExternalLock()
    except Exception as exc:
        raise RuntimeError(f"Unable to read scatter paint lock: {exc}")

    if existing_lock is not None and not self._PaintedPoints__sameSessionLock(
        existing_lock
    ):
        if not _is_stale_lock(existing_lock):
            raise RuntimeError(
                "External cache is locked by "
                f"{existing_lock.get('user', '')}@{existing_lock.get('host', '')} "
                f"for {existing_lock.get('scriptPath', '')}"
            )

    lock_metadata = self._PaintedPoints__activeLockMetadata()
    _write_bytes_file(
        self._PaintedPoints__lockFilePath(), _pack_lock_blob(lock_metadata)
    )
    return lock_metadata


def __releaseWriteLock(self, expected_lock=None):
    if self["cacheMode"].getValue() != CACHE_MODE_EXTERNAL:
        return
    lock_path = self._PaintedPoints__lockFilePath()
    if not lock_path or not os.path.exists(lock_path):
        return
    if expected_lock is not None:
        try:
            existing_lock = self._PaintedPoints__readExternalLock()
        except Exception:
            existing_lock = None
        if existing_lock is not None and not self._PaintedPoints__sameSessionLock(
            existing_lock
        ):
            return
    os.remove(lock_path)


def __writeBackupIfEnabled(self, cache_path):
    if (
        not self["backupEnabled"].getValue()
        or self["backupPolicy"].getValue() != BACKUP_POLICY_ON
    ):
        return ""
    if not cache_path or not os.path.exists(cache_path):
        return ""
    backup_path = _cache_text_path(cache_path, BACKUP_SUFFIX)
    _copy_file(cache_path, backup_path)
    return backup_path


def __diagnosticsExportPath(self):
    cache_path = self._PaintedPoints__resolvedCachePath()
    if cache_path:
        return _cache_text_path(cache_path, ".diagnostics.txt")
    script_path = _script_file_path(self)
    if script_path:
        return os.path.join(
            os.path.dirname(script_path), f"{self.getName()}_diagnostics.txt"
        )
    return os.path.join(os.getcwd(), f"{self.getName()}_diagnostics.txt")


def __interchangeExportPath(self):
    cache_path = self._PaintedPoints__resolvedCachePath()
    if cache_path:
        return _cache_text_path(cache_path, ".interchange.bin")
    script_path = _script_file_path(self)
    if script_path:
        return os.path.join(
            os.path.dirname(script_path), f"{self.getName()}_interchange.bin"
        )
    return os.path.join(os.getcwd(), f"{self.getName()}_interchange.bin")


def __authoredExportPath(self):
    cache_path = self._PaintedPoints__resolvedCachePath()
    if cache_path:
        return _cache_text_path(cache_path, ".authored.bin")
    script_path = _script_file_path(self)
    if script_path:
        return os.path.join(
            os.path.dirname(script_path), f"{self.getName()}_authored.bin"
        )
    return os.path.join(os.getcwd(), f"{self.getName()}_authored.bin")


def __resolvedCachePath(self):
    if self["cacheMode"].getValue() != CACHE_MODE_EXTERNAL:
        return ""

    cache_path = self["cachePath"].getValue().strip()
    if not cache_path:
        return ""

    if os.path.isabs(cache_path):
        return os.path.normpath(cache_path)

    path_mode = self["cachePathMode"].getValue()
    if path_mode == CACHE_PATH_MODE_RELATIVE_TO_PROJECT:
        project_root = self["projectRoot"].getValue().strip()
        if project_root:
            return os.path.normpath(os.path.join(project_root, cache_path))

    if path_mode == CACHE_PATH_MODE_RELATIVE_TO_SCRIPT:
        script_path = _script_file_path(self)
        if script_path:
            return os.path.normpath(
                os.path.join(os.path.dirname(script_path), cache_path)
            )

    return os.path.normpath(cache_path)


def __ensureCacheStore(self):
    if self["cacheMode"].getValue() == CACHE_MODE_EXTERNAL:
        return
    if _blob_bytes_from_object(self["cacheBlob"].getValue()):
        return
    self._PaintedPoints__writeStore(self._PaintedPoints__defaultStore())


def __loadStore(self):
    if self["cacheMode"].getValue() == CACHE_MODE_EXTERNAL:
        cache_path = self._PaintedPoints__resolvedCachePath()
        if not cache_path:
            return (
                self._PaintedPoints__defaultStore(),
                "External cache mode requires a cache path",
            )
        try:
            with open(cache_path, "rb") as stream:
                blob_bytes = stream.read()
        except FileNotFoundError:
            return (
                self._PaintedPoints__defaultStore(),
                f"External cache does not exist: {cache_path}",
            )
        except Exception as exc:
            return (
                self._PaintedPoints__defaultStore(),
                f"Failed to read scatter paint blob: {exc}",
            )
    else:
        blob_bytes = _blob_bytes_from_object(self["cacheBlob"].getValue())

    if not blob_bytes:
        return self._PaintedPoints__defaultStore(), None

    store, error = _unpack_store_blob(blob_bytes)
    if store is None:
        return self._PaintedPoints__defaultStore(), error

    store["node"] = self._PaintedPoints__nodeMetadata()
    return store, None


def __writeStore(self, store, copy_store=True):
    deep_copy_ms = 0.0
    if copy_store:
        deep_copy_start = time.perf_counter()
        store = copy.deepcopy(store)
        deep_copy_ms = (time.perf_counter() - deep_copy_start) * 1000.0

    node_metadata_start = time.perf_counter()
    store["schemaVersion"] = SCHEMA_VERSION
    store["node"] = self._PaintedPoints__nodeMetadata()
    node_metadata_ms = (time.perf_counter() - node_metadata_start) * 1000.0

    lock_start = time.perf_counter()
    lock_metadata = self._PaintedPoints__ensureWriteLock()
    store["lock"] = lock_metadata
    lock_ms = (time.perf_counter() - lock_start) * 1000.0
    pack_start = time.perf_counter()
    blob_bytes = _pack_store_blob(store)
    pack_ms = (time.perf_counter() - pack_start) * 1000.0
    persist_start = time.perf_counter()

    def write_metrics():
        return {
            "deepCopyMs": deep_copy_ms,
            "nodeMetadataMs": node_metadata_ms,
            "lockMs": lock_ms,
            "packMs": pack_ms,
            "persistMs": (time.perf_counter() - persist_start) * 1000.0,
        }

    if self["cacheMode"].getValue() == CACHE_MODE_EXTERNAL:
        cache_path = self._PaintedPoints__resolvedCachePath()
        if not cache_path:
            raise ValueError("External cache mode requires a cache path")
        self._PaintedPoints__writeBackupIfEnabled(cache_path)
        _write_bytes_file(cache_path, blob_bytes)
        self._PaintedPoints__syncing = True
        try:
            self["cacheBlob"].setValue(IECore.UCharVectorData())
        finally:
            self._PaintedPoints__syncing = False
        self._PaintedPoints__releaseWriteLock(expected_lock=lock_metadata)
        return store, write_metrics()

    self._PaintedPoints__syncing = True
    try:
        self["cacheBlob"].setValue(_blob_object_from_bytes(blob_bytes))
    finally:
        self._PaintedPoints__syncing = False
    return store, write_metrics()


def __validateStore(self, store, load_error=None):
    validation_chunk_point_limit = CHUNK_POINT_LIMIT
    invalid_point_count = 0
    invalid_stroke_count = 0
    topology_mismatch_count = 0
    last_error = load_error or ""
    resolved_attachment_count = 0
    unresolved_attachment_count = 0
    failing_target_paths = set()
    validation_categories = set()

    layer_ids = set()
    stroke_ids = set()
    point_ids = set()
    selection_set_ids = set()
    scene_path_count = len(store.get("scenePaths", []))
    instance_source_path_count = len(store.get("instanceSourcePaths", []))

    if load_error:
        topology_mismatch_count += 1
        validation_categories.add(0)

    for layer in store["layers"]:
        layer_id = layer.get("layerId")
        if layer_id in layer_ids or not layer.get("name"):
            topology_mismatch_count += 1
            validation_categories.add(2)
        layer_ids.add(layer_id)

        layer_strokes = [
            stroke
            for stroke in store["strokes"]
            if int(stroke.get("layerId", 0)) == int(layer_id)
        ]
        sorted_layer_strokes = sorted(
            layer_strokes, key=lambda item: item.get("order", 0)
        )
        expected_first_stroke_id = (
            sorted_layer_strokes[0]["strokeId"] if sorted_layer_strokes else 0
        )
        expected_last_stroke_id = (
            sorted_layer_strokes[-1]["strokeId"] if sorted_layer_strokes else 0
        )
        if int(layer.get("firstStrokeId", 0)) != int(expected_first_stroke_id):
            topology_mismatch_count += 1
            validation_categories.add(2)
        if int(layer.get("lastStrokeId", 0)) != int(expected_last_stroke_id):
            topology_mismatch_count += 1
            validation_categories.add(2)

        for stroke in layer_strokes:
            stroke_id = stroke.get("strokeId")
            if stroke_id in stroke_ids or not stroke.get("name"):
                invalid_stroke_count += 1
                validation_categories.add(0)
            stroke_ids.add(stroke_id)

            points = self._PaintedPoints__strokePoints(store, stroke_id)
            stroke_point_count = len(points)
            if int(stroke.get("pointCount", stroke_point_count)) != stroke_point_count:
                topology_mismatch_count += 1
                validation_categories.add(2)

            chunks = [
                chunk
                for chunk in store["chunks"]
                if int(chunk.get("strokeId", 0)) == int(stroke_id)
            ]
            expected_chunk_count = max(
                1,
                (stroke_point_count + validation_chunk_point_limit - 1)
                // validation_chunk_point_limit,
            )
            if len(chunks) != expected_chunk_count:
                topology_mismatch_count += 1
                validation_categories.add(2)

            chunks = sorted(chunks, key=lambda item: int(item.get("chunkIndex", 0)))

            if chunks:
                if int(stroke.get("firstChunkId", 0)) != int(
                    chunks[0].get("chunkId", 0)
                ):
                    topology_mismatch_count += 1
                    validation_categories.add(2)
                if int(stroke.get("lastChunkId", 0)) != int(
                    chunks[-1].get("chunkId", 0)
                ):
                    topology_mismatch_count += 1
                    validation_categories.add(2)

                expected_point_start = 0
                accumulated_point_count = 0
                for chunk_index, chunk in enumerate(chunks):
                    if int(chunk.get("chunkIndex", 0)) != chunk_index:
                        topology_mismatch_count += 1
                        validation_categories.add(2)
                    if (
                        int(chunk.get("pointStart", expected_point_start))
                        != expected_point_start
                    ):
                        topology_mismatch_count += 1
                        validation_categories.add(2)

                    remaining_point_count = stroke_point_count - accumulated_point_count
                    expected_point_count = (
                        0
                        if remaining_point_count == 0
                        else min(validation_chunk_point_limit, remaining_point_count)
                    )
                    if int(chunk.get("pointCount", 0)) != expected_point_count:
                        topology_mismatch_count += 1
                        validation_categories.add(2)
                    if bool(chunk.get("deleted", False)):
                        topology_mismatch_count += 1
                        validation_categories.add(2)

                    chunk_point_count = int(chunk.get("pointCount", 0))
                    expected_point_start += chunk_point_count
                    accumulated_point_count += chunk_point_count

                if accumulated_point_count != stroke_point_count:
                    topology_mismatch_count += 1
                    validation_categories.add(2)
            elif (
                int(stroke.get("firstChunkId", 0)) != 0
                or int(stroke.get("lastChunkId", 0)) != 0
            ):
                topology_mismatch_count += 1
                validation_categories.add(2)

            for point in points:
                point_id = point.get("pointId")
                if point_id in point_ids:
                    invalid_point_count += 1
                    validation_categories.add(0)
                point_ids.add(point_id)

                if int(point.get("layerId", 0)) != int(layer_id):
                    topology_mismatch_count += 1
                    validation_categories.add(2)
                if int(point.get("strokeId", 0)) != int(stroke_id):
                    topology_mismatch_count += 1
                    validation_categories.add(2)

                if not point.get("valid", True):
                    invalid_point_count += 1
                    validation_categories.add(3)

                barycentric = point.get("barycentric", [])
                if not isinstance(barycentric, (list, tuple)) or len(barycentric) != 3:
                    invalid_point_count += 1
                    validation_categories.add(3)
                    barycentric_sum = 0.0
                else:
                    barycentric_sum = sum(float(value) for value in barycentric)

                if len(barycentric) == 3 and abs(barycentric_sum - 1.0) > 0.01:
                    invalid_point_count += 1
                    validation_categories.add(3)

                target_path_id = int(point.get("targetPathId", 0))
                if target_path_id < 0 or target_path_id > scene_path_count:
                    invalid_point_count += 1
                    topology_mismatch_count += 1
                    validation_categories.add(0)
                    failing_target_paths.add(f"invalidTargetPathId:{target_path_id}")

                instance_source_path_id = int(point.get("instanceSourcePathId", 0))
                if (
                    instance_source_path_id < 0
                    or instance_source_path_id > instance_source_path_count
                ):
                    invalid_point_count += 1
                    topology_mismatch_count += 1
                    validation_categories.add(0)

                if int(point.get("anchorModeUsed", 0)) not in (0, 1, 2, 3):
                    invalid_point_count += 1
                    validation_categories.add(3)

                if int(point.get("anchorModeUsed", 3)) != 3:
                    resolved_attachment_count += 1
                else:
                    unresolved_attachment_count += 1

    for stroke in store["strokes"]:
        if int(stroke.get("layerId", 0)) not in layer_ids:
            invalid_stroke_count += 1
            validation_categories.add(0)

    for chunk in store.get("chunks", []):
        if int(chunk.get("strokeId", 0)) not in stroke_ids:
            topology_mismatch_count += 1
            validation_categories.add(2)

    for selection_set in store["selectionSets"]:
        selection_set_id = int(selection_set.get("selectionSetId", 0))
        if selection_set_id in selection_set_ids or not selection_set.get("name"):
            topology_mismatch_count += 1
            validation_categories.add(0)
        selection_set_ids.add(selection_set_id)
        for point_id in selection_set.get("pointIds", []):
            if int(point_id) not in point_ids:
                topology_mismatch_count += 1
                validation_categories.add(0)
        for stroke_id in selection_set.get("strokeIds", []):
            if int(stroke_id) not in stroke_ids:
                topology_mismatch_count += 1
                validation_categories.add(0)

    current_selection = dict(store.get("currentSelection", {}))
    for point_id in current_selection.get("pointIds", []):
        if int(point_id) not in point_ids:
            topology_mismatch_count += 1
            validation_categories.add(0)
    for stroke_id in current_selection.get("strokeIds", []):
        if int(stroke_id) not in stroke_ids:
            topology_mismatch_count += 1
            validation_categories.add(0)

    lock = dict(store.get("lock", {}))
    if lock:
        if int(lock.get("mode", LOCK_MODE_SESSION_AWARE)) != LOCK_MODE_SESSION_AWARE:
            topology_mismatch_count += 1
            validation_categories.add(1)
        if not str(lock.get("user", "")):
            validation_categories.add(1)

    if store.get("schemaVersion", SCHEMA_VERSION) != SCHEMA_VERSION:
        topology_mismatch_count += 1
        validation_categories.add(5)

    if _find_attached_points_for_painted_node(self) is None:
        validation_categories.add(4)

    summary = (
        f"{len(store['layers'])} layers, "
        f"{len(store['strokes'])} strokes, "
        f"{len(store['points'])} points, "
        f"{len(store['selectionSets'])} selection sets, "
        f"{resolved_attachment_count} resolved attachments, "
        f"{unresolved_attachment_count} fallback attachments"
    )
    if load_error:
        summary = f"Invalid scatter paint blob. {summary}"

    effective_categories = sorted(validation_categories or {6})
    diagnostics = store.setdefault(
        "diagnostics", self._PaintedPoints__emptyDiagnostics()
    )
    diagnostics.update(
        {
            "invalidPointCount": invalid_point_count,
            "invalidStrokeCount": invalid_stroke_count,
            "topologyMismatchCount": topology_mismatch_count,
            "failingFrame": 0,
            "failingTargetPaths": sorted(failing_target_paths),
            "lastErrorMessage": last_error,
            "validationSummary": summary,
            "categories": effective_categories,
            "validationCategories": effective_categories,
        }
    )

    return {
        "invalidPointCount": invalid_point_count,
        "invalidStrokeCount": invalid_stroke_count,
        "topologyMismatchCount": topology_mismatch_count,
        "lastErrorMessage": last_error,
        "validationSummary": summary,
        "failingTargetPaths": sorted(failing_target_paths),
        "validationCategories": effective_categories,
    }


def __trustedValidation(self, store):
    resolved_attachment_count = 0
    unresolved_attachment_count = 0
    for point in store.get("points", []):
        if int(point.get("anchorModeUsed", 3)) == 3:
            unresolved_attachment_count += 1
        else:
            resolved_attachment_count += 1

    validation_categories = {6}
    if _find_attached_points_for_painted_node(self) is None:
        validation_categories.add(4)

    summary = (
        f"{len(store['layers'])} layers, "
        f"{len(store['strokes'])} strokes, "
        f"{len(store['points'])} points, "
        f"{len(store['selectionSets'])} selection sets, "
        f"{resolved_attachment_count} resolved attachments, "
        f"{unresolved_attachment_count} fallback attachments"
    )

    diagnostics = store.setdefault(
        "diagnostics", self._PaintedPoints__emptyDiagnostics()
    )
    diagnostics.update(
        {
            "invalidPointCount": 0,
            "invalidStrokeCount": 0,
            "topologyMismatchCount": 0,
            "failingFrame": 0,
            "failingTargetPaths": [],
            "lastErrorMessage": "",
            "validationSummary": summary,
            "categories": sorted(validation_categories),
            "validationCategories": sorted(validation_categories),
        }
    )

    return {
        "invalidPointCount": 0,
        "invalidStrokeCount": 0,
        "topologyMismatchCount": 0,
        "lastErrorMessage": "",
        "validationSummary": summary,
        "failingTargetPaths": [],
        "validationCategories": sorted(validation_categories),
    }


def __applySyncedState(self, store, validation):
    lock = dict(store.get("lock", {}))
    self._PaintedPoints__authoredPointCount = len(store.get("points", []))
    self["layers"].setValue(
        IECore.StringVectorData(self._PaintedPoints__layerNames(store))
    )
    self["selectionSets"].setValue(
        IECore.StringVectorData(self._PaintedPoints__selectionSetNames(store))
    )
    self["cacheVersion"].setValue(SCHEMA_VERSION)
    self["invalidPointCount"].setValue(validation["invalidPointCount"])
    self["invalidStrokeCount"].setValue(validation["invalidStrokeCount"])
    self["failingFrame"].setValue(0)
    self["failingTargetPaths"].setValue(
        IECore.StringVectorData(validation["failingTargetPaths"])
    )
    self["lastErrorMessage"].setValue(validation["lastErrorMessage"])
    self["topologyMismatchCount"].setValue(validation["topologyMismatchCount"])
    self["validationSummary"].setValue(validation["validationSummary"])
    self["validationCategories"].setValue(
        _validation_category_strings(validation["validationCategories"])
    )
    self["cacheResolvedPath"].setValue(self._PaintedPoints__resolvedCachePath())
    self["cacheLockedBy"].setValue(str(lock.get("user", "")))
    self["cacheLockedHost"].setValue(str(lock.get("host", "")))
    self["cacheLockedTime"].setValue(str(lock.get("timestampUtc", "")))
    self["cacheLockedScript"].setValue(str(lock.get("scriptPath", "")))
    if "cacheDescription" in self:
        self["cacheDescription"].setValue(SCHEMA_DESCRIPTION)


def __syncStateFromStore(self, store=None, load_error=None, trusted=False):
    if store is None:
        store, load_error = self._PaintedPoints__loadStore()

    validate_start = time.perf_counter()
    if trusted and load_error is None:
        validation = self._PaintedPoints__trustedValidation(store)
    else:
        validation = self._PaintedPoints__validateStore(store, load_error)
    validate_ms = (time.perf_counter() - validate_start) * 1000.0

    self._PaintedPoints__syncing = True
    try:
        plug_sync_start = time.perf_counter()
        self._PaintedPoints__applySyncedState(store, validation)
        plug_sync_ms = (time.perf_counter() - plug_sync_start) * 1000.0
    finally:
        self._PaintedPoints__syncing = False

    return {
        "validateMs": validate_ms,
        "plugSyncMs": plug_sync_ms,
    }


def __plugSet(self, plug):
    if self._PaintedPoints__syncing:
        return

    if plug.isSame(self["cacheBlob"]):
        self._PaintedPoints__syncStateFromStore()
        return

    if (
        plug.isSame(self["cacheMode"])
        or plug.isSame(self["cachePath"])
        or plug.isSame(self["cachePathMode"])
        or plug.isSame(self["projectRoot"])
        or plug.isSame(self["relaxObjective"])
        or plug.isSame(self["defaultColor"])
    ):
        self._PaintedPoints__syncStateFromStore()


def __finalizeStoreMutation(
    self,
    store,
    refresh=True,
    changed_stroke_ids=None,
    mutation_label=None,
    copy_store=True,
):
    refresh_ms = 0.0
    write_ms = 0.0
    sync_ms = 0.0
    validate_ms = 0.0
    plug_sync_ms = 0.0
    total_start = time.perf_counter()

    if refresh:
        refresh_start = time.perf_counter()
        self._PaintedPoints__refreshDerivedData(
            store, changed_stroke_ids=changed_stroke_ids
        )
        refresh_ms = (time.perf_counter() - refresh_start) * 1000.0

    write_start = time.perf_counter()
    synced_store, write_metrics = self._PaintedPoints__writeStore(
        store, copy_store=copy_store
    )
    write_ms = (time.perf_counter() - write_start) * 1000.0

    sync_start = time.perf_counter()
    sync_metrics = self._PaintedPoints__syncStateFromStore(synced_store, trusted=True)
    sync_ms = (time.perf_counter() - sync_start) * 1000.0
    validate_ms = sync_metrics["validateMs"]
    plug_sync_ms = sync_metrics["plugSyncMs"]

    if mutation_label:
        IECore.msg(
            IECore.Msg.Level.Info,
            f"GafferScatterPaint.{mutation_label}",
            (
                "finalizeMutation totalMs={:.3f} refreshMs={:.3f} "
                "writeMs={:.3f} deepCopyMs={:.3f} nodeMetadataMs={:.3f} "
                "lockMs={:.3f} packMs={:.3f} persistMs={:.3f} "
                "syncMs={:.3f} validateMs={:.3f} plugSyncMs={:.3f}"
            ).format(
                (time.perf_counter() - total_start) * 1000.0,
                refresh_ms,
                write_ms,
                write_metrics["deepCopyMs"],
                write_metrics["nodeMetadataMs"],
                write_metrics["lockMs"],
                write_metrics["packMs"],
                write_metrics["persistMs"],
                sync_ms,
                validate_ms,
                plug_sync_ms,
            ),
        )


def __mutateStore(
    self,
    mutator,
    refresh=True,
    changed_stroke_ids=None,
    mutation_label=None,
    copy_store=True,
):
    store, _load_error = self._PaintedPoints__loadStore()
    result = mutator(store)
    self._PaintedPoints__finalizeStoreMutation(
        store,
        refresh=refresh,
        changed_stroke_ids=changed_stroke_ids,
        mutation_label=mutation_label,
        copy_store=copy_store,
    )
    return result


def migrateCacheMode(self):
    store, _load_error = self._PaintedPoints__loadStore()
    current_mode = self["cacheMode"].getValue()
    self._PaintedPoints__syncing = True
    try:
        if current_mode == CACHE_MODE_EXTERNAL:
            self["cacheMode"].setValue(CACHE_MODE_EMBEDDED)
            self._PaintedPoints__writeStore(store)
        else:
            if not self._PaintedPoints__resolvedCachePath():
                raise ValueError("External cache mode requires a cache path")
            self["cacheMode"].setValue(CACHE_MODE_EXTERNAL)
            self._PaintedPoints__writeStore(store)
    finally:
        self._PaintedPoints__syncing = False
    self._PaintedPoints__syncStateFromStore()
    return SCHEMA_VERSION


def relinkCache(self):
    if self["cacheMode"].getValue() != CACHE_MODE_EXTERNAL:
        raise RuntimeError("Relink is only valid in external cache mode")
    cache_path = self._PaintedPoints__resolvedCachePath()
    if not cache_path or not os.path.exists(cache_path):
        raise RuntimeError(f"External cache does not exist: {cache_path}")
    store, error = self._PaintedPoints__loadStore()
    if error:
        raise RuntimeError(error)
    self._PaintedPoints__syncStateFromStore()
    return cache_path


def upgradeCache(self):
    store, error = self._PaintedPoints__loadStore()
    if error:
        raise RuntimeError(error)
    source_version = int(store.get("schemaVersion", SCHEMA_VERSION))
    if source_version == SCHEMA_VERSION:
        self._PaintedPoints__syncStateFromStore()
        return SCHEMA_VERSION

    upgraded_store = copy.deepcopy(store)
    upgraded_store["schemaVersion"] = SCHEMA_VERSION
    upgraded_store.setdefault("upgrades", []).append(
        {
            "sourceSchemaVersion": source_version,
            "upgradedSchemaVersion": SCHEMA_VERSION,
            "timestampUtc": _utc_timestamp(),
            "report": f"Upgraded scatter paint blob from schema v{source_version} to v{SCHEMA_VERSION}.",
        }
    )

    if self["cacheMode"].getValue() == CACHE_MODE_EXTERNAL:
        cache_path = self._PaintedPoints__resolvedCachePath()
        if not cache_path:
            raise RuntimeError("External cache mode requires a cache path")
        upgraded_path = _cache_text_path(cache_path, f".v{SCHEMA_VERSION}.upgrade")
        original_cache_path = self["cachePath"].getValue()
        self._PaintedPoints__syncing = True
        try:
            self["cachePath"].setValue(upgraded_path)
            self._PaintedPoints__writeStore(upgraded_store)
            self["cachePath"].setValue(original_cache_path)
        finally:
            self._PaintedPoints__syncing = False
        self._PaintedPoints__syncStateFromStore()
        return upgraded_path

    self._PaintedPoints__writeStore(upgraded_store)
    self._PaintedPoints__syncStateFromStore()
    return SCHEMA_VERSION


def validateCache(self):
    self._PaintedPoints__syncStateFromStore()
    return self["validationSummary"].getValue()


def validateAttachments(self):
    store, error = self._PaintedPoints__loadStore()
    if error:
        raise RuntimeError(error)
    unresolved_count = sum(
        1
        for point in store.get("points", [])
        if int(point.get("anchorModeUsed", 3)) == 3
    )
    result = f"{len(store.get('points', []))} authored points, {unresolved_count} unresolved attachment fallbacks"
    return result


def compactCache(self):
    def compact(store):
        self._PaintedPoints__refreshDerivedData(
            store,
            changed_stroke_ids=[
                stroke["strokeId"] for stroke in store.get("strokes", [])
            ],
        )
        return len(store.get("chunks", []))

    return self._PaintedPoints__mutateStore(compact)


def cacheSnapshot(self):
    store, error = self._PaintedPoints__loadStore()
    if error:
        raise RuntimeError(error)
    return copy.deepcopy(store)


def mutateCacheStore(
    self,
    mutator,
    refresh=True,
    changed_stroke_ids=None,
    mutation_label=None,
    copy_store=True,
):
    store, error = self._PaintedPoints__loadStore()
    if error:
        raise RuntimeError(error)
    result = mutator(store)
    self._PaintedPoints__finalizeStoreMutation(
        store,
        refresh=refresh,
        changed_stroke_ids=changed_stroke_ids,
        mutation_label=mutation_label,
        copy_store=copy_store,
    )
    return result
