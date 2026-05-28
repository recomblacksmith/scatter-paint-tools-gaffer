import unittest

import Gaffer
import GafferTest
import GafferScene
import IECore
import imath

import GafferScatterPaint
from GafferScatterPaint import _core as GafferScatterPaintCore


class AttachmentRegressionTest(unittest.TestCase):
    def _build_empty_graph(
        self, attached_points_class=GafferScatterPaint.AttachedPoints
    ):
        script = Gaffer.ScriptNode()

        plane = GafferScene.Plane("ScatterPaintPlane")
        plane["name"].setValue("paintPlane")
        script.addChild(plane)

        painted_points = GafferScatterPaint.PaintedPoints("PaintedPoints")
        script.addChild(painted_points)
        painted_points["in"].setInput(plane["out"])

        attached_points = attached_points_class("AttachedPoints")
        script.addChild(attached_points)
        attached_points["in"].setInput(plane["out"])
        attached_points["points"].setInput(painted_points["out"])
        attached_points["outputLocation"].setValue("/paintPlane/scatter")

        return script, plane, painted_points, attached_points

    def _build_transformed_plane_graph(self):
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

    def _build_instanced_plane_graph(self):
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

    def _build_demo_graph(
        self, attached_points_class=GafferScatterPaint.AttachedPoints
    ):
        script, plane, painted_points, attached_points = self._build_empty_graph(
            attached_points_class=attached_points_class
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

    def _add_stroke_point(
        self,
        painted_points,
        *,
        layer_name,
        stroke_name,
        position,
        seed,
        triangle_index=0,
        barycentric=None,
        source_path="/paintPlane",
    ):
        layer_id = painted_points.createLayer(layer_name)
        stroke_id = painted_points.createStroke(layer_id, stroke_name)
        painted_points.paintStrokeCommit(
            stroke_id,
            [
                {
                    "P": list(position),
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": seed,
                    "sourcePath": source_path,
                    "triangleIndex": triangle_index,
                    "barycentric": list(barycentric or [0.5, 0.25, 0.25]),
                    "attachmentResolved": True,
                }
            ],
            append=True,
        )
        point_ids = [
            int(point["pointId"])
            for point in painted_points.pointRecords()
            if int(point.get("strokeId", 0)) == int(stroke_id)
        ]
        self.assertEqual(len(point_ids), 1)
        return layer_id, stroke_id, point_ids[0]

    def _load_store(self, painted_points):
        return painted_points.cacheSnapshot()

    def _blob_bytes(self, painted_points):
        return bytes(
            int(value) & 0xFF for value in painted_points["cacheBlob"].getValue()
        )

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
        return self._mutate_points(painted_points, self._break_path_point)

    def _break_path_point(self, point):
        self._backup_point(point)
        point["sourcePath"] = "/paintPlaneMissing"
        point["attachmentResolved"] = True
        return True

    def _break_triangles(self, painted_points):
        return self._mutate_points(painted_points, self._break_triangle_point)

    def _break_triangle_point(self, point):
        self._backup_point(point)
        point["triangleIndex"] = int(point.get("triangleIndex", 0)) + 100000
        point["attachmentResolved"] = True
        return True

    def _repair(self, painted_points):
        return self._mutate_points(painted_points, self._repair_point)

    def _repair_point(self, point):
        backup = point.get("_demoAttachmentBackup")
        if not isinstance(backup, dict):
            return False
        point["sourcePath"] = backup.get("sourcePath", point.get("sourcePath", ""))
        point["triangleIndex"] = int(
            backup.get("triangleIndex", point.get("triangleIndex", 0))
        )
        point["barycentric"] = list(
            backup.get("barycentric", point.get("barycentric", [1.0, 0.0, 0.0]))
        )
        point["attachmentResolved"] = bool(
            backup.get("attachmentResolved", point.get("attachmentResolved", False))
        )
        del point["_demoAttachmentBackup"]
        return True

    def _validate(self, attached_points, frame=None):
        output_location = (
            attached_points["outputLocation"].getValue().strip() or "/scatter"
        )
        context = attached_points.scriptNode().context()
        if frame is not None:
            context.setFrame(frame)
        with context:
            error = None
            try:
                attached_points["out"].object(output_location)
            except RuntimeError as exc:
                error = str(exc)
            return {
                "error": error,
                "summary": attached_points["solveStatus"].getValue(),
                "resolved": attached_points["resolvedPointCount"].getValue(),
                "unresolved": attached_points["unresolvedPointCount"].getValue(),
                "invalid": attached_points["invalidPointCount"].getValue(),
                "lastValidFrame": attached_points["lastValidFrame"].getValue(),
                "topologyMismatches": attached_points[
                    "topologyMismatchCount"
                ].getValue(),
                "failureReasons": list(
                    attached_points["attachmentFailureReasons"].getValue()
                ),
                "failingPaths": list(attached_points["failingTargetPaths"].getValue()),
            }

    def _object_error(self, attached_points, frame=None):
        output_location = (
            attached_points["outputLocation"].getValue().strip() or "/scatter"
        )
        context = attached_points.scriptNode().context()
        if frame is not None:
            context.setFrame(frame)
        with context:
            with self.assertRaises(RuntimeError) as context_manager:
                attached_points["out"].object(output_location)
        return str(context_manager.exception)

    def _primitive_data(self, attached_points, variable_name, frame=None):
        output_location = (
            attached_points["outputLocation"].getValue().strip() or "/scatter"
        )
        context = attached_points.scriptNode().context()
        if frame is not None:
            context.setFrame(frame)
        with context:
            primitive = attached_points["out"].object(output_location)
            return list(primitive[variable_name].data)

    def _primitive_variables(self, attached_points, frame=None):
        output_location = (
            attached_points["outputLocation"].getValue().strip() or "/scatter"
        )
        context = attached_points.scriptNode().context()
        if frame is not None:
            context.setFrame(frame)
        with context:
            primitive = attached_points["out"].object(output_location)
            return set(primitive.keys())

    def _update_layer(self, painted_points, layer_id, **updates):
        def mutate(store):
            layer = next(
                item
                for item in store["layers"]
                if int(item.get("layerId", 0)) == int(layer_id)
            )
            layer.update(updates)

        painted_points.mutateCacheStore(mutate)

    def _update_stroke(self, painted_points, stroke_id, **updates):
        def mutate(store):
            stroke = next(
                item
                for item in store["strokes"]
                if int(item.get("strokeId", 0)) == int(stroke_id)
            )
            stroke.update(updates)

        painted_points.mutateCacheStore(mutate)

    def _color_tuples(self, attached_points, variable_name, frame=None):
        return [
            tuple(float(channel) for channel in color)
            for color in self._primitive_data(
                attached_points, variable_name, frame=frame
            )
        ]

    def _assert_colors_equal(self, actual, expected):
        self.assertEqual(len(actual), len(expected))
        for actual_color, expected_color in zip(actual, expected):
            self.assertEqual(len(actual_color), len(expected_color))
            for actual_channel, expected_channel in zip(actual_color, expected_color):
                self.assertAlmostEqual(actual_channel, expected_channel, places=6)

    def testHealthyBreakPathRepair(self):
        _script, _plane, painted_points, attached_points = self._build_demo_graph()

        healthy = self._validate(attached_points)
        self.assertGreater(healthy["resolved"], 0)
        self.assertFalse(healthy["failureReasons"])

        self.assertGreater(self._break_paths(painted_points), 0)
        broken = self._validate(attached_points)
        self.assertTrue(
            any(
                reason.startswith("missingTargetPath:")
                for reason in broken["failureReasons"]
            )
        )
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
        self.assertTrue(
            any(
                reason.startswith("missingTriangle:")
                for reason in broken["failureReasons"]
            )
        )
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
        self.assertTrue(
            any(
                reason.startswith("missingSourcePath:")
                for reason in result["failureReasons"]
            )
        )

    def testUnsupportedTargetObject(self):
        script = Gaffer.ScriptNode()

        camera = GafferScene.Camera("ScatterPaintCamera")
        camera["name"].setValue("paintCamera")
        script.addChild(camera)

        painted_points = GafferScatterPaint.PaintedPoints("PaintedPoints")
        script.addChild(painted_points)
        painted_points["in"].setInput(camera["out"])

        attached_points = GafferScatterPaint.AttachedPoints("AttachedPoints")
        script.addChild(attached_points)
        attached_points["in"].setInput(camera["out"])
        attached_points["points"].setInput(painted_points["out"])
        attached_points["outputLocation"].setValue("/paintCamera/scatter")

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
                    "sourcePath": "/paintCamera",
                    "triangleIndex": 0,
                    "barycentric": [1.0, 0.0, 0.0],
                    "attachmentResolved": True,
                }
            ],
            append=True,
        )

        result = self._validate(attached_points)
        self.assertTrue(
            any(
                reason.startswith("unsupportedTargetObject:")
                for reason in result["failureReasons"]
            )
        )

    def testPaintStrokeCommitDirtiesSceneOutputs(self):
        script, _plane, painted_points, attached_points = self._build_empty_graph()

        layer_id = painted_points.createLayer("Demo Layer")
        stroke_id = painted_points.createStroke(layer_id, "Demo Stroke")

        dirtied = GafferTest.CapturingSlot(painted_points.plugDirtiedSignal())

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
                }
            ],
            append=True,
        )

        dirtied_plugs = [entry[0] for entry in dirtied]

        def _contains(plug):
            return any(candidate.isSame(plug) for candidate in dirtied_plugs)

        self.assertTrue(_contains(painted_points["cacheBlob"]))
        self.assertTrue(_contains(painted_points["out"]))
        self.assertTrue(_contains(painted_points["out"]["object"]))
        self.assertTrue(_contains(painted_points["out"]["bound"]))
        self.assertTrue(_contains(painted_points["out"]["childNames"]))

        attached_object = attached_points["out"].object("/paintPlane/scatter")
        self.assertEqual(attached_object.numPoints, 1)

    def testLayerVisibilityMuteSoloFiltering(self):
        script, _plane, painted_points, attached_points = self._build_empty_graph()

        _layer_a, stroke_a, point_a = self._add_stroke_point(
            painted_points,
            layer_name="Layer A",
            stroke_name="Stroke A",
            position=[0.0, 0.0, 0.0],
            seed=10,
            triangle_index=0,
        )
        layer_b, stroke_b, point_b = self._add_stroke_point(
            painted_points,
            layer_name="Layer B",
            stroke_name="Stroke B",
            position=[0.2, 0.0, 0.2],
            seed=20,
            triangle_index=1,
        )

        self.assertEqual(
            sorted(self._primitive_data(attached_points, "id", frame=1)),
            sorted([point_a, point_b]),
        )
        self.assertEqual(
            sorted(self._primitive_data(attached_points, "strokeId", frame=1)),
            sorted([stroke_a, stroke_b]),
        )

        self._update_layer(painted_points, layer_b, visible=False)
        self.assertEqual(
            self._primitive_data(attached_points, "id", frame=1), [point_a]
        )

        self._update_layer(painted_points, layer_b, visible=True, mute=True)
        self.assertEqual(
            self._primitive_data(attached_points, "id", frame=1), [point_a]
        )

        self._update_layer(painted_points, layer_b, mute=False, enabled=False)
        self.assertEqual(
            self._primitive_data(attached_points, "id", frame=1), [point_a]
        )

        self._update_layer(painted_points, layer_b, enabled=True, solo=True)
        self.assertEqual(
            self._primitive_data(attached_points, "id", frame=1), [point_b]
        )

        self._update_layer(painted_points, layer_b, solo=False)
        self.assertEqual(
            sorted(self._primitive_data(attached_points, "id", frame=1)),
            sorted([point_a, point_b]),
        )

    def testCompiledLayerStateActionsAffectEvaluatedOutput(self):
        _script, _plane, painted_points, attached_points = self._build_empty_graph()

        _layer_a, _stroke_a, point_a = self._add_stroke_point(
            painted_points,
            layer_name="Layer A",
            stroke_name="Stroke A",
            position=[0.0, 0.0, 0.0],
            seed=110,
            triangle_index=0,
        )
        layer_b, _stroke_b, point_b = self._add_stroke_point(
            painted_points,
            layer_name="Layer B",
            stroke_name="Stroke B",
            position=[0.2, 0.0, 0.2],
            seed=120,
            triangle_index=1,
        )

        self.assertEqual(
            sorted(self._primitive_data(attached_points, "id", frame=1)),
            sorted([point_a, point_b]),
        )

        self.assertEqual(painted_points.setLayerVisible(layer_b, False), layer_b)
        self.assertEqual(
            self._primitive_data(attached_points, "id", frame=1), [point_a]
        )

        self.assertEqual(painted_points.setLayerVisible(layer_b, True), layer_b)
        self.assertEqual(painted_points.setLayerMute(layer_b, True), layer_b)
        self.assertEqual(
            self._primitive_data(attached_points, "id", frame=1), [point_a]
        )

        self.assertEqual(painted_points.setLayerMute(layer_b, False), layer_b)
        self.assertEqual(painted_points.setLayerSolo(layer_b, True), layer_b)
        self.assertEqual(
            self._primitive_data(attached_points, "id", frame=1), [point_b]
        )

        layer_records = {
            int(layer["layerId"]): layer for layer in painted_points.layerRecords()
        }
        self.assertTrue(layer_records[layer_b]["solo"])

        self.assertEqual(painted_points.setLayerSolo(layer_b, False), layer_b)
        self.assertEqual(
            sorted(self._primitive_data(attached_points, "id", frame=1)),
            sorted([point_a, point_b]),
        )

    def testLayerAndStrokeFrameFiltering(self):
        script, _plane, painted_points, attached_points = self._build_empty_graph()
        attached_points["keepLastValidOutput"].setValue(False)

        layer_id, stroke_id, point_id = self._add_stroke_point(
            painted_points,
            layer_name="Frame Layer",
            stroke_name="Frame Stroke",
            position=[0.0, 0.0, 0.0],
            seed=30,
            triangle_index=0,
        )

        self._update_layer(
            painted_points,
            layer_id,
            frameStart=10,
            frameEnd=20,
            holdOutsideRange=False,
        )
        self._update_stroke(
            painted_points,
            stroke_id,
            frameStart=12,
            frameEnd=18,
        )

        self.assertEqual(self._primitive_data(attached_points, "id", frame=11), [])

        self.assertEqual(
            self._primitive_data(attached_points, "id", frame=12), [point_id]
        )

        self.assertEqual(self._primitive_data(attached_points, "id", frame=19), [])

        self._update_stroke(painted_points, stroke_id, frameStart=0, frameEnd=0)
        self._update_layer(painted_points, layer_id, holdOutsideRange=True)

        self.assertEqual(
            self._primitive_data(attached_points, "id", frame=30), [point_id]
        )

    def testCompiledLayerTimeRangeAffectsEvaluatedOutput(self):
        _script, _plane, painted_points, attached_points = self._build_empty_graph()
        attached_points["keepLastValidOutput"].setValue(False)

        layer_id, stroke_id, point_id = self._add_stroke_point(
            painted_points,
            layer_name="Frame Layer",
            stroke_name="Frame Stroke",
            position=[0.0, 0.0, 0.0],
            seed=130,
            triangle_index=0,
        )

        self.assertEqual(painted_points.setLayerTimeRange(layer_id, 10, 20), layer_id)
        self._update_stroke(painted_points, stroke_id, frameStart=12, frameEnd=18)

        self.assertEqual(self._primitive_data(attached_points, "id", frame=11), [])
        self.assertEqual(
            self._primitive_data(attached_points, "id", frame=12), [point_id]
        )
        self.assertEqual(self._primitive_data(attached_points, "id", frame=19), [])

        layer_records = {
            int(layer["layerId"]): layer for layer in painted_points.layerRecords()
        }
        self.assertEqual(int(layer_records[layer_id]["frameStart"]), 10)
        self.assertEqual(int(layer_records[layer_id]["frameEnd"]), 20)
        self.assertTrue(layer_records[layer_id]["timeVarying"])

        self.assertEqual(painted_points.setLayerTimeRange(layer_id, 0, 0), layer_id)
        self._update_layer(painted_points, layer_id, holdOutsideRange=True)
        self._update_stroke(painted_points, stroke_id, frameStart=0, frameEnd=0)

        layer_records = {
            int(layer["layerId"]): layer for layer in painted_points.layerRecords()
        }
        self.assertEqual(int(layer_records[layer_id]["frameStart"]), 0)
        self.assertEqual(int(layer_records[layer_id]["frameEnd"]), 0)
        self.assertFalse(layer_records[layer_id]["timeVarying"])
        self.assertEqual(
            self._primitive_data(attached_points, "id", frame=30), [point_id]
        )

    def testExplicitStrokeOverrideUsesHighestOrderStroke(self):
        script, _plane, painted_points, attached_points = self._build_empty_graph()

        layer_id, stroke_a, point_a = self._add_stroke_point(
            painted_points,
            layer_name="Override Layer",
            stroke_name="Stroke A",
            position=[0.0, 0.0, 0.0],
            seed=40,
            triangle_index=0,
        )
        _layer_id, stroke_b, point_b = self._add_stroke_point(
            painted_points,
            layer_name="Override Layer",
            stroke_name="Stroke B",
            position=[0.2, 0.0, 0.2],
            seed=41,
            triangle_index=1,
        )

        self._update_stroke(painted_points, stroke_a, order=10, mode=2)
        self._update_stroke(painted_points, stroke_b, order=20, mode=2)

        self.assertEqual(
            self._primitive_data(attached_points, "id", frame=1), [point_b]
        )
        self.assertEqual(
            self._primitive_data(attached_points, "strokeId", frame=1), [stroke_b]
        )

    def testLayerOverrideUsesHighestOrderLayer(self):
        script, _plane, painted_points, attached_points = self._build_empty_graph()

        layer_a, stroke_a, point_a = self._add_stroke_point(
            painted_points,
            layer_name="Layer A",
            stroke_name="Stroke A",
            position=[0.0, 0.0, 0.0],
            seed=50,
            triangle_index=0,
        )
        layer_b, stroke_b, point_b = self._add_stroke_point(
            painted_points,
            layer_name="Layer B",
            stroke_name="Stroke B",
            position=[0.2, 0.0, 0.2],
            seed=51,
            triangle_index=1,
        )

        self._update_layer(painted_points, layer_a, order=10, mode=2)
        self._update_layer(painted_points, layer_b, order=20, mode=2)
        self._update_stroke(painted_points, stroke_a, mode=0)
        self._update_stroke(painted_points, stroke_b, mode=0)

        self.assertEqual(
            self._primitive_data(attached_points, "id", frame=1), [point_b]
        )
        self.assertEqual(
            self._primitive_data(attached_points, "strokeId", frame=1), [stroke_b]
        )

    def testStrictUnresolvedRaisesErrorAndPreservesDiagnostics(self):
        _script, _plane, painted_points, attached_points = self._build_demo_graph()

        attached_points["keepLastValidOutput"].setValue(True)
        attached_points["strictUnresolved"].setValue(True)
        healthy_ids = self._primitive_data(attached_points, "id", frame=10)
        healthy_attachment = self._primitive_data(
            attached_points, "attachmentResolved", frame=10
        )
        self.assertTrue(all(value == 1 for value in healthy_attachment))

        self.assertGreater(self._break_paths(painted_points), 0)

        broken = self._validate(attached_points, frame=11)
        self.assertIn("Using last valid output from frame 10", broken["error"])
        self.assertIn("Using last valid output from frame 10", broken["summary"])
        self.assertEqual(broken["lastValidFrame"], 10)
        self.assertEqual(healthy_ids, [1, 2])
        self.assertEqual(healthy_attachment, [1, 1])

    def testStrictUnresolvedRaisesWithoutLastValidOutput(self):
        _script, _plane, painted_points, attached_points = self._build_demo_graph()

        attached_points["keepLastValidOutput"].setValue(False)
        attached_points["strictUnresolved"].setValue(True)

        self.assertGreater(self._break_paths(painted_points), 0)

        broken = self._validate(attached_points, frame=12)
        self.assertIn("missingTargetPath", broken["error"])
        self.assertNotIn("Using last valid output", broken["summary"])
        self.assertEqual(broken["lastValidFrame"], 0)
        self.assertGreater(broken["unresolved"], 0)

    def testPythonFallbackStrictUnresolvedRaisesErrorAndPreservesDiagnostics(self):
        _script, _plane, painted_points, attached_points = self._build_demo_graph(
            attached_points_class=GafferScatterPaintCore.AttachedPoints
        )

        attached_points["keepLastValidOutput"].setValue(True)
        attached_points["strictUnresolved"].setValue(True)
        self.assertTrue(
            all(
                value == 1
                for value in self._primitive_data(
                    attached_points, "attachmentResolved", frame=30
                )
            )
        )

        self.assertGreater(self._break_paths(painted_points), 0)

        broken = self._validate(attached_points, frame=31)
        self.assertIn("Using last valid output from frame 30", broken["error"])
        self.assertIn("Using last valid output from frame 30", broken["summary"])
        self.assertEqual(broken["lastValidFrame"], 30)
        self.assertGreater(broken["unresolved"], 0)

    def testExportPresetMinimalOmitsOptionalAttributes(self):
        _script, _plane, _painted_points, attached_points = self._build_demo_graph()

        attached_points["exportPreset"].setValue(0)

        variables = self._primitive_variables(attached_points)

        self.assertIn("P", variables)
        self.assertIn("N", variables)
        self.assertIn("sourcePath", variables)
        self.assertNotIn("triangleIndex", variables)
        self.assertNotIn("barycentric", variables)
        self.assertNotIn("attachmentResolved", variables)

    def testExportPresetFullIncludesOptionalAttributes(self):
        _script, _plane, _painted_points, attached_points = self._build_demo_graph()

        attached_points["exportPreset"].setValue(1)

        variables = self._primitive_variables(attached_points)

        self.assertIn("triangleIndex", variables)
        self.assertIn("barycentric", variables)
        self.assertIn("attachmentResolved", variables)

    def testAuthoredColorPrecedenceAndDebugColorOutput(self):
        _script, _plane, painted_points, attached_points = self._build_empty_graph()

        painted_points["defaultColor"].setValue(imath.Color3f(0.1, 0.2, 0.3))

        base_layer, base_stroke, base_point = self._add_stroke_point(
            painted_points,
            layer_name="Base Layer",
            stroke_name="Base Stroke",
            position=[0.0, 0.0, 0.0],
            seed=100,
            triangle_index=0,
        )
        layer_override, layer_stroke, layer_point = self._add_stroke_point(
            painted_points,
            layer_name="Layer Override",
            stroke_name="Layer Stroke",
            position=[0.2, 0.0, 0.0],
            seed=101,
            triangle_index=0,
        )
        stroke_override_layer, stroke_override, stroke_point = self._add_stroke_point(
            painted_points,
            layer_name="Stroke Override",
            stroke_name="Stroke Color",
            position=[0.4, 0.0, 0.0],
            seed=102,
            triangle_index=0,
        )
        point_override_layer, point_override_stroke, point_override = (
            self._add_stroke_point(
                painted_points,
                layer_name="Point Override",
                stroke_name="Point Color",
                position=[0.6, 0.0, 0.0],
                seed=103,
                triangle_index=0,
            )
        )

        self._update_layer(
            painted_points,
            layer_override,
            colorEnabled=True,
            color=[0.2, 0.4, 0.6],
        )
        self._update_layer(
            painted_points,
            stroke_override_layer,
            colorEnabled=True,
            color=[0.25, 0.25, 0.25],
        )
        self._update_stroke(
            painted_points,
            stroke_override,
            colorEnabled=True,
            color=[0.7, 0.5, 0.3],
        )
        self._update_layer(
            painted_points,
            point_override_layer,
            colorEnabled=True,
            color=[0.05, 0.05, 0.05],
        )
        self._update_stroke(
            painted_points,
            point_override_stroke,
            colorEnabled=True,
            color=[0.15, 0.15, 0.15],
        )

        painted_points.mutatePoints(
            lambda point: (
                point.update({"color": [0.9, 0.1, 0.2], "colorEnabled": True})
                or int(point.get("pointId", 0)) == int(point_override)
            )
        )

        expected_by_id = {
            int(base_point): (0.1, 0.2, 0.3),
            int(layer_point): (0.2, 0.4, 0.6),
            int(stroke_point): (0.7, 0.5, 0.3),
            int(point_override): (0.9, 0.1, 0.2),
        }

        point_records = {
            int(point["pointId"]): point for point in painted_points.pointRecords()
        }
        self.assertEqual(
            set(expected_by_id.keys()),
            set(point_records.keys()),
        )
        for point_id, expected_color in expected_by_id.items():
            self._assert_colors_equal(
                [tuple(point_records[point_id]["scatterColor"])],
                [expected_color],
            )

        ids = self._primitive_data(attached_points, "id")
        scatter_colors = self._color_tuples(attached_points, "scatterColor")
        cs_colors = self._color_tuples(attached_points, "Cs")

        ordered_expected = [expected_by_id[int(point_id)] for point_id in ids]
        self._assert_colors_equal(scatter_colors, ordered_expected)
        self._assert_colors_equal(cs_colors, ordered_expected)

        attached_points["debugColor"].setValue(True)
        debug_cs = self._color_tuples(attached_points, "Cs")
        debug_expected = [(0.15, 0.9, 0.25)] * len(ids)
        self._assert_colors_equal(debug_cs, debug_expected)
        self._assert_colors_equal(
            self._color_tuples(attached_points, "scatterColor"),
            ordered_expected,
        )

    def testPythonFallbackDebugColorMatchesScatterColorContract(self):
        _script, _plane, painted_points, attached_points = self._build_empty_graph(
            attached_points_class=GafferScatterPaintCore.AttachedPoints
        )

        painted_points["defaultColor"].setValue(imath.Color3f(0.3, 0.2, 0.1))
        _layer_id, _stroke_id, point_id = self._add_stroke_point(
            painted_points,
            layer_name="Fallback Layer",
            stroke_name="Fallback Stroke",
            position=[0.0, 0.0, 0.0],
            seed=104,
            triangle_index=0,
        )

        scatter_colors = self._color_tuples(attached_points, "scatterColor")
        cs_colors = self._color_tuples(attached_points, "Cs")
        self._assert_colors_equal(scatter_colors, [(0.3, 0.2, 0.1)])
        self._assert_colors_equal(cs_colors, [(0.3, 0.2, 0.1)])

        attached_points["debugColor"].setValue(True)
        self._assert_colors_equal(
            self._color_tuples(attached_points, "scatterColor"),
            [(0.3, 0.2, 0.1)],
        )
        self._assert_colors_equal(
            self._color_tuples(attached_points, "Cs"),
            [(0.15, 0.9, 0.25)],
        )
        self.assertEqual(self._primitive_data(attached_points, "id"), [point_id])

    def testExportPresetCustomFiltersOptionalAttributes(self):
        _script, _plane, _painted_points, attached_points = self._build_demo_graph()

        attached_points["exportPreset"].setValue(2)
        attached_points["includeAttributes"].setValue(
            " triangleIndex , attachmentResolved "
        )

        variables = self._primitive_variables(attached_points)

        self.assertIn("P", variables)
        self.assertIn("triangleIndex", variables)
        self.assertIn("attachmentResolved", variables)
        self.assertNotIn("barycentric", variables)

    def testPythonFallbackExportPresetCustomFiltersOptionalAttributes(self):
        _script, _plane, _painted_points, attached_points = self._build_demo_graph(
            attached_points_class=GafferScatterPaintCore.AttachedPoints
        )

        attached_points["exportPreset"].setValue(2)
        attached_points["includeAttributes"].setValue("barycentric")

        variables = self._primitive_variables(attached_points)

        self.assertIn("P", variables)
        self.assertIn("barycentric", variables)
        self.assertNotIn("triangleIndex", variables)
        self.assertNotIn("attachmentResolved", variables)

    def testNonStrictUnresolvedKeepsCurrentFallbackOutput(self):
        script, _plane, painted_points, attached_points = self._build_demo_graph()

        attached_points["keepLastValidOutput"].setValue(True)
        attached_points["strictUnresolved"].setValue(False)
        self.assertTrue(
            all(
                value == 1
                for value in self._primitive_data(
                    attached_points, "attachmentResolved", frame=20
                )
            )
        )

        self.assertGreater(self._break_paths(painted_points), 0)

        broken = self._validate(attached_points, frame=21)
        self.assertNotIn("Using last valid output", broken["summary"])
        self.assertEqual(broken["lastValidFrame"], 20)
        self.assertTrue(
            all(
                value == 0
                for value in self._primitive_data(
                    attached_points, "attachmentResolved", frame=21
                )
            )
        )

    def testSurfaceSolveModeHybridUsesStoredObjectFallback(self):
        (
            _script,
            _plane,
            _transform,
            painted_points,
            attached_points,
        ) = self._build_transformed_plane_graph()

        _layer_id, _stroke_id, point_id = self._add_stroke_point(
            painted_points,
            layer_name="Hybrid Layer",
            stroke_name="Hybrid Stroke",
            position=[0.0, 0.0, 0.0],
            seed=60,
            triangle_index=0,
            barycentric=[0.5, 0.25, 0.25],
            source_path="/paintPlane",
        )

        def mutate(store):
            point = next(
                item
                for item in store["points"]
                if int(item.get("pointId", 0)) == int(point_id)
            )
            point["restObjectP"] = [0.25, 0.0, 0.25]
            point["restWorldP"] = [99.0, 0.0, 0.0]
            point["restNormal"] = [0.0, 1.0, 0.0]
            point["restUp"] = [0.0, 0.0, 1.0]
            point["restUV"] = [0.25, 0.25]
            point["anchorModeUsed"] = 1
            point["triangleIndex"] = 99999

        painted_points.mutateCacheStore(mutate)

        result = self._validate(attached_points, frame=1)
        self.assertFalse(result["failureReasons"])
        self.assertEqual(result["resolved"], 1)
        self.assertEqual(result["unresolved"], 0)
        self.assertEqual(
            self._primitive_data(attached_points, "attachmentResolved", frame=1), [1]
        )

        position = self._primitive_data(attached_points, "P", frame=1)[0]
        self.assertAlmostEqual(position.x, 3.25, places=5)
        self.assertAlmostEqual(position.y, 0.0, places=5)
        self.assertAlmostEqual(position.z, 0.25, places=5)

    def testCrossMeshReprojectUsesStoredFallbackOnTriangleFailure(self):
        (
            _script,
            _plane,
            _transform,
            painted_points,
            attached_points,
        ) = self._build_transformed_plane_graph()

        attached_points["allowCrossMeshReproject"].setValue(True)
        attached_points["keepLastValidOutput"].setValue(False)
        attached_points["strictUnresolved"].setValue(False)

        _layer_id, _stroke_id, point_id = self._add_stroke_point(
            painted_points,
            layer_name="Cross Mesh Layer",
            stroke_name="Cross Mesh Stroke",
            position=[0.0, 0.0, 0.0],
            seed=61,
            triangle_index=0,
            barycentric=[0.5, 0.25, 0.25],
            source_path="/paintPlane",
        )

        def mutate(store):
            point = next(
                item
                for item in store["points"]
                if int(item.get("pointId", 0)) == int(point_id)
            )
            point["restObjectP"] = [0.75, 0.0, 0.25]
            point["restWorldP"] = [77.0, 0.0, 0.0]
            point["restNormal"] = [0.0, 1.0, 0.0]
            point["restUp"] = [0.0, 0.0, 1.0]
            point["restUV"] = [0.75, 0.25]
            point["anchorModeUsed"] = 0
            point["triangleIndex"] = 99999

        painted_points.mutateCacheStore(mutate)

        result = self._validate(attached_points, frame=1)
        self.assertTrue(
            any(
                reason.startswith("missingTriangle:")
                for reason in result["failureReasons"]
            )
        )
        self.assertEqual(result["resolved"], 0)
        self.assertEqual(result["unresolved"], 1)
        self.assertEqual(
            self._primitive_data(attached_points, "attachmentResolved", frame=1), [0]
        )

        position = self._primitive_data(attached_points, "P", frame=1)[0]
        self.assertAlmostEqual(position.x, 3.75, places=5)
        self.assertAlmostEqual(position.y, 0.0, places=5)
        self.assertAlmostEqual(position.z, 0.25, places=5)

    def testTriangleFailureWithoutCrossMeshReprojectRemainsUnresolved(self):
        (
            _script,
            _plane,
            _transform,
            painted_points,
            attached_points,
        ) = self._build_transformed_plane_graph()

        attached_points["keepLastValidOutput"].setValue(False)
        attached_points["strictUnresolved"].setValue(False)

        _layer_id, _stroke_id, point_id = self._add_stroke_point(
            painted_points,
            layer_name="No Reproject Layer",
            stroke_name="No Reproject Stroke",
            position=[0.0, 0.0, 0.0],
            seed=62,
            triangle_index=0,
            barycentric=[0.5, 0.25, 0.25],
            source_path="/paintPlane",
        )

        def mutate(store):
            point = next(
                item
                for item in store["points"]
                if int(item.get("pointId", 0)) == int(point_id)
            )
            point["restObjectP"] = [0.75, 0.0, 0.25]
            point["restWorldP"] = [77.0, 0.0, 0.0]
            point["anchorModeUsed"] = 0
            point["triangleIndex"] = 99999

        painted_points.mutateCacheStore(mutate)

        result = self._validate(attached_points, frame=1)
        self.assertTrue(
            any(
                reason.startswith("missingTriangle:")
                for reason in result["failureReasons"]
            )
        )
        self.assertEqual(result["resolved"], 0)
        self.assertEqual(result["unresolved"], 1)
        self.assertEqual(
            self._primitive_data(attached_points, "attachmentResolved", frame=1), [0]
        )

    def testPerInstanceBarycentricResolveUsesInstancePath(self):
        (
            _script,
            _plane,
            _prototype,
            _instancer,
            painted_points,
            attached_points,
        ) = self._build_instanced_plane_graph()

        _layer_id, _stroke_id, point_id = self._add_stroke_point(
            painted_points,
            layer_name="Instance Layer",
            stroke_name="Instance Stroke",
            position=[0.0, 0.0, 0.0],
            seed=71,
            triangle_index=0,
            barycentric=[1.0, 0.0, 0.0],
            source_path="/paintPlane",
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
            point = next(
                item
                for item in store["points"]
                if int(item.get("pointId", 0)) == int(point_id)
            )
            point["instanceId"] = 3
            point["instanceSourcePathId"] = instance_source_path_id

        painted_points.mutateCacheStore(mutate)

        result = self._validate(attached_points, frame=1)
        self.assertFalse(result["failureReasons"])
        self.assertEqual(result["resolved"], 1)
        self.assertEqual(result["unresolved"], 0)
        self.assertEqual(
            self._primitive_data(attached_points, "sourcePath", frame=1),
            ["/paintPlane/instances/paintProto/3"],
        )

        position = self._primitive_data(attached_points, "P", frame=1)[0]
        self.assertAlmostEqual(position.x, 0.0, places=5)
        self.assertAlmostEqual(position.y, 0.0, places=5)
        self.assertAlmostEqual(position.z, 0.0, places=5)

    def testPerInstanceHybridFallbackUsesInstanceTransform(self):
        (
            _script,
            _plane,
            _prototype,
            _instancer,
            painted_points,
            attached_points,
        ) = self._build_instanced_plane_graph()

        _layer_id, _stroke_id, point_id = self._add_stroke_point(
            painted_points,
            layer_name="Instance Hybrid Layer",
            stroke_name="Instance Hybrid Stroke",
            position=[0.0, 0.0, 0.0],
            seed=72,
            triangle_index=0,
            barycentric=[0.5, 0.25, 0.25],
            source_path="/paintPlane",
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
            point = next(
                item
                for item in store["points"]
                if int(item.get("pointId", 0)) == int(point_id)
            )
            point["instanceId"] = 2
            point["instanceSourcePathId"] = instance_source_path_id
            point["restObjectP"] = [0.0, 0.0, 0.0]
            point["restWorldP"] = [55.0, 0.0, 0.0]
            point["restNormal"] = [0.0, 1.0, 0.0]
            point["restUp"] = [0.0, 0.0, 1.0]
            point["anchorModeUsed"] = 1
            point["triangleIndex"] = 99999

        painted_points.mutateCacheStore(mutate)

        result = self._validate(attached_points, frame=1)
        self.assertFalse(result["failureReasons"])
        self.assertEqual(result["resolved"], 1)
        self.assertEqual(result["unresolved"], 0)
        self.assertEqual(
            self._primitive_data(attached_points, "sourcePath", frame=1),
            ["/paintPlane/instances/paintProto/2"],
        )

        position = self._primitive_data(attached_points, "P", frame=1)[0]
        self.assertAlmostEqual(position.x, -0.5, places=5)
        self.assertAlmostEqual(position.y, 0.5, places=5)
        self.assertAlmostEqual(position.z, 0.0, places=5)

    def testMissingInstancePathFallsBackToMissingTargetFailure(self):
        (
            _script,
            _plane,
            _prototype,
            _instancer,
            painted_points,
            attached_points,
        ) = self._build_instanced_plane_graph()

        attached_points["keepLastValidOutput"].setValue(False)
        attached_points["strictUnresolved"].setValue(False)

        _layer_id, _stroke_id, point_id = self._add_stroke_point(
            painted_points,
            layer_name="Missing Instance Layer",
            stroke_name="Missing Instance Stroke",
            position=[0.0, 0.0, 0.0],
            seed=73,
            triangle_index=0,
            barycentric=[1.0, 0.0, 0.0],
            source_path="/paintPlane",
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
            point = next(
                item
                for item in store["points"]
                if int(item.get("pointId", 0)) == int(point_id)
            )
            point["instanceId"] = 99
            point["instanceSourcePathId"] = instance_source_path_id

        painted_points.mutateCacheStore(mutate)

        result = self._validate(attached_points, frame=1)
        self.assertTrue(
            any(
                reason.startswith("missingTargetPath:")
                for reason in result["failureReasons"]
            )
        )
        self.assertEqual(result["resolved"], 0)
        self.assertEqual(result["unresolved"], 1)
        self.assertEqual(
            self._primitive_data(attached_points, "sourcePath", frame=1),
            ["/paintPlane/instances/paintProto/99"],
        )
        self.assertEqual(
            self._primitive_data(attached_points, "attachmentResolved", frame=1), [0]
        )

    def testTopologyGenerationMismatchBecomesTopologyFailure(self):
        _script, _plane, painted_points, attached_points = self._build_demo_graph()

        attached_points["keepLastValidOutput"].setValue(False)

        def mutate(store):
            point = store["points"][0]
            point["topologyGeneration"] = 99
            point["triangleIndex"] = 99999

        painted_points.mutateCacheStore(mutate)

        result = self._validate(attached_points, frame=1)
        self.assertTrue(
            any(
                reason.startswith("topologyGenerationMismatch:")
                for reason in result["failureReasons"]
            )
        )
        self.assertEqual(result["resolved"], 2)
        self.assertEqual(result["unresolved"], 0)
        self.assertEqual(result["topologyMismatches"], 1)
        self.assertEqual(
            self._primitive_data(attached_points, "attachmentResolved", frame=1), [1, 1]
        )

    def testTopologyGenerationMismatchStillResolvesOnStableMeshAcrossFrames(self):
        _script, _plane, painted_points, attached_points = self._build_demo_graph()

        attached_points["keepLastValidOutput"].setValue(False)

        def mutate(store):
            point = store["points"][0]
            point["topologyGeneration"] = 99

        painted_points.mutateCacheStore(mutate)

        result = self._validate(attached_points, frame=1)
        self.assertFalse(
            any(
                reason.startswith("topologyGenerationMismatch:")
                for reason in result["failureReasons"]
            )
        )
        self.assertEqual(result["resolved"], 2)
        self.assertEqual(result["unresolved"], 0)
        self.assertEqual(result["topologyMismatches"], 0)
        self.assertEqual(
            self._primitive_data(attached_points, "attachmentResolved", frame=1), [1, 1]
        )

    def testTopologyGenerationMismatchUsesStoredFallbackWhenTriangleFails(self):
        (
            _script,
            _plane,
            _transform,
            painted_points,
            attached_points,
        ) = self._build_transformed_plane_graph()

        attached_points["keepLastValidOutput"].setValue(False)

        _layer_id, _stroke_id, point_id = self._add_stroke_point(
            painted_points,
            layer_name="Deforming Recovery Layer",
            stroke_name="Deforming Recovery Stroke",
            position=[0.0, 0.0, 0.0],
            seed=74,
            triangle_index=0,
            barycentric=[0.5, 0.25, 0.25],
            source_path="/paintPlane",
        )

        def mutate(store):
            point = next(
                item
                for item in store["points"]
                if int(item.get("pointId", 0)) == int(point_id)
            )
            point["topologyGeneration"] = 99
            point["triangleIndex"] = 99999
            point["restObjectP"] = [0.25, 0.0, 0.25]
            point["restWorldP"] = [88.0, 0.0, 0.0]
            point["restNormal"] = [0.0, 1.0, 0.0]
            point["restUp"] = [0.0, 0.0, 1.0]
            point["anchorModeUsed"] = 0

        painted_points.mutateCacheStore(mutate)

        result = self._validate(attached_points, frame=1)
        self.assertTrue(
            any(
                reason.startswith("topologyGenerationMismatch:")
                for reason in result["failureReasons"]
            )
        )
        self.assertEqual(result["resolved"], 1)
        self.assertEqual(result["unresolved"], 0)
        self.assertEqual(result["topologyMismatches"], 1)
        self.assertEqual(
            self._primitive_data(attached_points, "attachmentResolved", frame=1), [1]
        )

        position = self._primitive_data(attached_points, "P", frame=1)[0]
        self.assertAlmostEqual(position.x, 3.25, places=5)
        self.assertAlmostEqual(position.y, 0.0, places=5)
        self.assertAlmostEqual(position.z, 0.25, places=5)

    def testCacheBlobRoundTripSnapshot(self):
        _script, _plane, painted_points, _attached_points = self._build_demo_graph()

        store = painted_points.cacheSnapshot()
        point_records = painted_points.pointRecords()

        self.assertEqual(store["schemaVersion"], 2)
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
            selection = next(
                item
                for item in store["selectionSets"]
                if item["selectionSetId"] == selection_id
            )
            selection["pointIds"] = [999999]
            selection["strokeIds"] = [888888]

        painted_points.mutateCacheStore(break_selection)
        result = self._validate_cache(painted_points)

        self.assertGreater(result["topologyMismatches"], 0)
        self.assertIn("selection sets", result["summary"])


if __name__ == "__main__":
    unittest.main()
