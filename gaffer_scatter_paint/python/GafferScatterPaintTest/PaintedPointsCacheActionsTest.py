import math
import os
import tempfile
import unittest

import Gaffer
import GafferScene
import IECore
import IECoreScene
import imath

import GafferScatterPaint
from GafferScatterPaint import _core as GafferScatterPaintCore
from GafferScatterPaint import _core_shared as GafferScatterPaintShared
from GafferScatterPaint import _storebridge as GafferScatterPaintStoreBridge
import GafferScatterPaintUI.actions as GafferScatterPaintActions


class PaintedPointsCacheActionsTest(unittest.TestCase):
    def _build_empty_graph(self):
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

        return script, plane, painted_points, attached_points

    def _build_demo_graph(self):
        script, plane, painted_points, attached_points = self._build_empty_graph()

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

    def _build_transformed_graph(self):
        script = Gaffer.ScriptNode()

        plane = GafferScene.Plane("ScatterPaintPlane")
        plane["name"].setValue("paintPlane")
        script.addChild(plane)

        plane_filter = GafferScene.PathFilter("PlaneFilter")
        plane_filter["paths"].setValue(IECore.StringVectorData(["/paintPlane"]))
        script.addChild(plane_filter)

        transform = GafferScene.Transform("PlaneTransform")
        script.addChild(transform)
        transform["in"].setInput(plane["out"])
        transform["filter"].setInput(plane_filter["out"])
        transform["transform"]["translate"].setValue(imath.V3f(3.0, 0.0, 0.0))

        painted_points = GafferScatterPaint.PaintedPoints("PaintedPoints")
        script.addChild(painted_points)
        painted_points["in"].setInput(transform["out"])

        attached_points = GafferScatterPaint.AttachedPoints("AttachedPoints")
        script.addChild(attached_points)
        attached_points["in"].setInput(transform["out"])
        attached_points["points"].setInput(painted_points["out"])
        attached_points["outputLocation"].setValue("/paintPlane/scatter")

        return script, plane, transform, painted_points, attached_points

    def _build_multi_mesh_graph(self):
        script = Gaffer.ScriptNode()

        plane = GafferScene.Plane("ScatterPaintPlane")
        plane["name"].setValue("paintPlane")
        script.addChild(plane)

        other_plane = GafferScene.Plane("OtherScatterPlane")
        other_plane["name"].setValue("otherPlane")
        script.addChild(other_plane)

        other_filter = GafferScene.PathFilter("OtherPlaneFilter")
        other_filter["paths"].setValue(IECore.StringVectorData(["/otherPlane"]))
        script.addChild(other_filter)

        other_transform = GafferScene.Transform("OtherPlaneTransform")
        script.addChild(other_transform)
        other_transform["in"].setInput(other_plane["out"])
        other_transform["filter"].setInput(other_filter["out"])
        other_transform["transform"]["translate"].setValue(imath.V3f(3.0, 0.0, 0.0))

        group = GafferScene.Group("ScatterPaintGroup")
        script.addChild(group)
        group["name"].setValue("root")
        group["in"][0].setInput(plane["out"])
        group["in"][1].setInput(other_transform["out"])

        other_set_filter = GafferScene.PathFilter("OtherPlaneSetFilter")
        other_set_filter["paths"].setValue(
            IECore.StringVectorData(["/root/otherPlane"])
        )
        script.addChild(other_set_filter)

        other_set = GafferScene.Set("OtherPlaneSet")
        script.addChild(other_set)
        other_set["in"].setInput(group["out"])
        other_set["filter"].setInput(other_set_filter["out"])
        other_set["name"].setValue("otherTargets")

        painted_points = GafferScatterPaint.PaintedPoints("PaintedPoints")
        script.addChild(painted_points)
        painted_points["in"].setInput(other_set["out"])

        attached_points = GafferScatterPaint.AttachedPoints("AttachedPoints")
        script.addChild(attached_points)
        attached_points["in"].setInput(other_set["out"])
        attached_points["points"].setInput(painted_points["out"])
        attached_points["outputLocation"].setValue("/root/scatter")

        return (
            script,
            plane,
            other_plane,
            group,
            other_set,
            painted_points,
            attached_points,
        )

    def _build_instanced_graph(self):
        script = Gaffer.ScriptNode()

        plane = GafferScene.Plane("ScatterPaintPlane")
        plane["name"].setValue("paintPlane")
        script.addChild(plane)

        prototype = GafferScene.Plane("ScatterPaintPrototype")
        prototype["name"].setValue("paintProto")
        script.addChild(prototype)

        plane_filter = GafferScene.PathFilter("PlaneFilter")
        plane_filter["paths"].setValue(IECore.StringVectorData(["/paintPlane"]))
        script.addChild(plane_filter)

        instancer = GafferScene.Instancer("PlaneInstancer")
        script.addChild(instancer)
        instancer["in"].setInput(plane["out"])
        instancer["prototypes"].setInput(prototype["out"])
        instancer["filter"].setInput(plane_filter["out"])

        painted_points = GafferScatterPaint.PaintedPoints("PaintedPoints")
        script.addChild(painted_points)
        painted_points["in"].setInput(instancer["out"])

        attached_points = GafferScatterPaint.AttachedPoints("AttachedPoints")
        script.addChild(attached_points)
        attached_points["in"].setInput(instancer["out"])
        attached_points["points"].setInput(painted_points["out"])
        attached_points["outputLocation"].setValue("/paintPlane/scatter")

        return script, plane, prototype, instancer, painted_points, attached_points

    def _build_empty_graph_with_classes(
        self,
        painted_points_class=GafferScatterPaint.PaintedPoints,
        attached_points_class=GafferScatterPaint.AttachedPoints,
    ):
        script = Gaffer.ScriptNode()

        plane = GafferScene.Plane("ScatterPaintPlane")
        plane["name"].setValue("paintPlane")
        script.addChild(plane)

        painted_points = painted_points_class("PaintedPoints")
        script.addChild(painted_points)
        painted_points["in"].setInput(plane["out"])

        attached_points = attached_points_class("AttachedPoints")
        script.addChild(attached_points)
        attached_points["in"].setInput(plane["out"])
        attached_points["points"].setInput(painted_points["out"])
        attached_points["outputLocation"].setValue("/paintPlane/scatter")

        return script, plane, painted_points, attached_points

    def testDemoApiHelperReportsMissingMethods(self):
        script, _plane, painted_points, _attached_points = self._build_empty_graph()

        self.assertTrue(
            GafferScatterPaintActions._supports_demo_api(
                painted_points, "ensureLayer", "ensureStroke", "paintStrokeCommit"
            )
        )
        self.assertFalse(
            GafferScatterPaintActions._supports_demo_api(
                painted_points, "ensureLayer", "definitelyMissingDemoMethod"
            )
        )

        with self.assertRaisesRegex(RuntimeError, "definitelyMissingDemoMethod"):
            GafferScatterPaintActions._require_demo_api(
                painted_points,
                "Test Demo",
                "ensureLayer",
                "definitelyMissingDemoMethod",
            )

    def _build_demo_graph_with_classes(
        self,
        painted_points_class=GafferScatterPaint.PaintedPoints,
        attached_points_class=GafferScatterPaint.AttachedPoints,
    ):
        script, plane, painted_points, attached_points = (
            self._build_empty_graph_with_classes(
                painted_points_class=painted_points_class,
                attached_points_class=attached_points_class,
            )
        )

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

    def _build_transformed_graph_with_classes(
        self,
        painted_points_class=GafferScatterPaint.PaintedPoints,
        attached_points_class=GafferScatterPaint.AttachedPoints,
    ):
        script = Gaffer.ScriptNode()

        plane = GafferScene.Plane("ScatterPaintPlane")
        plane["name"].setValue("paintPlane")
        script.addChild(plane)

        plane_filter = GafferScene.PathFilter("PlaneFilter")
        plane_filter["paths"].setValue(IECore.StringVectorData(["/paintPlane"]))
        script.addChild(plane_filter)

        transform = GafferScene.Transform("PlaneTransform")
        script.addChild(transform)
        transform["in"].setInput(plane["out"])
        transform["filter"].setInput(plane_filter["out"])
        transform["transform"]["translate"].setValue(imath.V3f(3.0, 0.0, 0.0))

        painted_points = painted_points_class("PaintedPoints")
        script.addChild(painted_points)
        painted_points["in"].setInput(transform["out"])

        attached_points = attached_points_class("AttachedPoints")
        script.addChild(attached_points)
        attached_points["in"].setInput(transform["out"])
        attached_points["points"].setInput(painted_points["out"])
        attached_points["outputLocation"].setValue("/paintPlane/scatter")

        return script, plane, transform, painted_points, attached_points

    def _build_multi_mesh_graph_with_classes(
        self,
        painted_points_class=GafferScatterPaint.PaintedPoints,
        attached_points_class=GafferScatterPaint.AttachedPoints,
    ):
        script = Gaffer.ScriptNode()

        plane = GafferScene.Plane("ScatterPaintPlane")
        plane["name"].setValue("paintPlane")
        script.addChild(plane)

        other_plane = GafferScene.Plane("OtherScatterPlane")
        other_plane["name"].setValue("otherPlane")
        script.addChild(other_plane)

        other_filter = GafferScene.PathFilter("OtherPlaneFilter")
        other_filter["paths"].setValue(IECore.StringVectorData(["/otherPlane"]))
        script.addChild(other_filter)

        other_transform = GafferScene.Transform("OtherPlaneTransform")
        script.addChild(other_transform)
        other_transform["in"].setInput(other_plane["out"])
        other_transform["filter"].setInput(other_filter["out"])
        other_transform["transform"]["translate"].setValue(imath.V3f(3.0, 0.0, 0.0))

        group = GafferScene.Group("ScatterPaintGroup")
        script.addChild(group)
        group["name"].setValue("root")
        group["in"][0].setInput(plane["out"])
        group["in"][1].setInput(other_transform["out"])

        other_set_filter = GafferScene.PathFilter("OtherPlaneSetFilter")
        other_set_filter["paths"].setValue(
            IECore.StringVectorData(["/root/otherPlane"])
        )
        script.addChild(other_set_filter)

        other_set = GafferScene.Set("OtherPlaneSet")
        script.addChild(other_set)
        other_set["in"].setInput(group["out"])
        other_set["filter"].setInput(other_set_filter["out"])
        other_set["name"].setValue("otherTargets")

        painted_points = painted_points_class("PaintedPoints")
        script.addChild(painted_points)
        painted_points["in"].setInput(other_set["out"])

        attached_points = attached_points_class("AttachedPoints")
        script.addChild(attached_points)
        attached_points["in"].setInput(other_set["out"])
        attached_points["points"].setInput(painted_points["out"])
        attached_points["outputLocation"].setValue("/root/scatter")

        return (
            script,
            plane,
            other_plane,
            group,
            other_set,
            painted_points,
            attached_points,
        )

    def _build_instanced_graph_with_classes(
        self,
        painted_points_class=GafferScatterPaint.PaintedPoints,
        attached_points_class=GafferScatterPaint.AttachedPoints,
    ):
        script = Gaffer.ScriptNode()

        plane = GafferScene.Plane("ScatterPaintPlane")
        plane["name"].setValue("paintPlane")
        script.addChild(plane)

        prototype = GafferScene.Plane("ScatterPaintPrototype")
        prototype["name"].setValue("paintProto")
        script.addChild(prototype)

        plane_filter = GafferScene.PathFilter("PlaneFilter")
        plane_filter["paths"].setValue(IECore.StringVectorData(["/paintPlane"]))
        script.addChild(plane_filter)

        instancer = GafferScene.Instancer("PlaneInstancer")
        script.addChild(instancer)
        instancer["in"].setInput(plane["out"])
        instancer["prototypes"].setInput(prototype["out"])
        instancer["filter"].setInput(plane_filter["out"])

        painted_points = painted_points_class("PaintedPoints")
        script.addChild(painted_points)
        painted_points["in"].setInput(instancer["out"])

        attached_points = attached_points_class("AttachedPoints")
        script.addChild(attached_points)
        attached_points["in"].setInput(instancer["out"])
        attached_points["points"].setInput(painted_points["out"])
        attached_points["outputLocation"].setValue("/paintPlane/scatter")

        return script, plane, prototype, instancer, painted_points, attached_points

    def _cache_text_path(self, base_path, suffix):
        root, ext = os.path.splitext(base_path)
        return (root if ext else base_path) + suffix

    def _blob_bytes(self, painted_points):
        return bytes(
            int(value) & 0xFF for value in painted_points["cacheBlob"].getValue()
        )

    def _mapping_to_dict(self, value):
        return {key: value[key] for key in value.keys()}

    def _benchmark_brush_sample(self, u, v, seed):
        x = u - 0.5
        z = v - 0.5
        if u + v <= 1.0:
            triangle_index = 0
            barycentric = [1.0 - u - v, u, v]
        else:
            triangle_index = 1
            barycentric = [1.0 - v, u + v - 1.0, 1.0 - u]

        return {
            "P": [x, 0.01, z],
            "N": [0.0, 1.0, 0.0],
            "sourcePath": "/paintPlane",
            "triangleIndex": triangle_index,
            "barycentric": barycentric,
            "attachmentResolved": True,
            "width": 0.085,
            "scale": 1.0,
            "pressureDensity": 1.0,
            "pressureSoftness": 1.0,
            "seed": seed,
            "valid": True,
        }

    def _generate_benchmark_brush_samples(self, count):
        columns = int(max(1, round(math.sqrt(count))))
        rows = int((count + columns - 1) / columns)
        usable_columns = max(1, columns - 1)
        usable_rows = max(1, rows - 1)
        margin = 0.04

        samples = []
        for index in range(count):
            column = index % columns
            row = index // columns
            u = column / float(usable_columns) if usable_columns else 0.5
            v = row / float(usable_rows) if usable_rows else 0.5
            u = margin + (1.0 - margin * 2.0) * u
            v = margin + (1.0 - margin * 2.0) * v
            samples.append(self._benchmark_brush_sample(u, v, 5000 + index))

        return samples

    def _assert_brush_commit_benchmark(self, point_count, expected_chunk_count):
        _script, _plane, painted_points, _attached_points = self._build_empty_graph()

        layer_id = painted_points.ensureLayer("Benchmark Layer")
        stroke_id = painted_points.ensureStroke(
            layer_id, f"Benchmark Stroke {point_count}"
        )
        result = self._mapping_to_dict(
            painted_points.brushPaintCommit(
                stroke_id,
                self._generate_benchmark_brush_samples(point_count),
                append=True,
            )
        )
        snapshot = painted_points.cacheSnapshot()
        stroke = next(
            item
            for item in snapshot["strokes"]
            if int(item["strokeId"]) == int(stroke_id)
        )
        chunks = sorted(
            [
                chunk
                for chunk in snapshot["chunks"]
                if int(chunk["strokeId"]) == int(stroke_id)
            ],
            key=lambda item: item["chunkIndex"],
        )

        self.assertEqual(result["committed"], point_count)
        self.assertEqual(result["resolved"], point_count)
        self.assertEqual(result["fallback"], 0)
        self.assertEqual(result["inputSampleCount"], point_count)
        self.assertEqual(result["resolvedSampleCount"], point_count)
        self.assertEqual(len(snapshot["points"]), point_count)
        self.assertEqual(stroke["pointCount"], point_count)
        self.assertEqual(len(chunks), expected_chunk_count)
        self.assertIn(f"{point_count} points", painted_points.validateCache())
        self.assertEqual(painted_points["invalidPointCount"].getValue(), 0)
        self.assertEqual(painted_points["invalidStrokeCount"].getValue(), 0)
        self.assertEqual(painted_points["topologyMismatchCount"].getValue(), 0)

        for key in (
            "authoredBuildMs",
            "sampleCompileMs",
            "surfaceResolveMs",
            "recordBuildMs",
            "writeMs",
            "packPointsMs",
            "packFinalChecksumMs",
            "resultPackMs",
        ):
            self.assertIn(key, result)
            self.assertGreaterEqual(float(result[key]), 0.0)

    def _read_blob(self, path):
        with open(path, "rb") as handle:
            blob_bytes = handle.read()
        store, error = GafferScatterPaintStoreBridge._unpack_store_blob(blob_bytes)
        self.assertFalse(error)
        self.assertIsNotNone(store)
        return store

    def _write_blob(self, path, store):
        blob_bytes = GafferScatterPaintStoreBridge._pack_store_blob(store)
        with open(path, "wb") as handle:
            handle.write(blob_bytes)

    def _pack_store_blob(self, store):
        return GafferScatterPaintStoreBridge._pack_store_blob(store)

    def _write_lock(self, cache_path, lock_metadata):
        lock_path = cache_path + ".lock"
        with open(lock_path, "wb") as handle:
            handle.write(GafferScatterPaintCore._pack_lock_blob(lock_metadata))
        return lock_path

    def testExportActionsWriteExpectedFiles(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            script, _plane, painted_points, _attached_points = self._build_demo_graph()
            script_path = os.path.join(temp_dir, "painted_points_test.gfr")
            script["fileName"].setValue(script_path)

            authored_path = painted_points.exportAuthoredCache()
            interchange_path = painted_points.exportInterchange()
            diagnostics_path = painted_points.exportDiagnostics()

            self.assertEqual(
                authored_path,
                os.path.join(temp_dir, "PaintedPoints_authored.bin"),
            )
            self.assertEqual(
                interchange_path,
                os.path.join(temp_dir, "PaintedPoints_interchange.bin"),
            )
            self.assertEqual(
                diagnostics_path,
                os.path.join(temp_dir, "PaintedPoints_diagnostics.txt"),
            )

            self.assertTrue(os.path.exists(authored_path))
            self.assertTrue(os.path.exists(interchange_path))
            self.assertTrue(os.path.exists(diagnostics_path))

            authored_store = self._read_blob(authored_path)
            interchange_store = self._read_blob(interchange_path)

            self.assertEqual(len(authored_store["points"]), 2)
            self.assertEqual(len(interchange_store["points"]), 2)
            self.assertEqual(interchange_store["nextIds"], authored_store["nextIds"])
            self.assertEqual(interchange_store["chunks"], authored_store["chunks"])
            self.assertEqual(
                interchange_store["currentSelection"],
                authored_store["currentSelection"],
            )
            self.assertEqual(interchange_store["upgrades"], authored_store["upgrades"])
            self.assertEqual(
                interchange_store.get("pointBackups", {}),
                authored_store.get("pointBackups", {}),
            )
            self.assertIn("diagnostics", interchange_store)
            self.assertIn("validationSummary", interchange_store["diagnostics"])

            with open(diagnostics_path, "r", encoding="utf-8") as handle:
                diagnostics_text = handle.read()

            self.assertIn("validationSummary:", diagnostics_text)
            self.assertIn("validationCategories: diagnostics", diagnostics_text)
            self.assertIn("invalidPointCount: 0", diagnostics_text)
            self.assertIn("failingFrame: 0", diagnostics_text)

    def testBlobRoundTripPreservesPointBackups(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            _script, _plane, painted_points, _attached_points = self._build_demo_graph()
            cache_path = os.path.join(temp_dir, "point_backups.cache")

            store = painted_points.cacheSnapshot()
            store["pointBackups"] = {
                "101": {
                    "targetPathId": 3,
                    "triangleIndex": 7,
                    "barycentric": [0.2, 0.3, 0.5],
                    "attachmentResolved": True,
                },
                42: {
                    "targetPathId": 1,
                    "triangleIndex": 2,
                    "barycentric": [1.0, 0.0, 0.0],
                    "attachmentResolved": False,
                },
            }

            self._write_blob(cache_path, store)
            round_tripped_store = self._read_blob(cache_path)

            self.assertIn("pointBackups", round_tripped_store)
            self.assertEqual(
                list(round_tripped_store["pointBackups"].keys()), [42, 101]
            )
            self.assertEqual(
                round_tripped_store["pointBackups"][42],
                {
                    "targetPathId": 1,
                    "triangleIndex": 2,
                    "barycentric": [1.0, 0.0, 0.0],
                    "attachmentResolved": False,
                },
            )
            self.assertEqual(
                round_tripped_store["pointBackups"][101]["targetPathId"], 3
            )
            self.assertEqual(
                round_tripped_store["pointBackups"][101]["triangleIndex"], 7
            )
            self.assertTrue(
                round_tripped_store["pointBackups"][101]["attachmentResolved"]
            )
            for actual, expected in zip(
                round_tripped_store["pointBackups"][101]["barycentric"],
                [0.2, 0.3, 0.5],
            ):
                self.assertAlmostEqual(actual, expected, places=6)

    def testLockBlobRoundTripPreservesMetadata(self):
        lock_metadata = {
            "mode": 0,
            "user": "cache-user",
            "host": "cache-host",
            "timestampUtc": "2026-05-17T12:34:56Z",
            "scriptPath": "/tmp/scatter_paint_test.gfr",
            "projectPath": "/tmp",
            "sessionId": "cache-session",
        }

        round_tripped = GafferScatterPaintCore._unpack_lock_blob(
            GafferScatterPaintShared._pack_lock_blob(lock_metadata)
        )

        self.assertEqual(round_tripped, lock_metadata)

    def testLockBlobRejectsInvalidPayloads(self):
        lock_metadata = {
            "mode": 0,
            "user": "cache-user",
            "host": "cache-host",
            "timestampUtc": "2026-05-17T12:34:56Z",
            "scriptPath": "/tmp/scatter_paint_test.gfr",
            "projectPath": "/tmp",
            "sessionId": "cache-session",
        }

        valid_blob = GafferScatterPaintShared._pack_lock_blob(lock_metadata)

        with self.assertRaisesRegex(
            ValueError, "Invalid scatter paint lock: bad magic"
        ):
            GafferScatterPaintCore._unpack_lock_blob(b"BAD!" + valid_blob[4:])

        corrupted_blob = bytearray(valid_blob)
        corrupted_blob[-1] ^= 0xFF
        with self.assertRaisesRegex(
            ValueError, "Invalid scatter paint lock: checksum mismatch"
        ):
            GafferScatterPaintCore._unpack_lock_blob(bytes(corrupted_blob))

    def testCacheModeMigrationPreservesStoreAcrossEmbeddedAndExternalModes(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            script, _plane, painted_points, _attached_points = self._build_demo_graph()
            script_path = os.path.join(temp_dir, "painted_points_test.gfr")
            cache_path = os.path.join(temp_dir, "painted_points.cache")
            script["fileName"].setValue(script_path)
            painted_points["cachePath"].setValue(cache_path)

            original_store = painted_points.cacheSnapshot()

            painted_points.migrateCacheMode()
            self.assertEqual(painted_points["cacheMode"].getValue(), 1)
            self.assertTrue(os.path.exists(cache_path))
            self.assertEqual(painted_points["cacheResolvedPath"].getValue(), cache_path)

            external_store = self._read_blob(cache_path)
            self.assertEqual(
                external_store["pointBackups"], original_store["pointBackups"]
            )
            self.assertEqual(external_store["layers"], original_store["layers"])
            self.assertEqual(external_store["strokes"], original_store["strokes"])
            self.assertEqual(external_store["points"], original_store["points"])
            self.assertEqual(
                external_store["selectionSets"], original_store["selectionSets"]
            )
            self.assertEqual(
                external_store["currentSelection"], original_store["currentSelection"]
            )

            painted_points.migrateCacheMode()
            self.assertEqual(painted_points["cacheMode"].getValue(), 0)
            self.assertEqual(painted_points["cacheResolvedPath"].getValue(), "")

            round_tripped_store = painted_points.cacheSnapshot()
            self.assertEqual(
                round_tripped_store["pointBackups"], original_store["pointBackups"]
            )
            self.assertEqual(round_tripped_store["layers"], original_store["layers"])
            self.assertEqual(round_tripped_store["strokes"], original_store["strokes"])
            self.assertEqual(round_tripped_store["points"], original_store["points"])
            self.assertEqual(
                round_tripped_store["selectionSets"], original_store["selectionSets"]
            )
            self.assertEqual(
                round_tripped_store["currentSelection"],
                original_store["currentSelection"],
            )

    def testContentFlagsMatchDiagnosticsPayload(self):
        _script, _plane, painted_points, _attached_points = self._build_demo_graph()

        store = painted_points.cacheSnapshot()
        unpacked_store, error = GafferScatterPaintStoreBridge._unpack_store_blob(
            GafferScatterPaintStoreBridge._pack_store_blob(store)
        )
        self.assertFalse(error)
        self.assertIsNotNone(unpacked_store)
        self.assertEqual(unpacked_store["node"].get("contentFlags", 0), 0)

        store["diagnostics"]["invalidPointCount"] = 1
        unpacked_store, error = GafferScatterPaintStoreBridge._unpack_store_blob(
            GafferScatterPaintStoreBridge._pack_store_blob(store)
        )
        self.assertFalse(error)
        self.assertIsNotNone(unpacked_store)
        self.assertEqual(
            unpacked_store["node"]["contentFlags"],
            GafferScatterPaintCore.CONTENT_FLAG_DIAGNOSTICS,
        )

    def testExportEvaluatedPointsWritesAttachedPointsGeometry(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            script, _plane, painted_points, attached_points = self._build_demo_graph()
            script_path = os.path.join(temp_dir, "painted_points_test.gfr")
            script["fileName"].setValue(script_path)

            attached_points["outputLocation"].setValue("/paintPlane/finalScatter")
            attached_points["pointType"].setValue("marker")

            export_path = painted_points.exportEvaluatedPoints()

            self.assertEqual(
                export_path,
                os.path.join(temp_dir, "PaintedPoints_evaluated.cob"),
            )
            self.assertTrue(os.path.exists(export_path))

            expected_object = attached_points["out"].object("/paintPlane/finalScatter")
            self.assertIsInstance(expected_object, IECoreScene.PointsPrimitive)

            reader = IECore.Reader.create(export_path)
            self.assertIsNotNone(reader)
            exported_object = reader.read()

            self.assertIsInstance(exported_object, IECoreScene.PointsPrimitive)
            self.assertEqual(exported_object.numPoints, expected_object.numPoints)
            self.assertEqual(exported_object["type"].data.value, "marker")
            self.assertEqual(
                list(exported_object["id"].data), list(expected_object["id"].data)
            )
            self.assertEqual(
                list(exported_object["sourcePath"].data),
                list(expected_object["sourcePath"].data),
            )

    def testExportEvaluatedPointsRequiresAttachedPointsNode(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            script, _plane, painted_points, attached_points = self._build_demo_graph()
            script["fileName"].setValue(
                os.path.join(temp_dir, "painted_points_test.gfr")
            )
            attached_points["points"].setInput(None)

            with self.assertRaisesRegex(
                RuntimeError,
                "Unable to export evaluated points: no AttachedPoints node is connected to this PaintedPoints output",
            ):
                painted_points.exportEvaluatedPoints()

    def testGeometryExportsWriteReadableSceneFiles(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            script, _plane, painted_points, attached_points = self._build_demo_graph()
            script_path = os.path.join(temp_dir, "painted_points_test.gfr")
            script["fileName"].setValue(script_path)

            attached_points["outputLocation"].setValue("/paintPlane/finalScatter")
            attached_points["pointType"].setValue("marker")
            expected_object = attached_points["out"].object("/paintPlane/finalScatter")

            scene_path = painted_points.exportGafferScene()
            usd_path = painted_points.exportUSD()
            abc_path = painted_points.exportAlembic()

            self.assertEqual(
                scene_path, os.path.join(temp_dir, "PaintedPoints_scene.scc")
            )
            self.assertEqual(
                usd_path, os.path.join(temp_dir, "PaintedPoints_scene.usda")
            )
            self.assertEqual(
                abc_path, os.path.join(temp_dir, "PaintedPoints_scene.abc")
            )

            for path in (scene_path, usd_path, abc_path):
                self.assertTrue(os.path.exists(path))
                reader = GafferScene.SceneReader()
                reader["fileName"].setValue(path)
                exported_object = reader["out"].object("/paintPlane/finalScatter")
                self.assertIsInstance(exported_object, IECoreScene.PointsPrimitive)
                self.assertEqual(exported_object.numPoints, expected_object.numPoints)
                self.assertEqual(
                    [tuple(value) for value in exported_object["P"].data],
                    [tuple(value) for value in expected_object["P"].data],
                )
                if path.endswith(".scc"):
                    self.assertEqual(exported_object["type"].data.value, "marker")
                    self.assertEqual(list(exported_object["id"].data), [1, 2])
                elif path.endswith(".usda"):
                    self.assertEqual(list(exported_object["id"].data), [1, 2])

    def testGeometryExportsRequireAttachedPointsNode(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            script, _plane, painted_points, attached_points = self._build_demo_graph()
            script["fileName"].setValue(
                os.path.join(temp_dir, "painted_points_test.gfr")
            )
            attached_points["points"].setInput(None)

            for export_method in (
                painted_points.exportGafferScene,
                painted_points.exportUSD,
                painted_points.exportAlembic,
            ):
                with self.assertRaisesRegex(
                    RuntimeError,
                    "Unable to export evaluated scene: no AttachedPoints node is connected to this PaintedPoints output",
                ):
                    export_method()

    def testMigrateCacheModeAndRelinkExternalCache(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            script, _plane, painted_points, _attached_points = self._build_demo_graph()
            script_path = os.path.join(temp_dir, "painted_points_test.gfr")
            cache_path = os.path.join(temp_dir, "painted_points.cache")
            script["fileName"].setValue(script_path)
            painted_points["cachePath"].setValue(cache_path)

            self.assertGreater(len(self._blob_bytes(painted_points)), 0)

            version = painted_points.migrateCacheMode()
            self.assertEqual(version, 2)
            self.assertEqual(painted_points["cacheMode"].getValue(), 1)
            self.assertTrue(os.path.exists(cache_path))
            self.assertEqual(len(self._blob_bytes(painted_points)), 0)
            self.assertEqual(
                os.path.normpath(painted_points["cacheResolvedPath"].getValue()),
                os.path.normpath(cache_path),
            )
            self.assertTrue(painted_points["cacheLockedBy"].getValue())
            self.assertTrue(painted_points["cacheLockedHost"].getValue())
            self.assertTrue(painted_points["cacheLockedTime"].getValue())
            self.assertEqual(
                painted_points["cacheLockedScript"].getValue(), script_path
            )
            self.assertFalse(os.path.exists(cache_path + ".lock"))

            relinked_path = painted_points.relinkCache()
            self.assertEqual(
                os.path.normpath(relinked_path), os.path.normpath(cache_path)
            )

            version = painted_points.migrateCacheMode()
            self.assertEqual(version, 2)
            self.assertEqual(painted_points["cacheMode"].getValue(), 0)
            self.assertGreater(len(self._blob_bytes(painted_points)), 0)

    def testMigrateCacheModeRequiresPathForExternalMode(self):
        _script, _plane, painted_points, _attached_points = self._build_demo_graph()

        with self.assertRaisesRegex(
            RuntimeError,
            "External cache mode requires a cache path",
        ):
            painted_points.migrateCacheMode()

    def testRelinkCacheRequiresExternalMode(self):
        _script, _plane, painted_points, _attached_points = self._build_demo_graph()

        with self.assertRaisesRegex(
            RuntimeError,
            "Relink is only valid in external cache mode",
        ):
            painted_points.relinkCache()

    def testRelinkCacheRejectsMissingExternalFile(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            script, _plane, painted_points, _attached_points = self._build_demo_graph()
            script["fileName"].setValue(
                os.path.join(temp_dir, "painted_points_test.gfr")
            )
            cache_path = os.path.join(temp_dir, "painted_points.cache")
            painted_points["cachePath"].setValue(cache_path)

            painted_points.migrateCacheMode()
            os.remove(cache_path)

            with self.assertRaisesRegex(
                RuntimeError,
                "External cache does not exist:",
            ):
                painted_points.relinkCache()

    def testExternalWritesCreateBackupWhenEnabled(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            script, _plane, painted_points, _attached_points = self._build_demo_graph()
            script["fileName"].setValue(
                os.path.join(temp_dir, "painted_points_test.gfr")
            )
            cache_path = os.path.join(temp_dir, "painted_points.cache")
            painted_points["cachePath"].setValue(cache_path)
            painted_points["backupEnabled"].setValue(True)
            painted_points["backupPolicy"].setValue(1)

            painted_points.migrateCacheMode()

            with open(cache_path, "rb") as handle:
                original_bytes = handle.read()

            painted_points.createLayer("Backup Layer")

            backup_path = self._cache_text_path(cache_path, ".bak")
            self.assertTrue(os.path.exists(backup_path))
            with open(backup_path, "rb") as handle:
                backup_bytes = handle.read()
            self.assertEqual(backup_bytes, original_bytes)
            self.assertFalse(os.path.exists(cache_path + ".lock"))

    def testExternalWriteRejectsForeignActiveLock(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            script, _plane, painted_points, _attached_points = self._build_demo_graph()
            script_path = os.path.join(temp_dir, "painted_points_test.gfr")
            cache_path = os.path.join(temp_dir, "painted_points.cache")
            script["fileName"].setValue(script_path)
            painted_points["cachePath"].setValue(cache_path)

            painted_points.migrateCacheMode()
            lock_path = self._write_lock(
                cache_path,
                {
                    "mode": 0,
                    "user": "foreign-user",
                    "host": "foreign-host",
                    "timestampUtc": GafferScatterPaintCore._utc_timestamp(),
                    "scriptPath": "/tmp/foreign_script.gfr",
                    "projectPath": "/tmp",
                    "sessionId": "foreign-session",
                },
            )

            with self.assertRaisesRegex(
                RuntimeError,
                "External cache is locked by foreign-user@foreign-host for /tmp/foreign_script.gfr",
            ):
                painted_points.createLayer("Blocked Layer")

            self.assertTrue(os.path.exists(lock_path))

    def testExternalWriteIgnoresStaleLock(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            script, _plane, painted_points, _attached_points = self._build_demo_graph()
            script_path = os.path.join(temp_dir, "painted_points_test.gfr")
            cache_path = os.path.join(temp_dir, "painted_points.cache")
            script["fileName"].setValue(script_path)
            painted_points["cachePath"].setValue(cache_path)

            painted_points.migrateCacheMode()
            lock_path = self._write_lock(
                cache_path,
                {
                    "mode": 0,
                    "user": "stale-user",
                    "host": "stale-host",
                    "timestampUtc": "2000-01-01T00:00:00Z",
                    "scriptPath": "/tmp/stale_script.gfr",
                    "projectPath": "/tmp",
                    "sessionId": "stale-session",
                },
            )

            layer_id = painted_points.createLayer("Recovered Layer")
            self.assertGreater(layer_id, 0)
            self.assertFalse(os.path.exists(lock_path))

            store = self._read_blob(cache_path)
            self.assertEqual(len(store["layers"]), 2)

    def testUpgradeCacheReturnsCurrentSchemaWithoutUpgradeFile(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            script, _plane, painted_points, _attached_points = self._build_demo_graph()
            script["fileName"].setValue(
                os.path.join(temp_dir, "painted_points_test.gfr")
            )
            cache_path = os.path.join(temp_dir, "painted_points.cache")
            painted_points["cachePath"].setValue(cache_path)

            painted_points.migrateCacheMode()
            result = painted_points.upgradeCache()

            self.assertEqual(result, 2)
            self.assertFalse(
                os.path.exists(self._cache_text_path(cache_path, ".v2.upgrade"))
            )

    def testUpgradeCacheRejectsUnsupportedOlderSchemaBlob(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            script, _plane, painted_points, _attached_points = self._build_demo_graph()
            script["fileName"].setValue(
                os.path.join(temp_dir, "painted_points_test.gfr")
            )
            cache_path = os.path.join(temp_dir, "painted_points.cache")
            painted_points["cachePath"].setValue(cache_path)

            painted_points.migrateCacheMode()

            original_store = self._read_blob(cache_path)
            original_store["schemaVersion"] = 0
            self._write_blob(cache_path, original_store)

            expected_path = self._cache_text_path(cache_path, ".v2.upgrade")

            with self.assertRaisesRegex(
                RuntimeError,
                "Invalid scatter paint blob: unsupported schema version 0",
            ):
                painted_points.upgradeCache()

            self.assertFalse(os.path.exists(expected_path))
            self.assertFalse(os.path.exists(cache_path + ".lock"))
            self.assertEqual(painted_points["cachePath"].getValue(), cache_path)

    def testFreezeBakeCreatesStaticPointsSibling(self):
        script, _plane, painted_points, _attached_points = self._build_demo_graph()

        baked_name = painted_points.freezeBakeToStaticNode()

        self.assertEqual(baked_name, "PaintedPointsStaticBake")
        self.assertIn(baked_name, script)

        baked_node = script[baked_name]
        self.assertIsInstance(baked_node, GafferScatterPaint.StaticPoints)
        self.assertEqual(baked_node["outputLocation"].getValue(), "/scatterBake")

        point_data = baked_node["pointData"].getValue()
        self.assertIsInstance(point_data, IECore.CompoundObject)
        self.assertEqual(point_data["pointCount"].value, 2)

        baked_object = baked_node["out"].object("/scatterBake")
        self.assertIsInstance(baked_object, IECoreScene.PointsPrimitive)
        self.assertEqual(baked_object.numPoints, 2)
        self.assertEqual(list(baked_object["id"].data), [1, 2])
        self.assertEqual(list(baked_object["strokeId"].data), [1, 1])
        self.assertEqual(
            list(baked_object["sourcePath"].data), ["/paintPlane", "/paintPlane"]
        )

    def testFreezeBakeSelectionCreatesStaticPointsFromSelectedPoints(self):
        script, _plane, painted_points, _attached_points = self._build_demo_graph()

        painted_points.setCurrentSelection([2], [])
        baked_name = painted_points.freezeBakeSelection()

        self.assertEqual(baked_name, "PaintedPointsSelectionStaticBake")
        self.assertIn(baked_name, script)

        baked_node = script[baked_name]
        self.assertIsInstance(baked_node, GafferScatterPaint.StaticPoints)
        self.assertEqual(
            baked_node["outputLocation"].getValue(), "/scatterBakeSelection"
        )

        point_data = baked_node["pointData"].getValue()
        self.assertIsInstance(point_data, IECore.CompoundObject)
        self.assertEqual(point_data["pointCount"].value, 1)

        baked_object = baked_node["out"].object("/scatterBakeSelection")
        self.assertIsInstance(baked_object, IECoreScene.PointsPrimitive)
        self.assertEqual(baked_object.numPoints, 1)
        self.assertEqual(list(baked_object["id"].data), [2])
        self.assertEqual(list(baked_object["strokeId"].data), [1])
        self.assertEqual(list(baked_object["sourcePath"].data), ["/paintPlane"])

    def testFreezeBakeSelectionIncludesSelectedStrokePoints(self):
        script, _plane, painted_points, _attached_points = self._build_demo_graph()

        painted_points.setCurrentSelection([], [1])
        baked_name = painted_points.freezeBakeSelection()

        self.assertEqual(baked_name, "PaintedPointsSelectionStaticBake")
        self.assertIn(baked_name, script)

        baked_object = script[baked_name]["out"].object("/scatterBakeSelection")
        self.assertIsInstance(baked_object, IECoreScene.PointsPrimitive)
        self.assertEqual(baked_object.numPoints, 2)
        self.assertEqual(list(baked_object["id"].data), [1, 2])

    def testFreezeBakeSelectionRequiresCurrentSelection(self):
        _script, _plane, painted_points, _attached_points = self._build_demo_graph()

        with self.assertRaisesRegex(
            RuntimeError,
            "Unable to bake selection: no points are currently selected",
        ):
            painted_points.freezeBakeSelection()

    def testFreezeBakeSelectionRangeStoresFrameSamples(self):
        script, _plane, painted_points, _attached_points = self._build_demo_graph()

        hidden_layer_id = painted_points.createLayer("Hidden Range")
        hidden_stroke_id = painted_points.createStroke(hidden_layer_id, "Hidden Stroke")
        painted_points.paintStrokeCommit(
            hidden_stroke_id,
            [
                {
                    "P": [0.6, 0.0, 0.6],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 21,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                }
            ],
            append=True,
        )
        painted_points.setLayerTimeRange(hidden_layer_id, 10, 20)
        painted_points.setCurrentSelection([], [hidden_stroke_id])

        baked_name = painted_points.freezeBakeSelection(10, 12)
        baked_node = script[baked_name]
        point_data = baked_node["pointData"].getValue()

        self.assertIsInstance(point_data, IECore.CompoundObject)
        self.assertEqual(list(point_data["frameNumbers"]), [10, 11, 12])

        context = script.context()
        context.setFrame(9)
        with context:
            before_range = baked_node["out"].object("/scatterBakeSelection")
        context.setFrame(12)
        with context:
            inside_range = baked_node["out"].object("/scatterBakeSelection")
        context.setFrame(30)
        with context:
            after_range = baked_node["out"].object("/scatterBakeSelection")

        self.assertEqual(before_range.numPoints, 1)
        self.assertEqual(inside_range.numPoints, 1)
        self.assertEqual(after_range.numPoints, 1)
        self.assertEqual(list(before_range["id"].data), list(inside_range["id"].data))
        self.assertEqual(list(after_range["id"].data), list(inside_range["id"].data))

    def testFreezeBakeUsesUniqueStaticNodeNames(self):
        script, _plane, painted_points, _attached_points = self._build_demo_graph()

        first_name = painted_points.freezeBakeToStaticNode()
        second_name = painted_points.freezeBakeToStaticNode()

        self.assertEqual(first_name, "PaintedPointsStaticBake")
        self.assertEqual(second_name, "PaintedPointsStaticBake2")
        self.assertIn(first_name, script)
        self.assertIn(second_name, script)

    def testFreezeBakeRangeStoresFrameSamples(self):
        script, _plane, painted_points, _attached_points = self._build_demo_graph()

        range_layer_id = painted_points.createLayer("Range Layer")
        range_stroke_id = painted_points.createStroke(range_layer_id, "Range Stroke")
        painted_points.paintStrokeCommit(
            range_stroke_id,
            [
                {
                    "P": [0.75, 0.0, 0.25],
                    "width": 0.15,
                    "scale": 1.1,
                    "seed": 12,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 1,
                    "barycentric": [0.25, 0.5, 0.25],
                    "attachmentResolved": True,
                }
            ],
            append=True,
        )
        painted_points.setLayerTimeRange(range_layer_id, 11, 11)

        baked_name = painted_points.freezeBakeToStaticNode(10, 12)
        baked_node = script[baked_name]
        point_data = baked_node["pointData"].getValue()

        self.assertIsInstance(point_data, IECore.CompoundObject)
        self.assertEqual(list(point_data["frameNumbers"]), [10, 11, 12])

        context = script.context()
        context.setFrame(10)
        with context:
            frame10 = baked_node["out"].object("/scatterBake")
        context.setFrame(11)
        with context:
            frame11 = baked_node["out"].object("/scatterBake")
        context.setFrame(12)
        with context:
            frame12 = baked_node["out"].object("/scatterBake")
        context.setFrame(99)
        with context:
            held = baked_node["out"].object("/scatterBake")

        self.assertEqual(frame10.numPoints, 3)
        self.assertEqual(frame11.numPoints, 3)
        self.assertEqual(frame12.numPoints, 3)
        self.assertEqual(held.numPoints, 3)
        self.assertEqual(list(frame10["id"].data), [1, 2, 3])
        self.assertEqual(list(frame11["id"].data), [1, 2, 3])
        self.assertEqual(list(held["id"].data), list(frame12["id"].data))

    def testFreezeBakeEvaluatedCreatesStaticPointsSibling(self):
        script, _plane, painted_points, attached_points = self._build_demo_graph()

        attached_points["outputLocation"].setValue("/paintPlane/finalScatter")
        attached_points["pointType"].setValue("marker")

        baked_name = painted_points.freezeBakeEvaluatedToStaticNode()

        self.assertEqual(baked_name, "PaintedPointsEvaluatedStaticBake")
        self.assertIn(baked_name, script)

        baked_node = script[baked_name]
        self.assertIsInstance(baked_node, GafferScatterPaint.StaticPoints)
        self.assertEqual(baked_node["outputLocation"].getValue(), "/scatterBake")
        self.assertEqual(baked_node["pointType"].getValue(), "marker")

        point_data = baked_node["pointData"].getValue()
        self.assertIsInstance(point_data, IECore.CompoundObject)
        self.assertEqual(point_data["pointCount"].value, 2)

        expected_object = attached_points["out"].object("/paintPlane/finalScatter")
        self.assertIsInstance(expected_object, IECoreScene.PointsPrimitive)

        baked_object = baked_node["out"].object("/scatterBake")
        self.assertIsInstance(baked_object, IECoreScene.PointsPrimitive)
        self.assertEqual(baked_object.numPoints, expected_object.numPoints)
        self.assertEqual(baked_object["type"].data.value, "marker")
        self.assertEqual(
            list(baked_object["id"].data), list(expected_object["id"].data)
        )
        self.assertEqual(
            list(baked_object["sourcePath"].data),
            list(expected_object["sourcePath"].data),
        )

    def testFreezeBakeEvaluatedRequiresAttachedPointsNode(self):
        script, _plane, painted_points, attached_points = self._build_demo_graph()
        attached_points["points"].setInput(None)

        with self.assertRaisesRegex(
            RuntimeError,
            "Unable to export evaluated points: no AttachedPoints node is connected to this PaintedPoints output",
        ):
            painted_points.freezeBakeEvaluatedToStaticNode()

    def testFreezeBakeEvaluatedRangeStoresFrameSamples(self):
        script, _plane, painted_points, attached_points = self._build_demo_graph()

        attached_points["outputLocation"].setValue("/paintPlane/finalScatter")

        plane_filter = GafferScene.PathFilter("PlaneFilter")
        script.addChild(plane_filter)
        plane_filter["paths"].setValue(IECore.StringVectorData(["/paintPlane"]))

        transform = GafferScene.Transform("PlaneTransform")
        script.addChild(transform)
        transform["in"].setInput(script["ScatterPaintPlane"]["out"])
        transform["filter"].setInput(plane_filter["out"])

        painted_points["in"].setInput(transform["out"])
        attached_points["in"].setInput(transform["out"])

        expression = Gaffer.Expression("ScatterExpression")
        script.addChild(expression)
        expression.setExpression(
            'parent["PlaneTransform"]["transform"]["translate"]["x"] = context.getFrame()',
            "python",
        )

        baked_name = painted_points.freezeBakeEvaluatedToStaticNode(5, 7)
        baked_node = script[baked_name]
        point_data = baked_node["pointData"].getValue()

        self.assertIsInstance(point_data, IECore.CompoundObject)
        self.assertEqual(list(point_data["frameNumbers"]), [5, 6, 7])

        context = script.context()
        context.setFrame(5)
        with context:
            frame5 = baked_node["out"].object("/scatterBake")
            frame5_positions = list(frame5["P"].data)
        context.setFrame(7)
        with context:
            frame7 = baked_node["out"].object("/scatterBake")
            frame7_positions = list(frame7["P"].data)
        context.setFrame(20)
        with context:
            held = baked_node["out"].object("/scatterBake")
            held_positions = list(held["P"].data)

        self.assertEqual(frame5.numPoints, 2)
        self.assertEqual(frame7.numPoints, 2)
        self.assertEqual(held.numPoints, 2)
        self.assertEqual(list(frame5["id"].data), [1, 2])
        self.assertEqual(list(frame7["id"].data), [1, 2])
        self.assertEqual(list(held["id"].data), list(frame7["id"].data))

        self.assertNotEqual(frame5_positions, frame7_positions)
        self.assertEqual(held_positions, frame7_positions)
        self.assertEqual(frame5_positions[0].x, 5.0)
        self.assertEqual(frame5_positions[1].x, 5.0)
        self.assertEqual(frame7_positions[0].x, 7.0)
        self.assertEqual(frame7_positions[1].x, 7.0)

    def testStaticPointsOutputLocationChangesLeafPath(self):
        static_points = GafferScatterPaint.StaticPoints("StaticPoints")

        self.assertEqual(static_points["name"].getValue(), "scatter")

        static_points["outputLocation"].setValue("/scatterBake")
        self.assertEqual(static_points["name"].getValue(), "scatterBake")

        static_points["outputLocation"].setValue("/nested/finalPoints")
        self.assertEqual(static_points["name"].getValue(), "finalPoints")

    def testLayerEditingOperationsUpdateStoreAndPoints(self):
        _script, _plane, painted_points, _attached_points = self._build_demo_graph()

        fx_layer_id = painted_points.createLayer("FX")
        detail_layer_id = painted_points.createLayer("Detail")
        painted_points.moveLayer(fx_layer_id, 0)
        painted_points.renameLayer(detail_layer_id, "Detail Renamed")

        detail_stroke_id = painted_points.createStroke(detail_layer_id, "Detail Stroke")
        painted_points.paintStrokeCommit(
            detail_stroke_id,
            [
                {
                    "P": [0.4, 0.0, 0.1],
                    "width": 0.2,
                    "scale": 1.25,
                    "seed": 9,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.2, 0.4, 0.4],
                    "attachmentResolved": True,
                }
            ],
            append=True,
        )

        merged_layer_id = painted_points.mergeLayers(detail_layer_id, fx_layer_id)
        self.assertEqual(merged_layer_id, fx_layer_id)

        layers = painted_points.layerRecords()
        self.assertEqual([layer["name"] for layer in layers], ["FX", "Demo Layer"])
        self.assertEqual([layer["order"] for layer in layers], [0, 1])

        strokes = painted_points.strokeRecords()
        fx_strokes = [stroke for stroke in strokes if stroke["layerId"] == fx_layer_id]
        self.assertEqual([stroke["name"] for stroke in fx_strokes], ["Detail Stroke"])
        self.assertEqual(fx_strokes[0]["order"], 0)

        point_records = painted_points.pointRecords()
        detail_points = [
            point for point in point_records if point["strokeId"] == detail_stroke_id
        ]
        self.assertEqual(len(detail_points), 1)
        self.assertEqual(detail_points[0]["layerId"], fx_layer_id)

        deleted_layer_id = painted_points.deleteLayer(fx_layer_id)
        self.assertEqual(deleted_layer_id, fx_layer_id)
        self.assertEqual(
            [layer["name"] for layer in painted_points.layerRecords()], ["Demo Layer"]
        )
        self.assertEqual(
            [stroke["name"] for stroke in painted_points.strokeRecords()],
            ["Demo Stroke"],
        )
        self.assertEqual(len(painted_points.pointRecords()), 2)

    def testStrokeEditingOperationsUpdateOrderingAndMergedPoints(self):
        _script, _plane, painted_points, _attached_points = self._build_demo_graph()

        demo_layer_id = painted_points.layerRecords()[0]["layerId"]
        fill_stroke_id = painted_points.createStroke(demo_layer_id, "Fill")
        accent_stroke_id = painted_points.createStroke(demo_layer_id, "Accent")
        painted_points.paintStrokeCommit(
            fill_stroke_id,
            [
                {
                    "P": [0.5, 0.0, 0.0],
                    "width": 0.15,
                    "scale": 0.75,
                    "seed": 7,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.4, 0.3, 0.3],
                    "attachmentResolved": True,
                }
            ],
            append=True,
        )
        painted_points.paintStrokeCommit(
            accent_stroke_id,
            [
                {
                    "P": [0.6, 0.0, 0.1],
                    "width": 0.2,
                    "scale": 1.5,
                    "seed": 8,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 1,
                    "barycentric": [0.2, 0.2, 0.6],
                    "attachmentResolved": True,
                }
            ],
            append=True,
        )

        painted_points.renameStroke(fill_stroke_id, "Fill Renamed")
        painted_points.moveStroke(accent_stroke_id, 0)

        strokes = painted_points.strokeRecords()
        demo_strokes = [
            stroke for stroke in strokes if stroke["layerId"] == demo_layer_id
        ]
        self.assertEqual(
            [stroke["name"] for stroke in demo_strokes],
            ["Demo Stroke", "Fill Renamed", "Accent"],
        )
        self.assertEqual([stroke["order"] for stroke in demo_strokes], [1, 2, 0])

        merged_stroke_id = painted_points.mergeStrokes(fill_stroke_id, accent_stroke_id)
        self.assertEqual(merged_stroke_id, accent_stroke_id)

        strokes = painted_points.strokeRecords()
        stroke_names = [
            stroke["name"] for stroke in strokes if stroke["layerId"] == demo_layer_id
        ]
        self.assertEqual(stroke_names, ["Demo Stroke", "Accent"])

        point_records = painted_points.pointRecords()
        merged_points = [
            point for point in point_records if point["strokeId"] == accent_stroke_id
        ]
        self.assertEqual(len(merged_points), 2)
        self.assertTrue(
            all(point["layerId"] == demo_layer_id for point in merged_points)
        )

        deleted_stroke_id = painted_points.deleteStroke(accent_stroke_id)
        self.assertEqual(deleted_stroke_id, accent_stroke_id)
        remaining_strokes = painted_points.strokeRecords()
        self.assertEqual(
            [stroke["name"] for stroke in remaining_strokes], ["Demo Stroke"]
        )
        self.assertEqual(len(painted_points.pointRecords()), 2)

    def testSelectionStatePersistsInCacheSnapshot(self):
        _script, _plane, painted_points, _attached_points = self._build_demo_graph()

        point_records = painted_points.pointRecords()
        point_ids = [point_records[0]["pointId"]]
        stroke_ids = [point_records[0]["strokeId"]]

        current_selection = painted_points.setCurrentSelection(point_ids, stroke_ids)
        stored_selection = painted_points.storeCurrentSelection(name="Pinned Selection")
        snapshot = painted_points.cacheSnapshot()

        self.assertEqual(current_selection["pointIds"], point_ids)
        self.assertEqual(current_selection["strokeIds"], stroke_ids)
        self.assertEqual(snapshot["currentSelection"]["pointIds"], point_ids)
        self.assertEqual(snapshot["currentSelection"]["strokeIds"], stroke_ids)
        self.assertEqual(len(snapshot["selectionSets"]), 1)
        self.assertEqual(
            snapshot["selectionSets"][0]["selectionSetId"],
            stored_selection["selectionSetId"],
        )
        self.assertEqual(snapshot["selectionSets"][0]["name"], "Pinned Selection")
        self.assertEqual(snapshot["selectionSets"][0]["pointIds"], point_ids)
        self.assertEqual(snapshot["selectionSets"][0]["strokeIds"], stroke_ids)

    def testLayerStateActionsPersistVisibleMuteSoloAndTimeRange(self):
        _script, _plane, painted_points, _attached_points = self._build_demo_graph()

        demo_layer = painted_points.layerRecords()[0]
        layer_id = demo_layer["layerId"]

        self.assertEqual(painted_points.setLayerVisible(layer_id, False), layer_id)
        self.assertEqual(painted_points.setLayerMute(layer_id, True), layer_id)
        self.assertEqual(painted_points.setLayerSolo(layer_id, True), layer_id)
        self.assertEqual(
            painted_points.setLayerTimeRange(layer_id, 1001, 1010), layer_id
        )

        updated_layer = next(
            layer
            for layer in painted_points.layerRecords()
            if int(layer["layerId"]) == int(layer_id)
        )
        self.assertFalse(updated_layer["visible"])
        self.assertTrue(updated_layer["mute"])
        self.assertTrue(updated_layer["solo"])
        self.assertTrue(updated_layer["timeVarying"])
        self.assertEqual(updated_layer["frameStart"], 1001)
        self.assertEqual(updated_layer["frameEnd"], 1010)
        self.assertEqual(len(painted_points.pointRecords()), 2)

        painted_points.setLayerTimeRange(layer_id, 0, 0)
        reset_layer = next(
            layer
            for layer in painted_points.layerRecords()
            if int(layer["layerId"]) == int(layer_id)
        )
        self.assertFalse(reset_layer["timeVarying"])
        self.assertEqual(reset_layer["frameStart"], 0)
        self.assertEqual(reset_layer["frameEnd"], 0)

    def testSelectionSetRenameAndDeleteUpdateSnapshot(self):
        _script, _plane, painted_points, _attached_points = self._build_demo_graph()

        point_records = painted_points.pointRecords()
        point_ids = [point_records[0]["pointId"]]
        stroke_ids = [point_records[0]["strokeId"]]
        painted_points.setCurrentSelection(point_ids, stroke_ids)
        stored_selection = painted_points.storeCurrentSelection(name="Pinned Selection")

        selection_set_id = stored_selection["selectionSetId"]
        self.assertEqual(
            painted_points.renameSelectionSet(
                selection_set_id, "Pinned Selection Renamed"
            ),
            selection_set_id,
        )

        renamed_snapshot = painted_points.cacheSnapshot()
        self.assertEqual(len(renamed_snapshot["selectionSets"]), 1)
        self.assertEqual(
            renamed_snapshot["selectionSets"][0]["name"], "Pinned Selection Renamed"
        )
        self.assertEqual(renamed_snapshot["selectionSets"][0]["pointIds"], point_ids)
        self.assertEqual(renamed_snapshot["selectionSets"][0]["strokeIds"], stroke_ids)

        self.assertEqual(
            painted_points.deleteSelectionSet(selection_set_id), selection_set_id
        )
        deleted_snapshot = painted_points.cacheSnapshot()
        self.assertEqual(deleted_snapshot["selectionSets"], [])
        self.assertEqual(deleted_snapshot["currentSelection"]["pointIds"], point_ids)
        self.assertEqual(deleted_snapshot["currentSelection"]["strokeIds"], stroke_ids)

    def testValidateAttachmentsReportsUnresolvedFallbacks(self):
        _script, _plane, painted_points, _attached_points = self._build_demo_graph()

        self.assertEqual(
            painted_points.validateAttachments(),
            "2 authored points, 0 unresolved attachment fallbacks",
        )

        point_id = painted_points.pointRecords()[0]["pointId"]

        def mutate(store):
            point = next(
                item
                for item in store["points"]
                if int(item.get("pointId", 0)) == int(point_id)
            )
            point["anchorModeUsed"] = 3
            point["valid"] = False

        painted_points.mutateCacheStore(mutate)

        self.assertEqual(
            painted_points.validateAttachments(),
            "2 authored points, 1 unresolved attachment fallbacks",
        )

    def testValidateCacheReportsDetailedDiagnosticsForCorruptStore(self):
        _script, _plane, painted_points, _attached_points = self._build_demo_graph()

        def mutate(store):
            point = store["points"][0]
            point["targetPathId"] = 999
            point["barycentric"] = [0.5, 0.25]
            store["currentSelection"]["pointIds"] = [99999]
            store["currentSelection"]["strokeIds"] = [99998]

        store = painted_points.cacheSnapshot()
        mutate(store)
        painted_points["cacheBlob"].setValue(
            IECore.UCharVectorData(list(self._pack_store_blob(store)))
        )

        summary = painted_points.validateCache()

        self.assertIn("2 points", summary)
        self.assertGreater(painted_points["invalidPointCount"].getValue(), 0)
        self.assertEqual(painted_points["invalidStrokeCount"].getValue(), 0)
        self.assertGreater(painted_points["topologyMismatchCount"].getValue(), 0)
        self.assertEqual(painted_points["lastErrorMessage"].getValue(), "")
        self.assertIn(
            "invalidTargetPathId:999",
            list(painted_points["failingTargetPaths"].getValue()),
        )
        self.assertEqual(
            set(painted_points["validationCategories"].getValue()),
            {"cache", "attachment"},
        )

    def testValidateCacheReportsMissingExternalCacheThroughDiagnostics(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            script, _plane, painted_points, _attached_points = self._build_demo_graph()
            script_path = os.path.join(temp_dir, "painted_points_test.gfr")
            cache_path = os.path.join(temp_dir, "painted_points.cache")
            script["fileName"].setValue(script_path)
            painted_points["cachePath"].setValue(cache_path)

            painted_points.migrateCacheMode()
            os.remove(cache_path)

            summary = painted_points.validateCache()

            self.assertIn("Invalid scatter paint blob.", summary)
            self.assertEqual(painted_points["topologyMismatchCount"].getValue(), 1)
            self.assertEqual(
                painted_points["lastErrorMessage"].getValue(),
                "External cache does not exist: " + cache_path,
            )
            self.assertEqual(list(painted_points["failingTargetPaths"].getValue()), [])
            self.assertEqual(painted_points["cacheResolvedPath"].getValue(), cache_path)
            self.assertEqual(
                set(painted_points["validationCategories"].getValue()), {"cache"}
            )

    def testValidateCacheReportsExportReadinessWithoutAttachedPoints(self):
        _script, _plane, painted_points, attached_points = self._build_demo_graph()
        attached_points["points"].setInput(None)

        summary = painted_points.validateCache()

        self.assertIn("2 points", summary)
        self.assertEqual(painted_points["lastErrorMessage"].getValue(), "")
        self.assertEqual(painted_points["topologyMismatchCount"].getValue(), 0)
        self.assertEqual(
            set(painted_points["validationCategories"].getValue()),
            {"exportReadiness"},
        )

    def testValidateCacheReportsDiagnosticsCategoryForCleanStore(self):
        _script, _plane, painted_points, _attached_points = self._build_demo_graph()

        summary = painted_points.validateCache()

        self.assertIn("2 points", summary)
        self.assertEqual(painted_points["invalidPointCount"].getValue(), 0)
        self.assertEqual(painted_points["invalidStrokeCount"].getValue(), 0)
        self.assertEqual(painted_points["topologyMismatchCount"].getValue(), 0)
        self.assertEqual(painted_points["lastErrorMessage"].getValue(), "")
        self.assertEqual(
            set(painted_points["validationCategories"].getValue()),
            {"diagnostics"},
        )

    def testValidateCacheReportsLockCategoryForSessionAwareMismatch(self):
        _script, _plane, painted_points, _attached_points = self._build_demo_graph()

        def mutate(store):
            store["lock"]["mode"] = 1
            store["lock"]["user"] = ""

        store = painted_points.cacheSnapshot()
        mutate(store)
        painted_points["cacheBlob"].setValue(
            IECore.UCharVectorData(list(self._pack_store_blob(store)))
        )

        summary = painted_points.validateCache()

        self.assertIn("2 points", summary)
        self.assertEqual(painted_points["invalidPointCount"].getValue(), 0)
        self.assertEqual(painted_points["invalidStrokeCount"].getValue(), 0)
        self.assertEqual(painted_points["topologyMismatchCount"].getValue(), 1)
        self.assertEqual(painted_points["lastErrorMessage"].getValue(), "")
        self.assertEqual(
            set(painted_points["validationCategories"].getValue()),
            {"lock"},
        )

    def testValidateCacheReportsUpgradeStateForSchemaMismatch(self):
        _script, _plane, painted_points, _attached_points = self._build_demo_graph()

        blob_bytes = bytearray(self._blob_bytes(painted_points))
        payload_offset = len(GafferScatterPaintShared.CACHE_MAGIC) + 4 + 4 + 4 + 4 + 8
        blob_bytes[payload_offset : payload_offset + 4] = (999).to_bytes(4, "little")
        painted_points["cacheBlob"].setValue(IECore.UCharVectorData(list(blob_bytes)))

        summary = painted_points.validateCache()

        self.assertIn("Invalid scatter paint blob", summary)
        self.assertEqual(painted_points["topologyMismatchCount"].getValue(), 1)
        self.assertIn(
            "checksum mismatch", painted_points["lastErrorMessage"].getValue()
        )
        self.assertEqual(
            set(painted_points["validationCategories"].getValue()),
            {"cache"},
        )

    def testValidateCacheAcceptsLargeChunkBoundaryWithoutErrors(self):
        _script, _plane, painted_points, _attached_points = self._build_demo_graph()

        stroke_id = painted_points.lastStrokeId()
        large_points = []
        for index in range(8193):
            large_points.append(
                {
                    "P": [float(index) * 0.001, 0.0, 0.0],
                    "width": 0.05,
                    "scale": 1.0,
                    "seed": index,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": index % 2,
                    "barycentric": [0.34, 0.33, 0.33],
                    "attachmentResolved": True,
                }
            )

        painted_points.paintStrokeCommit(stroke_id, large_points, append=False)

        summary = painted_points.validateCache()

        self.assertIn("8193 points", summary)
        self.assertEqual(painted_points["invalidPointCount"].getValue(), 0)
        self.assertEqual(painted_points["invalidStrokeCount"].getValue(), 0)
        self.assertEqual(painted_points["topologyMismatchCount"].getValue(), 0)
        self.assertEqual(painted_points["lastErrorMessage"].getValue(), "")
        self.assertEqual(
            set(painted_points["validationCategories"].getValue()),
            {"diagnostics"},
        )

    def testExportDiagnosticsIncludesInvalidTargetDetails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            script, _plane, painted_points, _attached_points = self._build_demo_graph()
            script["fileName"].setValue(
                os.path.join(temp_dir, "painted_points_test.gfr")
            )

            def mutate(store):
                point = store["points"][0]
                point["targetPathId"] = 999
                point["barycentric"] = [0.5, 0.25]

            painted_points.mutateCacheStore(mutate)

            diagnostics_path = painted_points.exportDiagnostics()
            with open(diagnostics_path, "r", encoding="utf-8") as handle:
                diagnostics_text = handle.read()

            self.assertIn("validationCategories: cache, attachment", diagnostics_text)
            self.assertIn("invalidPointCount: ", diagnostics_text)
            self.assertIn("failingFrame: 0", diagnostics_text)
            self.assertIn("topologyMismatchCount: ", diagnostics_text)
            self.assertIn("invalidTargetPathId:999", diagnostics_text)
            self.assertIn("lastErrorMessage: ", diagnostics_text)
            self.assertIn("cacheResolvedPath: ", diagnostics_text)
            self.assertIn("cacheVersion: 2", diagnostics_text)

    def testUpgradeCacheRejectsUnsupportedFutureSchemaBlob(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            script, _plane, painted_points, _attached_points = self._build_demo_graph()
            script["fileName"].setValue(
                os.path.join(temp_dir, "painted_points_test.gfr")
            )
            cache_path = os.path.join(temp_dir, "painted_points.cache")
            painted_points["cachePath"].setValue(cache_path)

            painted_points.migrateCacheMode()

            original_store = self._read_blob(cache_path)
            original_store["schemaVersion"] = 999
            self._write_blob(cache_path, original_store)

            expected_path = self._cache_text_path(cache_path, ".v2.upgrade")

            with self.assertRaisesRegex(
                RuntimeError,
                "Invalid scatter paint blob: unsupported schema version 999",
            ):
                painted_points.upgradeCache()

            self.assertFalse(os.path.exists(expected_path))
            self.assertEqual(painted_points["cachePath"].getValue(), cache_path)
            self.assertEqual(
                os.path.normpath(painted_points["cacheResolvedPath"].getValue()),
                os.path.normpath(cache_path),
            )
            self.assertEqual(painted_points["cacheVersion"].getValue(), 2)
            self.assertFalse(os.path.exists(cache_path + ".lock"))

    def testCompactCacheRebuildsChunksAndBumpsGeneration(self):
        _script, _plane, painted_points, _attached_points = self._build_demo_graph()

        stroke_id = painted_points.lastStrokeId()
        large_points = []
        for index in range(9000):
            large_points.append(
                {
                    "P": [float(index) * 0.001, 0.0, 0.0],
                    "width": 0.05,
                    "scale": 1.0,
                    "seed": index,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": index % 2,
                    "barycentric": [0.34, 0.33, 0.33],
                    "attachmentResolved": True,
                }
            )

        painted_points.paintStrokeCommit(stroke_id, large_points, append=False)
        before_snapshot = painted_points.cacheSnapshot()
        before_chunks = sorted(
            [
                chunk
                for chunk in before_snapshot["chunks"]
                if chunk["strokeId"] == stroke_id
            ],
            key=lambda item: item["chunkIndex"],
        )

        compacted_chunk_count = painted_points.compactCache()
        self.assertEqual(compacted_chunk_count, 2)

        after_snapshot = painted_points.cacheSnapshot()
        after_chunks = sorted(
            [
                chunk
                for chunk in after_snapshot["chunks"]
                if chunk["strokeId"] == stroke_id
            ],
            key=lambda item: item["chunkIndex"],
        )
        self.assertEqual(len(after_chunks), 2)
        self.assertEqual([chunk["pointCount"] for chunk in after_chunks], [8192, 808])
        self.assertEqual(
            [chunk["generation"] for chunk in after_chunks],
            [chunk["generation"] + 1 for chunk in before_chunks],
        )
        self.assertIn("9000 points", painted_points.validateCache())
        self.assertEqual(painted_points["topologyMismatchCount"].getValue(), 0)

    def testSplitStrokeBySelectionSplitsInteriorSubsetIntoNewStrokes(self):
        _script, _plane, painted_points, _attached_points = self._build_empty_graph()

        layer_id = painted_points.createLayer("Split Layer")
        unaffected_stroke_id = painted_points.createStroke(layer_id, "Unaffected")
        split_stroke_id = painted_points.createStroke(layer_id, "Split Me")
        painted_points.paintStrokeCommit(
            unaffected_stroke_id,
            [
                {
                    "P": [-1.0, 0.0, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 90,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                }
            ],
            append=True,
        )
        painted_points.paintStrokeCommit(
            split_stroke_id,
            [
                {
                    "P": [0.0, 0.0, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 100,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                },
                {
                    "P": [0.1, 0.0, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 101,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                },
                {
                    "P": [0.2, 0.0, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 102,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                },
                {
                    "P": [0.3, 0.0, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 103,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                },
            ],
            append=True,
        )

        point_records = painted_points.pointRecords()
        split_points = [
            point
            for point in point_records
            if int(point["strokeId"]) == int(split_stroke_id)
        ]
        selected_point_id = split_points[1]["pointId"]
        untouched_point_id = split_points[0]["pointId"]
        selection_set = painted_points.storeCurrentSelection(name="Before Split")
        painted_points.setCurrentSelection([selected_point_id], [])
        message = painted_points.splitStrokeBySelection()

        self.assertIn("Removed 1 selected points", message)

        strokes = painted_points.strokeRecords()
        remaining_ids = {stroke["strokeId"] for stroke in strokes}
        self.assertIn(unaffected_stroke_id, remaining_ids)
        self.assertNotIn(split_stroke_id, remaining_ids)

        split_replacements = [
            stroke
            for stroke in strokes
            if stroke["layerId"] == layer_id
            and int(stroke["strokeId"]) != int(unaffected_stroke_id)
        ]
        self.assertEqual(len(split_replacements), 2)
        self.assertTrue(
            all(
                int(stroke["strokeId"]) != int(split_stroke_id)
                for stroke in split_replacements
            )
        )

        points = painted_points.pointRecords()
        surviving_split_points = [
            point
            for point in points
            if int(point["strokeId"])
            in {int(stroke["strokeId"]) for stroke in split_replacements}
        ]
        self.assertEqual(len(surviving_split_points), 3)
        self.assertNotIn(selected_point_id, [point["pointId"] for point in points])
        self.assertIn(untouched_point_id, [point["pointId"] for point in points])

        split_snapshot = painted_points.cacheSnapshot()
        self.assertEqual(split_snapshot["currentSelection"]["pointIds"], [])
        stored_selection = next(
            item
            for item in split_snapshot["selectionSets"]
            if int(item["selectionSetId"]) == int(selection_set["selectionSetId"])
        )
        self.assertEqual(stored_selection["pointIds"], [])
        self.assertEqual(stored_selection["strokeIds"], [])

    def testSplitStrokeBySelectionDeletesFullySelectedStroke(self):
        _script, _plane, painted_points, _attached_points = self._build_demo_graph()

        point_records = painted_points.pointRecords()
        stroke_id = point_records[0]["strokeId"]
        selection_set = painted_points.storeCurrentSelection(name="Whole Stroke")
        painted_points.setCurrentSelection([], [stroke_id])
        message = painted_points.splitStrokeBySelection()

        self.assertIn("Removed 2 selected points", message)
        self.assertEqual(painted_points.strokeRecords(), [])
        self.assertEqual(painted_points.pointRecords(), [])

        snapshot = painted_points.cacheSnapshot()
        self.assertEqual(snapshot["currentSelection"]["strokeIds"], [])
        stored_selection = next(
            item
            for item in snapshot["selectionSets"]
            if int(item["selectionSetId"]) == int(selection_set["selectionSetId"])
        )
        self.assertEqual(stored_selection["pointIds"], [])
        self.assertEqual(stored_selection["strokeIds"], [])

        validation = painted_points.validateCache()
        self.assertIn("0 strokes", validation)
        self.assertEqual(painted_points["topologyMismatchCount"].getValue(), 0)

    def testRelaxSelectionMutatesOnlySelectedInteriorPoint(self):
        script, _plane, painted_points, _attached_points = self._build_empty_graph()

        layer_id = painted_points.createLayer("Relax Layer")
        stroke_id = painted_points.createStroke(layer_id, "Relax Stroke")
        painted_points.paintStrokeCommit(
            stroke_id,
            [
                {
                    "P": [0.0, 0.0, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 200,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                },
                {
                    "P": [0.4, 0.0, 0.4],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 201,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 1,
                    "barycentric": [0.1, 0.1, 0.8],
                    "attachmentResolved": True,
                },
                {
                    "P": [0.2, 0.0, 0.2],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 202,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 1,
                    "barycentric": [0.25, 0.5, 0.25],
                    "attachmentResolved": True,
                },
            ],
            append=True,
        )

        point_records = painted_points.pointRecords()
        start_point = point_records[0]
        middle_point = point_records[1]
        end_point = point_records[2]

        painted_points.setCurrentSelection([middle_point["pointId"]], [])

        context = script.context()
        context.setFrame(29)
        with context:
            updated_count = painted_points.relaxSelection()

        self.assertEqual(updated_count, 1)

        updated_points = {
            point["pointId"]: point for point in painted_points.pointRecords()
        }
        updated_start = updated_points[start_point["pointId"]]
        updated_middle = updated_points[middle_point["pointId"]]
        updated_end = updated_points[end_point["pointId"]]

        self.assertEqual(updated_start["lastValidFrame"], start_point["lastValidFrame"])
        self.assertEqual(updated_end["lastValidFrame"], end_point["lastValidFrame"])
        self.assertEqual(updated_middle["lastValidFrame"], 29)
        self.assertEqual(updated_middle["topologyGeneration"], 29)
        self.assertTrue(updated_middle["attachmentResolved"])
        self.assertNotEqual(updated_middle["P"], middle_point["P"])
        self.assertIn(updated_middle["triangleIndex"], [0, 1])
        self.assertIn("3 points", painted_points.validateCache())
        self.assertEqual(painted_points["topologyMismatchCount"].getValue(), 0)

    def testRelaxSelectionExpandsSelectedStroke(self):
        script, _plane, painted_points, _attached_points = self._build_demo_graph()

        stroke_id = painted_points.pointRecords()[0]["strokeId"]
        painted_points.setCurrentSelection([], [stroke_id])

        context = script.context()
        context.setFrame(31)
        with context:
            updated_count = painted_points.relaxSelection()

        self.assertEqual(updated_count, 2)

        updated_points = [
            point
            for point in painted_points.pointRecords()
            if int(point["strokeId"]) == int(stroke_id)
        ]
        self.assertEqual(len(updated_points), 2)
        self.assertTrue(all(point["lastValidFrame"] == 31 for point in updated_points))
        self.assertTrue(
            all(point["topologyGeneration"] == 31 for point in updated_points)
        )
        self.assertTrue(all(point["attachmentResolved"] for point in updated_points))
        self.assertNotEqual(updated_points[1]["P"], [0.2, 0.0, 0.2])
        self.assertIn("2 points", painted_points.validateCache())
        self.assertEqual(painted_points["topologyMismatchCount"].getValue(), 0)

    def testRelaxSelectionEvenRedistributionSkipsCurrentPointWeight(self):
        def relax_middle_x(relax_objective):
            script, _plane, painted_points, _attached_points = self._build_empty_graph()

            layer_id = painted_points.createLayer("Relax Objective Layer")
            stroke_id = painted_points.createStroke(layer_id, "Relax Objective Stroke")
            painted_points.paintStrokeCommit(
                stroke_id,
                [
                    {
                        "P": [0.0, 0.0, 0.0],
                        "width": 0.1,
                        "scale": 1.0,
                        "seed": 300,
                        "sourcePath": "/paintPlane",
                        "triangleIndex": 0,
                        "barycentric": [0.5, 0.25, 0.25],
                        "attachmentResolved": True,
                    },
                    {
                        "P": [0.8, 0.0, 0.8],
                        "width": 0.1,
                        "scale": 1.0,
                        "seed": 301,
                        "sourcePath": "/paintPlane",
                        "triangleIndex": 1,
                        "barycentric": [0.1, 0.1, 0.8],
                        "attachmentResolved": True,
                    },
                    {
                        "P": [0.2, 0.0, 0.2],
                        "width": 0.1,
                        "scale": 1.0,
                        "seed": 302,
                        "sourcePath": "/paintPlane",
                        "triangleIndex": 1,
                        "barycentric": [0.25, 0.5, 0.25],
                        "attachmentResolved": True,
                    },
                ],
                append=True,
            )

            middle_point = painted_points.pointRecords()[1]
            painted_points.setCurrentSelection([middle_point["pointId"]], [])
            painted_points["relaxObjective"].setValue(relax_objective)

            with script.context():
                updated_count = painted_points.relaxSelection()

            self.assertEqual(updated_count, 1)
            updated_middle = next(
                point
                for point in painted_points.pointRecords()
                if int(point["pointId"]) == int(middle_point["pointId"])
            )
            return updated_middle["restObjectP"][0]

        preserve_x = relax_middle_x(0)
        even_x = relax_middle_x(1)

        self.assertNotEqual(preserve_x, even_x)
        self.assertLess(abs(even_x - 0.1), abs(preserve_x - 0.1))

    def testReprojectSelectionUpdatesSelectedPointAnchors(self):
        (
            script,
            _plane,
            _transform,
            painted_points,
            _attached_points,
        ) = self._build_transformed_graph()

        layer_id = painted_points.createLayer("Reproject Layer")
        stroke_id = painted_points.createStroke(layer_id, "Reproject Stroke")
        painted_points.paintStrokeCommit(
            stroke_id,
            [
                {
                    "P": [0.0, 0.0, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 10,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                }
            ],
            append=True,
        )

        point_id = painted_points.pointRecords()[0]["pointId"]

        def mutate(store):
            point = next(
                item
                for item in store["points"]
                if int(item.get("pointId", 0)) == int(point_id)
            )
            point["triangleIndex"] = 99999
            point["barycentric"] = [1.0, 0.0, 0.0]
            point["restObjectP"] = [0.25, 0.0, 0.25]
            point["restWorldP"] = [77.0, 0.0, 0.0]
            point["restNormal"] = [1.0, 0.0, 0.0]
            point["restUp"] = [0.0, 1.0, 0.0]
            point["restUV"] = [0.0, 0.0]
            point["anchorModeUsed"] = 3
            point["valid"] = False

        painted_points.mutateCacheStore(mutate)
        painted_points.setCurrentSelection([point_id], [])

        context = script.context()
        context.setFrame(7)
        with context:
            updated_count = painted_points.reprojectSelection()

        self.assertEqual(updated_count, 1)

        point = next(
            item
            for item in painted_points.pointRecords()
            if int(item["pointId"]) == int(point_id)
        )
        self.assertIn(
            point["sourcePath"], ["/paintPlane", "/paintPlane/instances/paintProto/2"]
        )
        self.assertIn(point["triangleIndex"], [0, 1])
        self.assertAlmostEqual(sum(point["barycentric"]), 1.0, places=5)
        self.assertTrue(all(value >= 0.0 for value in point["barycentric"]))
        self.assertEqual(point["lastValidFrame"], 7)
        self.assertEqual(point["topologyGeneration"], 7)
        self.assertTrue(point["attachmentResolved"])
        self.assertAlmostEqual(point["restObjectP"][0], 0.25, places=5)
        self.assertAlmostEqual(point["P"][0], point["restObjectP"][0] + 3.0, places=5)
        self.assertAlmostEqual(point["P"][1], point["restObjectP"][1], places=5)
        self.assertAlmostEqual(point["P"][2], point["restObjectP"][2], places=5)

    def testReprojectSelectionExpandsSelectedStrokePoints(self):
        script, _plane, painted_points, _attached_points = self._build_demo_graph()

        point_records = painted_points.pointRecords()
        stroke_id = point_records[0]["strokeId"]
        painted_points.setCurrentSelection([], [stroke_id])

        context = script.context()
        context.setFrame(11)
        with context:
            updated_count = painted_points.reprojectSelection()

        self.assertEqual(updated_count, 2)

        updated_points = [
            point
            for point in painted_points.pointRecords()
            if int(point["strokeId"]) == int(stroke_id)
        ]
        self.assertEqual(len(updated_points), 2)
        self.assertTrue(all(point["lastValidFrame"] == 11 for point in updated_points))
        self.assertTrue(
            all(point["topologyGeneration"] == 11 for point in updated_points)
        )
        self.assertTrue(all(point["attachmentResolved"] for point in updated_points))

    def testReprojectSelectionUsesTargetFilterForCrossMeshCandidate(self):
        (
            script,
            _plane,
            _other_plane,
            _group,
            _other_set,
            painted_points,
            _attached_points,
        ) = self._build_multi_mesh_graph()

        layer_id = painted_points.createLayer("Cross Mesh Layer")
        stroke_id = painted_points.createStroke(layer_id, "Cross Mesh Stroke")
        painted_points.paintStrokeCommit(
            stroke_id,
            [
                {
                    "P": [0.0, 0.0, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 20,
                    "sourcePath": "/root/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                }
            ],
            append=True,
        )

        point_id = painted_points.pointRecords()[0]["pointId"]

        def mutate(store):
            point = next(
                item
                for item in store["points"]
                if int(item.get("pointId", 0)) == int(point_id)
            )
            point["triangleIndex"] = 99999
            point["restObjectP"] = [2.95, 0.0, 0.0]
            point["restWorldP"] = [2.95, 0.0, 0.0]
            point["anchorModeUsed"] = 3
            point["valid"] = False

        painted_points.mutateCacheStore(mutate)
        painted_points["targetFilter"].setValue("/root/otherPlane")
        painted_points.setCurrentSelection([point_id], [])

        context = script.context()
        context.setFrame(13)
        with context:
            updated_count = painted_points.reprojectSelection()

        self.assertEqual(updated_count, 1)

        point = next(
            item
            for item in painted_points.pointRecords()
            if int(item["pointId"]) == int(point_id)
        )
        self.assertEqual(point["sourcePath"], "/root/otherPlane")
        self.assertEqual(point["instanceSourcePath"], "")
        self.assertAlmostEqual(point["P"][0], point["restObjectP"][0] + 3.0, places=5)
        self.assertAlmostEqual(point["P"][1], point["restObjectP"][1], places=5)
        self.assertAlmostEqual(point["P"][2], point["restObjectP"][2], places=5)
        self.assertEqual(point["lastValidFrame"], 13)
        self.assertEqual(point["topologyGeneration"], 13)
        self.assertTrue(point["attachmentResolved"])

    def testReprojectSelectionUsesTargetSetFilterForCrossMeshCandidate(self):
        (
            script,
            _plane,
            _other_plane,
            _group,
            _other_set,
            painted_points,
            _attached_points,
        ) = self._build_multi_mesh_graph()

        layer_id = painted_points.createLayer("Cross Mesh Set Layer")
        stroke_id = painted_points.createStroke(layer_id, "Cross Mesh Set Stroke")
        painted_points.paintStrokeCommit(
            stroke_id,
            [
                {
                    "P": [0.0, 0.0, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 21,
                    "sourcePath": "/root/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                }
            ],
            append=True,
        )

        point_id = painted_points.pointRecords()[0]["pointId"]

        def mutate(store):
            point = next(
                item
                for item in store["points"]
                if int(item.get("pointId", 0)) == int(point_id)
            )
            point["triangleIndex"] = 99999
            point["restObjectP"] = [2.95, 0.0, 0.0]
            point["restWorldP"] = [2.95, 0.0, 0.0]
            point["anchorModeUsed"] = 3
            point["valid"] = False

        painted_points.mutateCacheStore(mutate)
        painted_points["targetSetFilter"].setValue("otherTargets")
        painted_points.setCurrentSelection([point_id], [])

        context = script.context()
        context.setFrame(17)
        with context:
            updated_count = painted_points.reprojectSelection()

        self.assertEqual(updated_count, 1)

        point = next(
            item
            for item in painted_points.pointRecords()
            if int(item["pointId"]) == int(point_id)
        )
        self.assertEqual(point["sourcePath"], "/root/otherPlane")
        self.assertEqual(point["instanceSourcePath"], "")
        self.assertAlmostEqual(point["P"][0], point["restObjectP"][0] + 3.0, places=5)
        self.assertEqual(point["lastValidFrame"], 17)
        self.assertEqual(point["topologyGeneration"], 17)
        self.assertTrue(point["attachmentResolved"])

    def testReprojectSelectionDoesNotCrossInstancesWithoutExplicitMultiSelection(self):
        (
            script,
            _plane,
            _prototype,
            _instancer,
            painted_points,
            _attached_points,
        ) = self._build_instanced_graph()

        layer_id = painted_points.createLayer("Instance Solo Layer")
        stroke_id = painted_points.createStroke(layer_id, "Instance Solo Stroke")
        painted_points.paintStrokeCommit(
            stroke_id,
            [
                {
                    "P": [0.0, 0.0, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 30,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [1.0, 0.0, 0.0],
                    "attachmentResolved": True,
                }
            ],
            append=True,
        )

        point_id = painted_points.pointRecords()[0]["pointId"]

        def mutate(store):
            instance_paths = store.setdefault("instanceSourcePaths", [])
            try:
                instance_source_path_id = (
                    instance_paths.index("/paintPlane/instances/paintProto") + 1
                )
            except ValueError:
                instance_paths.append("/paintPlane/instances/paintProto")
                instance_source_path_id = len(instance_paths)
            point = next(
                item
                for item in store["points"]
                if int(item.get("pointId", 0)) == int(point_id)
            )
            point["instanceId"] = 2
            point["instanceSourcePathId"] = instance_source_path_id
            point["triangleIndex"] = 99999
            point["restObjectP"] = [0.0, 0.0, 0.0]
            point["restWorldP"] = [0.45, 0.45, 0.0]
            point["anchorModeUsed"] = 3
            point["valid"] = False

        painted_points.mutateCacheStore(mutate)
        painted_points["targetFilter"].setValue(
            "/paintPlane/instances/paintProto/0 /paintPlane/instances/paintProto/3"
        )
        painted_points.setCurrentSelection([point_id], [])

        context = script.context()
        context.setFrame(19)
        with context:
            updated_count = painted_points.reprojectSelection()

        self.assertEqual(updated_count, 1)

        point = next(
            item
            for item in painted_points.pointRecords()
            if int(item["pointId"]) == int(point_id)
        )
        self.assertEqual(point["sourcePath"], "/paintPlane")
        self.assertEqual(
            point["instanceSourcePath"], "/paintPlane/instances/paintProto"
        )
        self.assertEqual(point["instanceId"], 2)
        self.assertAlmostEqual(point["P"][0], point["restObjectP"][0] - 0.5, places=5)
        self.assertAlmostEqual(point["P"][1], point["restObjectP"][1] + 0.5, places=5)
        self.assertEqual(point["lastValidFrame"], 19)
        self.assertEqual(point["topologyGeneration"], 19)

    def testReprojectSelectionCrossesInstancesWhenMultipleInstancesSelected(self):
        (
            script,
            _plane,
            _prototype,
            _instancer,
            painted_points,
            _attached_points,
        ) = self._build_instanced_graph()

        layer_id = painted_points.createLayer("Instance Multi Layer")
        stroke_a = painted_points.createStroke(layer_id, "Instance Stroke A")
        stroke_b = painted_points.createStroke(layer_id, "Instance Stroke B")
        painted_points.paintStrokeCommit(
            stroke_a,
            [
                {
                    "P": [0.0, 0.0, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 31,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [1.0, 0.0, 0.0],
                    "attachmentResolved": True,
                }
            ],
            append=True,
        )
        painted_points.paintStrokeCommit(
            stroke_b,
            [
                {
                    "P": [0.0, 0.0, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 32,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [1.0, 0.0, 0.0],
                    "attachmentResolved": True,
                }
            ],
            append=True,
        )

        point_records = painted_points.pointRecords()
        point_a_id = next(
            point["pointId"]
            for point in point_records
            if int(point["strokeId"]) == int(stroke_a)
        )
        point_b_id = next(
            point["pointId"]
            for point in point_records
            if int(point["strokeId"]) == int(stroke_b)
        )

        def mutate(store):
            instance_paths = store.setdefault("instanceSourcePaths", [])
            try:
                instance_source_path_id = (
                    instance_paths.index("/paintPlane/instances/paintProto") + 1
                )
            except ValueError:
                instance_paths.append("/paintPlane/instances/paintProto")
                instance_source_path_id = len(instance_paths)

            point_a = next(
                item
                for item in store["points"]
                if int(item.get("pointId", 0)) == int(point_a_id)
            )
            point_a["instanceId"] = 2
            point_a["instanceSourcePathId"] = instance_source_path_id
            point_a["triangleIndex"] = 99999
            point_a["restObjectP"] = [1.0, 0.0, 0.0]
            point_a["restWorldP"] = [0.5, 0.5, 0.0]
            point_a["anchorModeUsed"] = 3
            point_a["valid"] = False

            point_b = next(
                item
                for item in store["points"]
                if int(item.get("pointId", 0)) == int(point_b_id)
            )
            point_b["instanceId"] = 3
            point_b["instanceSourcePathId"] = instance_source_path_id

        painted_points.mutateCacheStore(mutate)
        painted_points["targetFilter"].setValue(
            "/paintPlane/instances/paintProto/2 /paintPlane/instances/paintProto/3"
        )
        painted_points.setCurrentSelection([point_a_id, point_b_id], [])

        context = script.context()
        context.setFrame(23)
        with context:
            updated_count = painted_points.reprojectSelection()

        self.assertEqual(updated_count, 2)

        point_a = next(
            item
            for item in painted_points.pointRecords()
            if int(item["pointId"]) == int(point_a_id)
        )
        self.assertEqual(point_a["sourcePath"], "/paintPlane")
        self.assertEqual(
            point_a["instanceSourcePath"], "/paintPlane/instances/paintProto"
        )
        self.assertEqual(point_a["instanceId"], 3)
        self.assertAlmostEqual(
            point_a["P"][0], point_a["restObjectP"][0] + 0.5, places=5
        )
        self.assertAlmostEqual(
            point_a["P"][1], point_a["restObjectP"][1] + 0.5, places=5
        )
        self.assertEqual(point_a["lastValidFrame"], 23)
        self.assertEqual(point_a["topologyGeneration"], 23)

    def testFallbackSplitStrokeBySelectionSplitsInteriorSubsetIntoNewStrokes(self):
        _script, _plane, painted_points, _attached_points = (
            self._build_empty_graph_with_classes(
                painted_points_class=GafferScatterPaintCore.PaintedPoints,
                attached_points_class=GafferScatterPaintCore.AttachedPoints,
            )
        )

        layer_id = painted_points.createLayer("Split Layer")
        unaffected_stroke_id = painted_points.createStroke(layer_id, "Unaffected")
        split_stroke_id = painted_points.createStroke(layer_id, "Split Me")
        painted_points.paintStrokeCommit(
            unaffected_stroke_id,
            [
                {
                    "P": [-1.0, 0.0, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 90,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                }
            ],
            append=True,
        )
        painted_points.paintStrokeCommit(
            split_stroke_id,
            [
                {
                    "P": [0.0, 0.0, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 100,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                },
                {
                    "P": [0.1, 0.0, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 101,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                },
                {
                    "P": [0.2, 0.0, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 102,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                },
                {
                    "P": [0.3, 0.0, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 103,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                },
            ],
            append=True,
        )

        point_records = painted_points.pointRecords()
        split_points = [
            point
            for point in point_records
            if int(point["strokeId"]) == int(split_stroke_id)
        ]
        selected_point_id = split_points[1]["pointId"]
        untouched_point_id = split_points[0]["pointId"]
        selection_set_id = painted_points.storeCurrentSelection(name="Before Split")
        painted_points.setCurrentSelection([selected_point_id], [])
        message = painted_points.splitStrokeBySelection()

        self.assertIn("Removed 1 selected points", message)

        strokes = painted_points.strokeRecords()
        remaining_ids = {stroke["strokeId"] for stroke in strokes}
        self.assertIn(unaffected_stroke_id, remaining_ids)
        self.assertNotIn(split_stroke_id, remaining_ids)

        split_replacements = [
            stroke
            for stroke in strokes
            if stroke["layerId"] == layer_id
            and int(stroke["strokeId"]) != int(unaffected_stroke_id)
        ]
        self.assertEqual(len(split_replacements), 2)

        points = painted_points.pointRecords()
        surviving_split_points = [
            point
            for point in points
            if int(point["strokeId"])
            in {int(stroke["strokeId"]) for stroke in split_replacements}
        ]
        self.assertEqual(len(surviving_split_points), 3)
        self.assertNotIn(selected_point_id, [point["pointId"] for point in points])
        self.assertIn(untouched_point_id, [point["pointId"] for point in points])

        split_snapshot = painted_points.cacheSnapshot()
        self.assertEqual(split_snapshot["currentSelection"]["pointIds"], [])
        stored_selection = next(
            item
            for item in split_snapshot["selectionSets"]
            if int(item["selectionSetId"]) == int(selection_set_id)
        )
        self.assertEqual(stored_selection["pointIds"], [])
        self.assertEqual(stored_selection["strokeIds"], [])

    def testFallbackRelaxSelectionMutatesOnlySelectedInteriorPoint(self):
        script, _plane, painted_points, _attached_points = (
            self._build_empty_graph_with_classes(
                painted_points_class=GafferScatterPaintCore.PaintedPoints,
                attached_points_class=GafferScatterPaintCore.AttachedPoints,
            )
        )

        layer_id = painted_points.createLayer("Relax Layer")
        stroke_id = painted_points.createStroke(layer_id, "Relax Stroke")
        painted_points.paintStrokeCommit(
            stroke_id,
            [
                {
                    "P": [0.0, 0.0, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 200,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                },
                {
                    "P": [0.4, 0.0, 0.4],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 201,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 1,
                    "barycentric": [0.1, 0.1, 0.8],
                    "attachmentResolved": True,
                },
                {
                    "P": [0.2, 0.0, 0.2],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 202,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 1,
                    "barycentric": [0.25, 0.5, 0.25],
                    "attachmentResolved": True,
                },
            ],
            append=True,
        )

        point_records = painted_points.pointRecords()
        start_point = point_records[0]
        middle_point = point_records[1]
        end_point = point_records[2]

        painted_points.setCurrentSelection([middle_point["pointId"]], [])

        context = script.context()
        context.setFrame(29)
        with context:
            updated_count = painted_points.relaxSelection()

        self.assertEqual(updated_count, 1)

        updated_points = {
            point["pointId"]: point for point in painted_points.pointRecords()
        }
        updated_start = updated_points[start_point["pointId"]]
        updated_middle = updated_points[middle_point["pointId"]]
        updated_end = updated_points[end_point["pointId"]]

        self.assertEqual(updated_start["lastValidFrame"], start_point["lastValidFrame"])
        self.assertEqual(updated_end["lastValidFrame"], end_point["lastValidFrame"])
        self.assertEqual(updated_middle["lastValidFrame"], 29)
        self.assertEqual(updated_middle["topologyGeneration"], 29)
        self.assertTrue(updated_middle["attachmentResolved"])
        self.assertNotEqual(updated_middle["P"], middle_point["P"])
        self.assertIn(updated_middle["triangleIndex"], [0, 1])
        self.assertIn("3 points", painted_points.validateCache())
        self.assertEqual(painted_points["topologyMismatchCount"].getValue(), 0)

    def testFallbackRelaxSelectionEvenRedistributionSkipsCurrentPointWeight(self):
        def relax_middle_x(relax_objective):
            script, _plane, painted_points, _attached_points = (
                self._build_empty_graph_with_classes(
                    painted_points_class=GafferScatterPaintCore.PaintedPoints,
                    attached_points_class=GafferScatterPaintCore.AttachedPoints,
                )
            )

            layer_id = painted_points.createLayer("Relax Objective Layer")
            stroke_id = painted_points.createStroke(layer_id, "Relax Objective Stroke")
            painted_points.paintStrokeCommit(
                stroke_id,
                [
                    {
                        "P": [0.0, 0.0, 0.0],
                        "width": 0.1,
                        "scale": 1.0,
                        "seed": 400,
                        "sourcePath": "/paintPlane",
                        "triangleIndex": 0,
                        "barycentric": [0.5, 0.25, 0.25],
                        "attachmentResolved": True,
                    },
                    {
                        "P": [0.8, 0.0, 0.8],
                        "width": 0.1,
                        "scale": 1.0,
                        "seed": 401,
                        "sourcePath": "/paintPlane",
                        "triangleIndex": 1,
                        "barycentric": [0.1, 0.1, 0.8],
                        "attachmentResolved": True,
                    },
                    {
                        "P": [0.2, 0.0, 0.2],
                        "width": 0.1,
                        "scale": 1.0,
                        "seed": 402,
                        "sourcePath": "/paintPlane",
                        "triangleIndex": 1,
                        "barycentric": [0.25, 0.5, 0.25],
                        "attachmentResolved": True,
                    },
                ],
                append=True,
            )

            middle_point = painted_points.pointRecords()[1]
            painted_points.setCurrentSelection([middle_point["pointId"]], [])
            painted_points["relaxObjective"].setValue(relax_objective)

            with script.context():
                updated_count = painted_points.relaxSelection()

            self.assertEqual(updated_count, 1)
            updated_middle = next(
                point
                for point in painted_points.pointRecords()
                if int(point["pointId"]) == int(middle_point["pointId"])
            )
            return updated_middle["restObjectP"][0]

        preserve_x = relax_middle_x(0)
        even_x = relax_middle_x(1)

        self.assertNotEqual(preserve_x, even_x)
        self.assertLess(abs(even_x - 0.1), abs(preserve_x - 0.1))

    def testFallbackReprojectSelectionUsesTargetFilterForCrossMeshCandidate(self):
        (
            script,
            _plane,
            _other_plane,
            _group,
            _other_set,
            painted_points,
            attached_points,
        ) = self._build_multi_mesh_graph_with_classes(
            painted_points_class=GafferScatterPaintCore.PaintedPoints,
            attached_points_class=GafferScatterPaintCore.AttachedPoints,
        )

        layer_id = painted_points.createLayer("Cross Mesh Layer")
        stroke_id = painted_points.createStroke(layer_id, "Cross Mesh Stroke")
        painted_points.paintStrokeCommit(
            stroke_id,
            [
                {
                    "P": [0.0, 0.0, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 20,
                    "sourcePath": "/root/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                }
            ],
            append=True,
        )

        point_id = painted_points.pointRecords()[0]["pointId"]

        def mutate(store):
            point = next(
                item
                for item in store["points"]
                if int(item.get("pointId", 0)) == int(point_id)
            )
            point["triangleIndex"] = 99999
            point["restObjectP"] = [2.95, 0.0, 0.0]
            point["restWorldP"] = [2.95, 0.0, 0.0]
            point["anchorModeUsed"] = 3
            point["valid"] = False

        painted_points.mutateCacheStore(mutate)
        attached_points["allowCrossMeshReproject"].setValue(True)
        painted_points["targetFilter"].setValue("/root/otherPlane")
        painted_points.setCurrentSelection([point_id], [])

        context = script.context()
        context.setFrame(13)
        with context:
            updated_count = painted_points.reprojectSelection()

        self.assertEqual(updated_count, 1)

        point = next(
            item
            for item in painted_points.pointRecords()
            if int(item["pointId"]) == int(point_id)
        )
        self.assertEqual(point["sourcePath"], "/root/otherPlane")
        self.assertEqual(point["instanceSourcePath"], "")
        self.assertAlmostEqual(point["P"][0], point["restObjectP"][0] + 3.0, places=5)
        self.assertAlmostEqual(point["P"][1], point["restObjectP"][1], places=5)
        self.assertAlmostEqual(point["P"][2], point["restObjectP"][2], places=5)
        self.assertEqual(point["lastValidFrame"], 13)
        self.assertEqual(point["topologyGeneration"], 13)
        self.assertTrue(point["attachmentResolved"])

    def testFallbackReprojectSelectionDoesNotCrossInstancesWithoutExplicitMultiSelection(
        self,
    ):
        (
            script,
            _plane,
            _prototype,
            _instancer,
            painted_points,
            attached_points,
        ) = self._build_instanced_graph_with_classes(
            painted_points_class=GafferScatterPaintCore.PaintedPoints,
            attached_points_class=GafferScatterPaintCore.AttachedPoints,
        )

        layer_id = painted_points.createLayer("Instance Solo Layer")
        stroke_id = painted_points.createStroke(layer_id, "Instance Solo Stroke")
        painted_points.paintStrokeCommit(
            stroke_id,
            [
                {
                    "P": [0.0, 0.0, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 30,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [1.0, 0.0, 0.0],
                    "attachmentResolved": True,
                }
            ],
            append=True,
        )

        point_id = painted_points.pointRecords()[0]["pointId"]

        def mutate(store):
            instance_paths = store.setdefault("instanceSourcePaths", [])
            try:
                instance_source_path_id = (
                    instance_paths.index("/paintPlane/instances/paintProto") + 1
                )
            except ValueError:
                instance_paths.append("/paintPlane/instances/paintProto")
                instance_source_path_id = len(instance_paths)
            point = next(
                item
                for item in store["points"]
                if int(item.get("pointId", 0)) == int(point_id)
            )
            point["instanceId"] = 2
            point["instanceSourcePathId"] = instance_source_path_id
            point["triangleIndex"] = 99999
            point["restObjectP"] = [0.0, 0.0, 0.0]
            point["restWorldP"] = [0.45, 0.45, 0.0]
            point["anchorModeUsed"] = 3
            point["valid"] = False

        painted_points.mutateCacheStore(mutate)
        attached_points["allowCrossMeshReproject"].setValue(True)
        painted_points["targetFilter"].setValue(
            "/paintPlane/instances/paintProto/0 /paintPlane/instances/paintProto/3"
        )
        painted_points.setCurrentSelection([point_id], [])

        context = script.context()
        context.setFrame(19)
        with context:
            updated_count = painted_points.reprojectSelection()

        self.assertEqual(updated_count, 1)

        point = next(
            item
            for item in painted_points.pointRecords()
            if int(item["pointId"]) == int(point_id)
        )
        self.assertEqual(point["sourcePath"], "/paintPlane")
        self.assertEqual(
            point["instanceSourcePath"], "/paintPlane/instances/paintProto"
        )
        self.assertEqual(point["instanceId"], 2)
        self.assertAlmostEqual(point["P"][0], point["restObjectP"][0] - 0.5, places=5)
        self.assertAlmostEqual(point["P"][1], point["restObjectP"][1] + 0.5, places=5)
        self.assertEqual(point["lastValidFrame"], 19)
        self.assertEqual(point["topologyGeneration"], 19)

    def testCompiledRefreshDerivedDataChunksLargeStrokes(self):
        _script, _plane, painted_points, _attached_points = self._build_demo_graph()

        stroke_id = painted_points.lastStrokeId()
        large_points = []
        for index in range(9000):
            large_points.append(
                {
                    "P": [float(index) * 0.001, 0.0, 0.0],
                    "width": 0.05,
                    "scale": 1.0,
                    "seed": index,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": index % 2,
                    "barycentric": [0.34, 0.33, 0.33],
                    "attachmentResolved": True,
                }
            )

        painted_points.paintStrokeCommit(stroke_id, large_points, append=False)
        snapshot = painted_points.cacheSnapshot()

        self.assertEqual(len(snapshot["points"]), 9000)
        self.assertEqual(len(snapshot["chunks"]), 2)
        chunks = sorted(
            [chunk for chunk in snapshot["chunks"] if chunk["strokeId"] == stroke_id],
            key=lambda item: item["chunkIndex"],
        )
        self.assertEqual([chunk["chunkIndex"] for chunk in chunks], [0, 1])
        self.assertEqual([chunk["pointCount"] for chunk in chunks], [8192, 808])
        self.assertEqual(chunks[0]["pointStart"], 0)
        self.assertEqual(chunks[1]["pointStart"], 8192)

        stroke = next(
            item for item in snapshot["strokes"] if item["strokeId"] == stroke_id
        )
        self.assertEqual(stroke["pointCount"], 9000)
        self.assertEqual(stroke["firstChunkId"], chunks[0]["chunkId"])
        self.assertEqual(stroke["lastChunkId"], chunks[-1]["chunkId"])

        self.assertIn("9000 points", painted_points.validateCache())
        self.assertEqual(painted_points["invalidPointCount"].getValue(), 0)
        self.assertEqual(painted_points["invalidStrokeCount"].getValue(), 0)
        self.assertEqual(painted_points["topologyMismatchCount"].getValue(), 0)

    def testBrushPaintCommitBenchmarkHandles5000Points(self):
        self._assert_brush_commit_benchmark(5000, expected_chunk_count=1)

    def testBrushPaintCommitBenchmarkHandles35000Points(self):
        self._assert_brush_commit_benchmark(35000, expected_chunk_count=5)


if __name__ == "__main__":
    unittest.main()
