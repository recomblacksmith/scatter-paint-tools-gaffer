import unittest

import Gaffer
import GafferScene
import IECore

import GafferScatterPaint
class AttachmentRegressionTest(unittest.TestCase):

    def _build_demo_graph(self):
        script = Gaffer.ScriptNode()

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

        return script, plane, painted_points, attached_points

    def _load_store(self, painted_points):
        return painted_points.cacheSnapshot()

    def _blob_bytes(self, painted_points):
        return bytes(int(value) & 0xFF for value in painted_points["cacheBlob"].getValue())

    def _set_blob_bytes(self, painted_points, blob_bytes):
        painted_points["cacheBlob"].setValue(IECore.UCharVectorData(list(blob_bytes)))

    def _validate_cache(self, painted_points):
        summary = painted_points.validateCache()
        return {
            "summary": summary,
            "invalid": painted_points["invalidPointCount"].getValue(),
            "invalidStrokes": painted_points["invalidStrokeCount"].getValue(),
            "topologyMismatches": painted_points["topologyMismatchCount"].getValue(),
            "failingPaths": list(painted_points["failingTargetPaths"].getValue()),
            "lastError": painted_points["lastErrorMessage"].getValue(),
        }

    def _backup_point(self, point):
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

    def _mutate_points(self, painted_points, mutator):
        return painted_points.mutatePoints(mutator)

    def _break_paths(self, painted_points):
        return self._mutate_points(
            painted_points,
            lambda point: self._break_path_point(point),
        )

    def _break_path_point(self, point):
        self._backup_point(point)
        point["sourcePath"] = "/paintPlaneMissing"
        point["attachmentResolved"] = True
        return True

    def _break_triangles(self, painted_points):
        return self._mutate_points(
            painted_points,
            lambda point: self._break_triangle_point(point),
        )

    def _break_triangle_point(self, point):
        self._backup_point(point)
        point["triangleIndex"] = int(point.get("triangleIndex", 0)) + 100000
        point["attachmentResolved"] = True
        return True

    def _repair(self, painted_points):
        return self._mutate_points(
            painted_points,
            lambda point: self._repair_point(point),
        )

    def _repair_point(self, point):
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

    def _validate(self, attached_points):
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

    def testHealthyBreakPathRepair(self):
        _script, _plane, painted_points, attached_points = self._build_demo_graph()

        healthy = self._validate(attached_points)
        self.assertGreater(healthy["resolved"], 0)
        self.assertFalse(healthy["failureReasons"])

        self.assertGreater(self._break_paths(painted_points), 0)
        broken = self._validate(attached_points)
        self.assertTrue(any(reason.startswith("missingTargetPath:") for reason in broken["failureReasons"]))
        self.assertGreater(broken["unresolved"], 0)

        self.assertGreater(self._repair(painted_points), 0)
        repaired = self._validate(attached_points)
        self.assertGreater(repaired["resolved"], 0)
        self.assertFalse(repaired["failureReasons"])

    def testHealthyBreakTriangleRepair(self):
        _script, _plane, painted_points, attached_points = self._build_demo_graph()

        healthy = self._validate(attached_points)
        self.assertGreater(healthy["resolved"], 0)
        self.assertFalse(healthy["failureReasons"])

        self.assertGreater(self._break_triangles(painted_points), 0)
        broken = self._validate(attached_points)
        self.assertTrue(any(reason.startswith("missingTriangle:") for reason in broken["failureReasons"]))
        self.assertGreater(broken["topologyMismatches"], 0)

        self.assertGreater(self._repair(painted_points), 0)
        repaired = self._validate(attached_points)
        self.assertGreater(repaired["resolved"], 0)
        self.assertFalse(repaired["failureReasons"])

    def testMissingSourcePath(self):
        _script, _plane, painted_points, attached_points = self._build_demo_graph()

        def clear_source(point):
            point["sourcePath"] = ""
            point["attachmentResolved"] = True
            return True

        self.assertGreater(self._mutate_points(painted_points, clear_source), 0)
        result = self._validate(attached_points)
        self.assertTrue(any(reason.startswith("missingSourcePath:") for reason in result["failureReasons"]))

    def testUnsupportedTargetObject(self):
        script = Gaffer.ScriptNode()

        sphere = GafferScene.Sphere("ScatterPaintSphere")
        sphere["name"].setValue("paintSphere")
        script.addChild(sphere)

        painted_points = GafferScatterPaint.PaintedPoints("PaintedPoints")
        script.addChild(painted_points)
        painted_points["in"].setInput(sphere["out"])

        attached_points = GafferScatterPaint.AttachedPoints("AttachedPoints")
        script.addChild(attached_points)
        attached_points["in"].setInput(sphere["out"])
        attached_points["points"].setInput(painted_points["out"])
        attached_points["outputLocation"].setValue("/paintSphere/scatter")

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
                    "sourcePath": "/paintSphere",
                    "triangleIndex": 0,
                    "barycentric": [1.0, 0.0, 0.0],
                    "attachmentResolved": True,
                }
            ],
            append=True,
        )

        result = self._validate(attached_points)
        self.assertTrue(any(reason.startswith("unsupportedTargetObject:") for reason in result["failureReasons"]))

    def testCacheBlobRoundTripSnapshot(self):
        _script, _plane, painted_points, _attached_points = self._build_demo_graph()

        store = painted_points.cacheSnapshot()
        point_records = painted_points.pointRecords()

        self.assertEqual(store["schemaVersion"], 1)
        self.assertEqual(len(store["layers"]), 1)
        self.assertEqual(len(store["strokes"]), 1)
        self.assertEqual(len(store["chunks"]), 1)
        self.assertEqual(len(store["points"]), 2)
        self.assertEqual(len(point_records), 2)
        self.assertEqual(point_records[0]["sourcePath"], "/paintPlane")

    def testCacheBlobRejectsBadMagic(self):
        _script, _plane, painted_points, _attached_points = self._build_demo_graph()

        blob_bytes = bytearray(self._blob_bytes(painted_points))
        blob_bytes[0:8] = b"BADPAINT"
        self._set_blob_bytes(painted_points, bytes(blob_bytes))

        result = self._validate_cache(painted_points)
        self.assertIn("bad magic", result["lastError"])
        self.assertIn("Invalid scatter paint blob", result["summary"])

    def testCacheBlobRejectsChecksumCorruption(self):
        _script, _plane, painted_points, _attached_points = self._build_demo_graph()

        blob_bytes = bytearray(self._blob_bytes(painted_points))
        blob_bytes[-1] ^= 0x01
        self._set_blob_bytes(painted_points, bytes(blob_bytes))

        result = self._validate_cache(painted_points)
        self.assertIn("checksum mismatch", result["lastError"])
        self.assertIn("Invalid scatter paint blob", result["summary"])

    def testCacheValidationDetectsBrokenPointPathReference(self):
        _script, _plane, painted_points, _attached_points = self._build_demo_graph()

        def break_path_id(store):
            self.assertTrue(store["points"])
            store["points"][0]["targetPathId"] = len(store["scenePaths"]) + 99

        painted_points.mutateCacheStore(break_path_id)
        result = self._validate_cache(painted_points)

        self.assertGreater(result["invalid"], 0)
        self.assertGreater(result["topologyMismatches"], 0)
        self.assertTrue(result["failingPaths"])

    def testCacheValidationDetectsBrokenSelectionReference(self):
        _script, _plane, painted_points, _attached_points = self._build_demo_graph()

        selection_id = painted_points.createSelectionSet("Broken Selection")

        def break_selection(store):
            selection = next(item for item in store["selectionSets"] if item["selectionSetId"] == selection_id)
            selection["pointIds"] = [999999]
            selection["strokeIds"] = [888888]

        painted_points.mutateCacheStore(break_selection)
        result = self._validate_cache(painted_points)

        self.assertGreater(result["topologyMismatches"], 0)
        self.assertIn("selection sets", result["summary"])


if __name__ == "__main__":
    unittest.main()
