import Gaffer
import GafferScene
import IECore

import GafferScatterPaint
def _build_demo_graph(script):
    plane = GafferScene.Plane("ScatterPaintPlane")
    plane["name"].setValue("paintPlane")
    script.addChild(plane)

    painted_points = GafferScatterPaint.PaintedPoints("PaintedPoints")
    script.addChild(painted_points)
    painted_points["in"].setInput(plane["out"])

    attached_points = GafferScatterPaint.AttachedPoints("AttachedPoints")
    script.addChild(attached_points)
    attached_points["in"].setInput(plane["out"])
    attached_points["points"].setInput(painted_points["out"])
    attached_points["outputLocation"].setValue("/paintPlane/scatter")

    layer_id = painted_points.createLayer("Demo Layer")
    stroke_id = painted_points.createStroke(layer_id, "Demo Stroke")
    painted_points.paintStrokeCommit(
        stroke_id,
        [
            {
                "P": [0.0, 0.0, 0.0],
                "width": 0.1,
                "scale": 1.0,
                "seed": 0,
                "sourcePath": "/paintPlane",
                "triangleIndex": 0,
                "barycentric": [0.5, 0.25, 0.25],
                "attachmentResolved": True,
            },
            {
                "P": [0.2, 0.0, 0.2],
                "width": 0.1,
                "scale": 1.0,
                "seed": 1,
                "sourcePath": "/paintPlane",
                "triangleIndex": 1,
                "barycentric": [0.25, 0.5, 0.25],
                "attachmentResolved": True,
            },
        ],
        append=True,
    )

    return plane, painted_points, attached_points


def _load_store(painted_points):
    return painted_points.cacheSnapshot()


def _blob_bytes(painted_points):
    return bytes(int(value) & 0xFF for value in painted_points["cacheBlob"].getValue())


def _set_blob_bytes(painted_points, blob_bytes):
    painted_points["cacheBlob"].setValue(IECore.UCharVectorData(list(blob_bytes)))


def _validate_cache(painted_points):
    summary = painted_points.validateCache()
    return {
        "summary": summary,
        "invalid": painted_points["invalidPointCount"].getValue(),
        "invalidStrokes": painted_points["invalidStrokeCount"].getValue(),
        "topologyMismatches": painted_points["topologyMismatchCount"].getValue(),
        "failingPaths": list(painted_points["failingTargetPaths"].getValue()),
        "lastError": painted_points["lastErrorMessage"].getValue(),
    }


def _backup_point(point):
    backup = point.get("_demoAttachmentBackup")
    if isinstance(backup, dict):
        return backup

    backup = {
        "sourcePath": point.get("sourcePath", ""),
        "triangleIndex": int(point.get("triangleIndex", 0)),
        "barycentric": list(point.get("barycentric", [1.0, 0.0, 0.0])),
        "attachmentResolved": bool(point.get("attachmentResolved", False)),
    }
    point["_demoAttachmentBackup"] = backup
    return backup


def _mutate_points(painted_points, mutator):
    return painted_points.mutatePoints(mutator)


def _break_paths(painted_points):
    def mutate(point):
        _backup_point(point)
        point["sourcePath"] = "/paintPlaneMissing"
        point["attachmentResolved"] = True
        return True

    return _mutate_points(painted_points, mutate)


def _break_triangles(painted_points):
    def mutate(point):
        _backup_point(point)
        point["triangleIndex"] = int(point.get("triangleIndex", 0)) + 100000
        point["attachmentResolved"] = True
        return True

    return _mutate_points(painted_points, mutate)


def _repair(painted_points):
    def mutate(point):
        backup = point.get("_demoAttachmentBackup")
        if not isinstance(backup, dict):
            return False
        point["sourcePath"] = backup.get("sourcePath", point.get("sourcePath", ""))
        point["triangleIndex"] = int(backup.get("triangleIndex", point.get("triangleIndex", 0)))
        point["barycentric"] = list(backup.get("barycentric", point.get("barycentric", [1.0, 0.0, 0.0])))
        point["attachmentResolved"] = bool(
            backup.get("attachmentResolved", point.get("attachmentResolved", False))
        )
        del point["_demoAttachmentBackup"]
        return True

    return _mutate_points(painted_points, mutate)


def _validate(attached_points):
    output_location = attached_points["outputLocation"].getValue().strip() or "/scatter"
    attached_points["out"].object(output_location)
    return {
        "summary": attached_points["solveStatus"].getValue(),
        "resolved": attached_points["resolvedPointCount"].getValue(),
        "unresolved": attached_points["unresolvedPointCount"].getValue(),
        "invalid": attached_points["invalidPointCount"].getValue(),
        "topologyMismatches": attached_points["topologyMismatchCount"].getValue(),
        "failureReasons": list(attached_points["attachmentFailureReasons"].getValue()),
        "failingPaths": list(attached_points["failingTargetPaths"].getValue()),
    }


def _expect(condition, message):
    if not condition:
        raise AssertionError(message)


def run():
    script = Gaffer.ScriptNode()
    _plane, painted_points, attached_points = _build_demo_graph(script)

    healthy = _validate(attached_points)
    _expect(healthy["resolved"] > 0, f"Expected resolved demo attachments, got {healthy}")
    _expect(not healthy["failureReasons"], f"Expected no healthy failure reasons, got {healthy}")

    _break_paths(painted_points)
    broken_paths = _validate(attached_points)
    _expect(
        any(reason.startswith("missingTargetPath:") for reason in broken_paths["failureReasons"]),
        f"Expected missingTargetPath failure, got {broken_paths}",
    )
    _expect(broken_paths["unresolved"] > 0, f"Expected unresolved path failures, got {broken_paths}")

    _repair(painted_points)
    repaired_paths = _validate(attached_points)
    _expect(repaired_paths["resolved"] > 0, f"Expected repaired path attachments, got {repaired_paths}")
    _expect(not repaired_paths["failureReasons"], f"Expected no path failure reasons after repair, got {repaired_paths}")

    _break_triangles(painted_points)
    broken_triangles = _validate(attached_points)
    _expect(
        any(reason.startswith("missingTriangle:") for reason in broken_triangles["failureReasons"]),
        f"Expected missingTriangle failure, got {broken_triangles}",
    )
    _expect(
        broken_triangles["topologyMismatches"] > 0,
        f"Expected topology mismatches for broken triangles, got {broken_triangles}",
    )

    _repair(painted_points)
    repaired_triangles = _validate(attached_points)
    _expect(
        repaired_triangles["resolved"] > 0,
        f"Expected repaired triangle attachments, got {repaired_triangles}",
    )
    _expect(
        not repaired_triangles["failureReasons"],
        f"Expected no triangle failure reasons after repair, got {repaired_triangles}",
    )

    round_trip_cache = _validate_cache(painted_points)
    _expect(round_trip_cache["invalid"] == 0, f"Expected healthy cache, got {round_trip_cache}")

    bad_magic_bytes = bytearray(_blob_bytes(painted_points))
    bad_magic_bytes[0:8] = b"BADPAINT"
    _set_blob_bytes(painted_points, bytes(bad_magic_bytes))
    bad_magic_cache = _validate_cache(painted_points)
    _expect("bad magic" in bad_magic_cache["lastError"], f"Expected bad magic error, got {bad_magic_cache}")

    _plane, painted_points, attached_points = _build_demo_graph(Gaffer.ScriptNode())
    checksum_bytes = bytearray(_blob_bytes(painted_points))
    checksum_bytes[-1] ^= 0x01
    _set_blob_bytes(painted_points, bytes(checksum_bytes))
    checksum_cache = _validate_cache(painted_points)
    _expect("checksum mismatch" in checksum_cache["lastError"], f"Expected checksum mismatch, got {checksum_cache}")

    _plane, painted_points, attached_points = _build_demo_graph(Gaffer.ScriptNode())

    def break_path_id(store):
        store["points"][0]["targetPathId"] = len(store["scenePaths"]) + 99

    painted_points.mutateCacheStore(break_path_id)
    broken_path_cache = _validate_cache(painted_points)
    _expect(broken_path_cache["invalid"] > 0, f"Expected invalid cache point count, got {broken_path_cache}")

    selection_id = painted_points.createSelectionSet("Broken Selection")

    def break_selection(store):
        selection = next(item for item in store["selectionSets"] if item["selectionSetId"] == selection_id)
        selection["pointIds"] = [999999]
        selection["strokeIds"] = [888888]

    painted_points.mutateCacheStore(break_selection)
    broken_selection_cache = _validate_cache(painted_points)
    _expect(
        broken_selection_cache["topologyMismatches"] > 0,
        f"Expected topology mismatch for broken selection references, got {broken_selection_cache}",
    )

    IECore.msg(
        IECore.Msg.Level.Info,
        "GafferScatterPaintRegression",
        "Attachment and cache blob regression passed for attachment, corruption, and broken-reference workflows.",
    )
    return {
        "healthy": healthy,
        "brokenPaths": broken_paths,
        "repairedPaths": repaired_paths,
        "brokenTriangles": broken_triangles,
        "repairedTriangles": repaired_triangles,
        "roundTripCache": round_trip_cache,
        "badMagicCache": bad_magic_cache,
        "checksumCache": checksum_cache,
        "brokenPathCache": broken_path_cache,
        "brokenSelectionCache": broken_selection_cache,
    }


if __name__ == "__main__":
    run()
