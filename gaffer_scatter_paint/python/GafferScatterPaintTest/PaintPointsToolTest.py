import os
import pathlib
import unittest

import IECore
import imath

import Gaffer
import GafferScene
import GafferSceneUI
import GafferUITest
import GafferUI

import GafferScatterPaint
import GafferScatterPaintUI
from GafferScatterPaint import _storebridge as GafferScatterPaintStoreBridge

_REQUIRES_SCENEVIEW = unittest.skipIf(
    os.environ.get("QT_QPA_PLATFORM") == "offscreen" or not os.environ.get("DISPLAY"),
    "requires a GL-backed SceneView context",
)


class PaintPointsToolTest(GafferUITest.TestCase):
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

    def _build_overlapping_planes_graph(self):
        script = Gaffer.ScriptNode()

        front_plane = GafferScene.Plane("FrontPlane")
        front_plane["name"].setValue("frontPlane")
        script.addChild(front_plane)

        back_plane = GafferScene.Plane("BackPlane")
        back_plane["name"].setValue("backPlane")
        script.addChild(back_plane)

        group = GafferScene.Group("PaintGroup")
        script.addChild(group)
        group["name"].setValue("paintGroup")
        group["in"][0].setInput(front_plane["out"])
        group["in"][1].setInput(back_plane["out"])

        painted_points = GafferScatterPaint.PaintedPoints("PaintedPoints")
        script.addChild(painted_points)
        painted_points["in"].setInput(group["out"])

        attached_points = GafferScatterPaint.AttachedPoints("AttachedPoints")
        script.addChild(attached_points)
        attached_points["in"].setInput(group["out"])
        attached_points["points"].setInput(painted_points["out"])
        attached_points["outputLocation"].setValue("/paintGroup/scatter")

        return script, group, painted_points, attached_points

    def _front_plane_sample(self):
        return {
            "point": [0.0, 0.0, 0.0],
            "width": 0.1,
            "scale": 1.0,
            "pressureDensity": 1.0,
            "pressureSoftness": 1.0,
            "seed": 11,
            "sourcePath": "/paintGroup/frontPlane",
            "triangleIndex": 0,
            "barycentric": [0.5, 0.25, 0.25],
            "attachmentResolved": True,
            "N": [0.0, 1.0, 0.0],
            "valid": True,
        }

    def _curve_payload(self, label):
        return IECore.CompoundObject(
            {
                "label": IECore.StringData(label),
                "samples": IECore.FloatVectorData([0.0, 0.5, 1.0]),
            }
        )

    def _scene_path_string(self, path):
        if path is None:
            return None
        if isinstance(path, IECore.InternedStringVectorData):
            path = list(path)
        return "/" + "/".join(str(segment) for segment in path)

    @_REQUIRES_SCENEVIEW
    def testCompiledToolCommitAndEraseDemoStroke(self):
        script = Gaffer.ScriptNode()
        view = GafferSceneUI.SceneView(script)

        tool = GafferScatterPaintUI.PaintPointsTool(view)

        self.assertIsInstance(tool, GafferScatterPaintUI.PaintPointsTool)
        self.assertTrue(tool.view().isSame(view))

        committed = tool.commitDemoStroke()
        self.assertEqual(committed, tool["previewCount"].getValue())

        painted_points = script[tool["targetNode"].getValue()]
        self.assertIsInstance(painted_points, GafferScatterPaint.PaintedPoints)
        self.assertEqual(len(painted_points.pointRecords()), committed)

        removed = tool.eraseLastStroke()
        self.assertEqual(removed, committed)
        self.assertEqual(len(painted_points.pointRecords()), 0)

    @_REQUIRES_SCENEVIEW
    def testCompiledToolModeAndBrushPlugs(self):
        script = Gaffer.ScriptNode()
        view = GafferSceneUI.SceneView(script)

        tool = GafferScatterPaintUI.PaintPointsTool(view)

        self.assertEqual(tool["mode"].getValue(), 0)
        self.assertAlmostEqual(tool["brushSize"].getValue(), 0.1)
        self.assertEqual(tool["points"].getValue(), 1)
        self.assertAlmostEqual(tool["density"].getValue(), 1.0)
        self.assertAlmostEqual(tool["softness"].getValue(), 1.0)
        self.assertAlmostEqual(tool["spacing"].getValue(), 0.1)
        self.assertEqual(tool["rotationMode"].getValue(), 0)
        self.assertAlmostEqual(tool["scaleJitter"].getValue(), 0.0)
        self.assertAlmostEqual(tool["widthJitter"].getValue(), 0.0)
        self.assertEqual(tool["frameMode"].getValue(), 0)
        self.assertEqual(tool["frameStart"].getValue(), 0)
        self.assertEqual(tool["frameEnd"].getValue(), 0)
        self.assertFalse(tool["muteBehavior"].getValue())
        self.assertFalse(tool["soloBehavior"].getValue())
        self.assertEqual(tool["relaxObjective"].getValue(), 0)
        self.assertEqual(tool["surfaceMode"].getValue(), 0)
        self.assertEqual(tool["paintThroughMode"].getValue(), 0)
        self.assertEqual(tool["globalModePrecedence"].getValue(), 0)
        self.assertTrue(tool["pressureDefaultsEnabled"].getValue())
        self.assertEqual(tool["pressureDefaultsMappingMode"].getValue(), 0)
        self.assertIsInstance(
            tool["pressureDefaultsDensityCurve"].getValue(), IECore.CompoundObject
        )
        self.assertIsInstance(
            tool["pressureDefaultsSoftnessCurve"].getValue(), IECore.CompoundObject
        )

    @_REQUIRES_SCENEVIEW
    def testCompiledToolFrameControlsFollowContextFrame(self):
        script = Gaffer.ScriptNode()
        view = GafferSceneUI.SceneView(script)

        tool = GafferScatterPaintUI.PaintPointsTool(view)
        tool["active"].setValue(True)

        context = script.context()
        context.setFrame(37)
        self.assertEqual(tool["frameStart"].getValue(), 37)
        self.assertEqual(tool["frameEnd"].getValue(), 37)

        context.setFrame(104)
        self.assertEqual(tool["frameStart"].getValue(), 104)
        self.assertEqual(tool["frameEnd"].getValue(), 104)

        committed = tool.commitDemoStroke()
        self.assertEqual(committed, tool["previewCount"].getValue())
        self.assertIn("Paint mode committed demo stroke", tool["status"].getValue())

        painted_points = script[tool["targetNode"].getValue()]
        self.assertEqual(len(painted_points.pointRecords()), committed)

        tool["mode"].setValue(1)
        removed = tool.commitDemoStroke()
        self.assertEqual(removed, committed)
        self.assertEqual(len(painted_points.pointRecords()), 0)
        self.assertIn("Erase mode removed", tool["status"].getValue())

    @_REQUIRES_SCENEVIEW
    def testCompiledToolRelaxModeUsesCurrentSelection(self):
        script, _plane, painted_points, attached_points = self._build_empty_graph()

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
        middle_point = point_records[1]
        painted_points.setCurrentSelection([middle_point["pointId"]], [])

        view = GafferSceneUI.SceneView(script)
        view["in"].setInput(attached_points["out"])
        tool = GafferScatterPaintUI.PaintPointsTool(view)
        tool["mode"].setValue(4)

        context = script.context()
        context.setFrame(29)
        with context:
            updated_count = tool.commitDemoStroke()

        self.assertEqual(updated_count, 1)
        updated_middle = next(
            point
            for point in painted_points.pointRecords()
            if int(point["pointId"]) == int(middle_point["pointId"])
        )
        self.assertEqual(updated_middle["lastValidFrame"], 29)
        self.assertEqual(updated_middle["topologyGeneration"], 29)
        self.assertTrue(updated_middle["attachmentResolved"])
        self.assertNotEqual(updated_middle["P"], middle_point["P"])
        self.assertIn("Relax updated 1 points", tool["status"].getValue())

    @_REQUIRES_SCENEVIEW
    def testCompiledToolReprojectModeUsesCurrentSelection(self):
        (
            script,
            _plane,
            _transform,
            painted_points,
            attached_points,
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

        view = GafferSceneUI.SceneView(script)
        view["in"].setInput(attached_points["out"])
        tool = GafferScatterPaintUI.PaintPointsTool(view)
        tool["mode"].setValue(5)

        context = script.context()
        context.setFrame(7)
        with context:
            updated_count = tool.commitDemoStroke()

        self.assertEqual(updated_count, 1)
        point = next(
            item
            for item in painted_points.pointRecords()
            if int(item["pointId"]) == int(point_id)
        )
        self.assertEqual(point["sourcePath"], "/paintPlane")
        self.assertIn(point["triangleIndex"], [0, 1])
        self.assertAlmostEqual(sum(point["barycentric"]), 1.0, places=5)
        self.assertTrue(all(value >= 0.0 for value in point["barycentric"]))
        self.assertEqual(point["lastValidFrame"], 7)
        self.assertEqual(point["topologyGeneration"], 7)
        self.assertTrue(point["attachmentResolved"])
        self.assertAlmostEqual(point["restObjectP"][0], 0.25, places=5)
        self.assertAlmostEqual(point["P"][0], point["restObjectP"][0] + 3.0, places=5)
        self.assertIn("Reproject updated 1 points", tool["status"].getValue())

    def testSelectBrushMetadataUsesCurrentSelectionModel(self):
        script = Gaffer.ScriptNode()
        script["plane"] = GafferScene.Plane()
        script["plane"]["name"].setValue("paintPlane")
        script["paintedPoints"] = GafferScatterPaint.PaintedPoints()
        script["paintedPoints"]["in"].setInput(script["plane"]["out"])

        stroke_id = script["paintedPoints"].ensureStroke(
            script["paintedPoints"].ensureLayer("Layer 1"),
            "Stroke 1",
        )
        script["paintedPoints"].paintStrokeCommit(
            stroke_id,
            [
                {
                    "P": [0.0, 0.01, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 0,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                },
                {
                    "P": [10.0, 0.01, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 1,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 1,
                    "barycentric": [0.25, 0.5, 0.25],
                    "attachmentResolved": True,
                },
            ],
            append=False,
        )

        point_records = script["paintedPoints"].pointRecords()
        selected = script["paintedPoints"].setCurrentSelection(
            [point_records[0]["pointId"]],
            [point_records[0]["strokeId"]],
        )

        self.assertEqual(selected["pointIds"], [point_records[0]["pointId"]])
        self.assertEqual(selected["strokeIds"], [point_records[0]["strokeId"]])

    def testSelectLassoSelectionModelSupportsMultiPointCurrentSelection(self):
        script = Gaffer.ScriptNode()
        script["plane"] = GafferScene.Plane()
        script["plane"]["name"].setValue("paintPlane")
        script["paintedPoints"] = GafferScatterPaint.PaintedPoints()
        script["paintedPoints"]["in"].setInput(script["plane"]["out"])

        stroke_a = script["paintedPoints"].ensureStroke(
            script["paintedPoints"].ensureLayer("Layer 1"),
            "Stroke A",
        )
        stroke_b = script["paintedPoints"].ensureStroke(
            script["paintedPoints"].ensureLayer("Layer 1"),
            "Stroke B",
        )

        script["paintedPoints"].paintStrokeCommit(
            stroke_a,
            [
                {
                    "P": [0.0, 0.01, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 10,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                },
                {
                    "P": [0.1, 0.01, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 11,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 1,
                    "barycentric": [0.25, 0.5, 0.25],
                    "attachmentResolved": True,
                },
            ],
            append=False,
        )
        script["paintedPoints"].paintStrokeCommit(
            stroke_b,
            [
                {
                    "P": [5.0, 0.01, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 12,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                }
            ],
            append=False,
        )

        point_records = script["paintedPoints"].pointRecords()
        selected = script["paintedPoints"].setCurrentSelection(
            [point_records[0]["pointId"], point_records[1]["pointId"]],
            [point_records[0]["strokeId"]],
        )

        self.assertEqual(
            selected["pointIds"],
            [point_records[0]["pointId"], point_records[1]["pointId"]],
        )
        self.assertEqual(selected["strokeIds"], [point_records[0]["strokeId"]])

    def testMetadataDeclaresRelaxAndReprojectModes(self):
        metadata_path = (
            pathlib.Path(__file__).resolve().parents[2]
            / "startup"
            / "GafferScatterPaintUI"
            / "metadata.py"
        )
        metadata_text = metadata_path.read_text()

        self.assertIn('"preset:Relax"', metadata_text)
        self.assertIn('"preset:Reproject"', metadata_text)
        self.assertIn("Relax/Reproject selection actions", metadata_text)
        self.assertIn('"preset:Preserve Silhouette"', metadata_text)
        self.assertIn('"preset:Even Redistribution"', metadata_text)
        self.assertIn('"preset:SplitBySelection"', metadata_text)
        self.assertIn('"preset:Move"', metadata_text)
        self.assertIn('"preset:SetMode"', metadata_text)
        self.assertIn('"preset:Visible Space"', metadata_text)
        self.assertIn('"preset:Attachment Space"', metadata_text)
        self.assertIn('"preset:Persistent"', metadata_text)
        self.assertIn('"preset:Additive"', metadata_text)
        self.assertIn('"preset:Override"', metadata_text)
        self.assertIn("Top-level HUD frame mode affordance", metadata_text)
        self.assertIn("Top-level HUD frame start affordance", metadata_text)
        self.assertIn("Top-level HUD frame end affordance", metadata_text)
        self.assertIn("Top-level HUD mute behavior affordance", metadata_text)
        self.assertIn("Top-level HUD solo behavior affordance", metadata_text)
        self.assertIn('"nodeToolbar:bottom:type"', metadata_text)
        self.assertIn('"GafferUI.StandardNodeToolbar.bottom"', metadata_text)
        self.assertIn('"toolbarLayout:section"', metadata_text)
        self.assertIn('"toolbarLayout:width"', metadata_text)
        self.assertIn('"label"', metadata_text)
        self.assertIn('"Size"', metadata_text)
        self.assertIn('"Points"', metadata_text)
        self.assertIn('"Density"', metadata_text)
        self.assertIn('"Softness"', metadata_text)
        self.assertIn('"Spacing"', metadata_text)
        self.assertIn("frameMode", metadata_text)
        self.assertIn("frameStart", metadata_text)
        self.assertIn("frameEnd", metadata_text)
        self.assertIn("muteBehavior", metadata_text)
        self.assertIn("soloBehavior", metadata_text)
        self.assertIn("visible/world brush space", metadata_text)
        self.assertIn("marquee subset selection", metadata_text)
        self.assertIn("pressure-aware Paint", metadata_text)
        self.assertIn("pressureDefaultsDensityCurve", metadata_text)
        self.assertIn("pressureDefaultsSoftnessCurve", metadata_text)
        self.assertIn("brush Erase space behavior", metadata_text)
        self.assertIn("layerMoveToIndex", metadata_text)
        self.assertIn("layerMode", metadata_text)
        self.assertIn("pressureValue", metadata_text)
        self.assertIn("eraseSpace", metadata_text)
        self.assertIn("brushDefaults.points", metadata_text)
        self.assertIn("Mirrors PaintedPoints.targetFilter", metadata_text)
        self.assertIn("Mirrors PaintedPoints.brushDefaults.points", metadata_text)
        pressure_index = metadata_text.index('"pressureValue": [')
        rotation_index = metadata_text.index('"rotationMode": [')
        scale_jitter_index = metadata_text.index('"scaleJitter": [')
        width_jitter_index = metadata_text.index('"widthJitter": [')
        preview_index = metadata_text.index('"previewCount": [')
        self.assertLess(pressure_index, rotation_index)
        self.assertLess(rotation_index, scale_jitter_index)
        self.assertLess(scale_jitter_index, width_jitter_index)
        self.assertLess(width_jitter_index, preview_index)
        self.assertIn(
            '"rotationMode": [\n            "description",\n            '
            '"Mirrors PaintedPoints.brushDefaults.rotationMode so the tool reuses the brush rotation preset widget.",\n            '
            '"preset:None",\n            0,\n            "preset:Per-Point",\n            1,\n            "preset:Random",\n            2,\n            '
            '"plugValueWidget:type",\n            "GafferUI.PresetsPlugValueWidget",\n            "label",\n            "Rotation",\n            '
            '"toolbarLayout:section",\n            "Right"',
            metadata_text,
        )
        self.assertIn(
            '"scaleJitter": [\n            "description",\n            '
            '"Mirrors PaintedPoints.brushDefaults.scaleJitter so the tool exposes the random scale variation control on the active node.",\n            '
            '"label",\n            "Scale Jitter",\n            "toolbarLayout:section",\n            "Right"',
            metadata_text,
        )
        self.assertIn(
            '"widthJitter": [\n            "description",\n            '
            '"Mirrors PaintedPoints.brushDefaults.widthJitter so the tool exposes the random width variation control on the active node.",\n            '
            '"label",\n            "Width Jitter",\n            "toolbarLayout:section",\n            "Right"',
            metadata_text,
        )
        self.assertIn(
            "Mirrors PaintedPoints.pressureDefaults.mappingMode", metadata_text
        )

    @_REQUIRES_SCENEVIEW
    def testCompiledToolStrokeEditSplitUsesCurrentSelection(self):
        script, _plane, painted_points, attached_points = self._build_empty_graph()

        layer_id = painted_points.ensureLayer("Layer 1")
        stroke_id = painted_points.ensureStroke(layer_id, "Stroke 1")
        painted_points.paintStrokeCommit(
            stroke_id,
            [
                {
                    "P": [0.0, 0.01, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 100,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                },
                {
                    "P": [0.1, 0.01, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 101,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 1,
                    "barycentric": [0.25, 0.5, 0.25],
                    "attachmentResolved": True,
                },
                {
                    "P": [0.2, 0.01, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 102,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                },
            ],
            append=False,
        )

        point_records = painted_points.pointRecords()
        selected_point = point_records[1]
        painted_points.setCurrentSelection([selected_point["pointId"]], [stroke_id])

        view = GafferSceneUI.SceneView(script)
        view["in"].setInput(attached_points["out"])
        tool = GafferScatterPaintUI.PaintPointsTool(view)
        tool["mode"].setValue(7)
        tool["targetNode"].setValue(painted_points.relativeName(script))
        tool["strokeEditAction"].setValue(4)

        changed = tool.commitDemoStroke()

        self.assertEqual(changed, 1)
        stroke_records = painted_points.strokeRecords()
        self.assertEqual(len(stroke_records), 2)
        self.assertNotIn(stroke_id, {record["strokeId"] for record in stroke_records})
        self.assertEqual(len(painted_points.pointRecords()), 2)
        self.assertIn("StrokeEdit Removed 1 selected points", tool["status"].getValue())

    @_REQUIRES_SCENEVIEW
    def testCompiledToolStrokeEditRenameUsesCurrentSelection(self):
        script, _plane, painted_points, attached_points = self._build_empty_graph()

        layer_id = painted_points.ensureLayer("Layer 1")
        stroke_id = painted_points.ensureStroke(layer_id, "Stroke 1")
        painted_points.paintStrokeCommit(
            stroke_id,
            [
                {
                    "P": [0.0, 0.01, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 110,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                }
            ],
            append=False,
        )

        point_record = painted_points.pointRecords()[0]
        painted_points.setCurrentSelection([point_record["pointId"]], [stroke_id])

        view = GafferSceneUI.SceneView(script)
        view["in"].setInput(attached_points["out"])
        tool = GafferScatterPaintUI.PaintPointsTool(view)
        tool["mode"].setValue(7)
        tool["targetNode"].setValue(painted_points.relativeName(script))
        tool["strokeEditAction"].setValue(0)
        tool["strokeName"].setValue("Renamed Stroke")

        changed = tool.commitDemoStroke()

        self.assertEqual(changed, 1)
        stroke_record = next(
            item for item in painted_points.strokeRecords() if item["strokeId"] == stroke_id
        )
        self.assertEqual(stroke_record["name"], "Renamed Stroke")
        self.assertIn("StrokeEdit renamed stroke", tool["status"].getValue())

    @_REQUIRES_SCENEVIEW
    def testCompiledToolMirrorsNodeBackedControlsToTargetNode(self):
        script, _plane, painted_points, attached_points = self._build_empty_graph()
        density_curve = self._curve_payload("node density")
        softness_curve = self._curve_payload("node softness")

        view = GafferSceneUI.SceneView(script)
        view["in"].setInput(attached_points["out"])
        tool = GafferScatterPaintUI.PaintPointsTool(view)
        tool["targetNode"].setValue(painted_points.relativeName(script))

        painted_points["targetFilter"].setValue("/paintPlane")
        painted_points["targetSetFilter"].setValue("heroSet")
        painted_points["brushDefaults"]["points"].setValue(4)
        painted_points["brushDefaults"]["rotationMode"].setValue(2)
        painted_points["brushDefaults"]["scaleJitter"].setValue(0.35)
        painted_points["brushDefaults"]["widthJitter"].setValue(0.15)
        painted_points["surfaceMode"].setValue(1)
        painted_points["paintThroughMode"].setValue(1)
        painted_points["relaxObjective"].setValue(1)
        painted_points["globalModePrecedence"].setValue(1)
        painted_points["pressureDefaults"]["enabled"].setValue(False)
        painted_points["pressureDefaults"]["mappingMode"].setValue(1)
        painted_points["pressureDefaults"]["densityCurve"].setValue(density_curve)
        painted_points["pressureDefaults"]["softnessCurve"].setValue(softness_curve)

        tool["targetNode"].setValue(painted_points.relativeName(script))

        self.assertEqual(tool["targetFilter"].getValue(), "/paintPlane")
        self.assertEqual(tool["targetSetFilter"].getValue(), "heroSet")
        self.assertEqual(tool["points"].getValue(), 4)
        self.assertEqual(tool["rotationMode"].getValue(), 2)
        self.assertAlmostEqual(tool["scaleJitter"].getValue(), 0.35)
        self.assertAlmostEqual(tool["widthJitter"].getValue(), 0.15)
        self.assertEqual(tool["surfaceMode"].getValue(), 1)
        self.assertEqual(tool["paintThroughMode"].getValue(), 1)
        self.assertEqual(tool["relaxObjective"].getValue(), 1)
        self.assertEqual(tool["globalModePrecedence"].getValue(), 1)
        self.assertFalse(tool["pressureDefaultsEnabled"].getValue())
        self.assertEqual(tool["pressureDefaultsMappingMode"].getValue(), 1)
        self.assertEqual(
            tool["pressureDefaultsDensityCurve"].getValue()["label"].value,
            "node density",
        )
        self.assertEqual(
            tool["pressureDefaultsSoftnessCurve"].getValue()["label"].value,
            "node softness",
        )

        tool["targetFilter"].setValue("/paintPlane/scatter")
        tool["targetSetFilter"].setValue("scatterSet")
        tool["points"].setValue(6)
        tool["rotationMode"].setValue(1)
        tool["scaleJitter"].setValue(0.55)
        tool["widthJitter"].setValue(0.25)
        tool["surfaceMode"].setValue(0)
        tool["paintThroughMode"].setValue(0)
        tool["relaxObjective"].setValue(0)
        tool["globalModePrecedence"].setValue(0)
        tool["pressureDefaultsEnabled"].setValue(True)
        tool["pressureDefaultsMappingMode"].setValue(0)
        tool["pressureDefaultsDensityCurve"].setValue(
            self._curve_payload("tool density")
        )
        tool["pressureDefaultsSoftnessCurve"].setValue(
            self._curve_payload("tool softness")
        )

        self.assertEqual(
            painted_points["targetFilter"].getValue(), "/paintPlane/scatter"
        )
        self.assertEqual(painted_points["targetSetFilter"].getValue(), "scatterSet")
        self.assertEqual(painted_points["brushDefaults"]["points"].getValue(), 6)
        self.assertEqual(painted_points["brushDefaults"]["rotationMode"].getValue(), 1)
        self.assertAlmostEqual(
            painted_points["brushDefaults"]["scaleJitter"].getValue(), 0.55
        )
        self.assertAlmostEqual(
            painted_points["brushDefaults"]["widthJitter"].getValue(), 0.25
        )
        self.assertEqual(painted_points["surfaceMode"].getValue(), 0)
        self.assertEqual(painted_points["paintThroughMode"].getValue(), 0)
        self.assertEqual(painted_points["relaxObjective"].getValue(), 0)
        self.assertEqual(painted_points["globalModePrecedence"].getValue(), 0)
        self.assertTrue(painted_points["pressureDefaults"]["enabled"].getValue())
        self.assertEqual(
            painted_points["pressureDefaults"]["mappingMode"].getValue(), 0
        )
        self.assertEqual(
            painted_points["pressureDefaults"]["densityCurve"]
            .getValue()["label"]
            .value,
            "tool density",
        )
        self.assertEqual(
            painted_points["pressureDefaults"]["softnessCurve"]
            .getValue()["label"]
            .value,
            "tool softness",
        )

    @_REQUIRES_SCENEVIEW
    def testTopLevelHudAffordancesPropagateToLayerEditControls(self):
        script = Gaffer.ScriptNode()
        view = GafferSceneUI.SceneView(script)
        tool = GafferScatterPaintUI.PaintPointsTool(view)

        tool["frameMode"].setValue(2)
        self.assertEqual(tool["layerMode"].getValue(), 2)

        tool["layerMode"].setValue(1)
        self.assertEqual(tool["frameMode"].getValue(), 1)

        tool["frameStart"].setValue(1001)
        self.assertEqual(tool["layerFrameStart"].getValue(), 1001)

        tool["layerFrameStart"].setValue(1002)
        self.assertEqual(tool["frameStart"].getValue(), 1002)

        tool["frameEnd"].setValue(1010)
        self.assertEqual(tool["layerFrameEnd"].getValue(), 1010)

        tool["layerFrameEnd"].setValue(1011)
        self.assertEqual(tool["frameEnd"].getValue(), 1011)

        tool["muteBehavior"].setValue(True)
        self.assertTrue(tool["layerMute"].getValue())

        tool["layerMute"].setValue(False)
        self.assertFalse(tool["muteBehavior"].getValue())

        tool["soloBehavior"].setValue(True)
        self.assertTrue(tool["layerSolo"].getValue())

        tool["layerSolo"].setValue(False)
        self.assertFalse(tool["soloBehavior"].getValue())

    def testLayerAndStrokeEditBackendsSupportSelectionDrivenRename(self):
        script = Gaffer.ScriptNode()
        script["plane"] = GafferScene.Plane()
        script["plane"]["name"].setValue("paintPlane")
        script["paintedPoints"] = GafferScatterPaint.PaintedPoints()
        script["paintedPoints"]["in"].setInput(script["plane"]["out"])

        layer_id = script["paintedPoints"].ensureLayer("Layer 1")
        stroke_id = script["paintedPoints"].ensureStroke(layer_id, "Stroke 1")
        script["paintedPoints"].paintStrokeCommit(
            stroke_id,
            [
                {
                    "P": [0.0, 0.01, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 20,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                }
            ],
            append=False,
        )

        point_records = script["paintedPoints"].pointRecords()
        selected = script["paintedPoints"].setCurrentSelection(
            [point_records[0]["pointId"]],
            [point_records[0]["strokeId"]],
        )
        self.assertEqual(selected["strokeIds"], [stroke_id])

        self.assertEqual(
            script["paintedPoints"].renameLayer(layer_id, "Renamed Layer"), layer_id
        )
        self.assertEqual(
            script["paintedPoints"].renameStroke(stroke_id, "Renamed Stroke"), stroke_id
        )

        layer_records = {
            item["layerId"]: item for item in script["paintedPoints"].layerRecords()
        }
        stroke_records = {
            item["strokeId"]: item for item in script["paintedPoints"].strokeRecords()
        }
        self.assertEqual(layer_records[layer_id]["name"], "Renamed Layer")
        self.assertEqual(stroke_records[stroke_id]["name"], "Renamed Stroke")

    def testLayerEditBackendSupportsLayerStateActions(self):
        script = Gaffer.ScriptNode()
        script["plane"] = GafferScene.Plane()
        script["plane"]["name"].setValue("paintPlane")
        script["paintedPoints"] = GafferScatterPaint.PaintedPoints()
        script["paintedPoints"]["in"].setInput(script["plane"]["out"])

        layer_id = script["paintedPoints"].ensureLayer("Layer 1")
        stroke_id = script["paintedPoints"].ensureStroke(layer_id, "Stroke 1")
        script["paintedPoints"].paintStrokeCommit(
            stroke_id,
            [
                {
                    "P": [0.0, 0.01, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 30,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                }
            ],
            append=False,
        )

        point_records = script["paintedPoints"].pointRecords()
        selected = script["paintedPoints"].setCurrentSelection(
            [point_records[0]["pointId"]],
            [point_records[0]["strokeId"]],
        )
        self.assertEqual(selected["strokeIds"], [stroke_id])

        self.assertEqual(
            script["paintedPoints"].setLayerVisible(layer_id, False), layer_id
        )
        self.assertEqual(script["paintedPoints"].setLayerMute(layer_id, True), layer_id)
        self.assertEqual(script["paintedPoints"].setLayerSolo(layer_id, True), layer_id)
        self.assertEqual(
            script["paintedPoints"].setLayerTimeRange(layer_id, 10, 20), layer_id
        )

        layer_records = {
            item["layerId"]: item for item in script["paintedPoints"].layerRecords()
        }
        self.assertFalse(layer_records[layer_id]["visible"])
        self.assertTrue(layer_records[layer_id]["mute"])
        self.assertTrue(layer_records[layer_id]["solo"])
        self.assertEqual(layer_records[layer_id]["frameStart"], 10)
        self.assertEqual(layer_records[layer_id]["frameEnd"], 20)
        self.assertTrue(layer_records[layer_id]["timeVarying"])

    def testLayerEditMetadataStateActionModelMatchesBackendShape(self):
        script = Gaffer.ScriptNode()
        script["plane"] = GafferScene.Plane()
        script["plane"]["name"].setValue("paintPlane")
        script["paintedPoints"] = GafferScatterPaint.PaintedPoints()
        script["paintedPoints"]["in"].setInput(script["plane"]["out"])

        layer_id = script["paintedPoints"].ensureLayer("Layer 1")
        stroke_id = script["paintedPoints"].ensureStroke(layer_id, "Stroke 1")
        script["paintedPoints"].paintStrokeCommit(
            stroke_id,
            [
                {
                    "P": [0.0, 0.01, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 31,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                }
            ],
            append=False,
        )

        point_record = script["paintedPoints"].pointRecords()[0]
        selected = script["paintedPoints"].setCurrentSelection(
            [point_record["pointId"]],
            [point_record["strokeId"]],
        )
        self.assertEqual(selected["strokeIds"], [stroke_id])

        script["paintedPoints"].setLayerVisible(layer_id, False)
        script["paintedPoints"].setLayerMute(layer_id, True)
        script["paintedPoints"].setLayerSolo(layer_id, False)
        script["paintedPoints"].setLayerTimeRange(layer_id, 5, 15)

        layer_record = next(
            item
            for item in script["paintedPoints"].layerRecords()
            if item["layerId"] == layer_id
        )
        self.assertFalse(layer_record["visible"])
        self.assertTrue(layer_record["mute"])
        self.assertFalse(layer_record["solo"])
        self.assertEqual(layer_record["frameStart"], 5)
        self.assertEqual(layer_record["frameEnd"], 15)
        self.assertTrue(layer_record["timeVarying"])

    @_REQUIRES_SCENEVIEW
    def testCompiledToolDemoStrokeStoresPressureFields(self):
        script = Gaffer.ScriptNode()
        view = GafferSceneUI.SceneView(script)

        tool = GafferScatterPaintUI.PaintPointsTool(view)
        tool["pressureDefaultsEnabled"].setValue(True)
        tool["pressureDefaultsMappingMode"].setValue(1)
        tool["pressureValue"].setValue(0.5)
        tool["density"].setValue(0.8)
        tool["softness"].setValue(0.6)

        committed = tool.commitDemoStroke()

        self.assertEqual(committed, tool["previewCount"].getValue())
        painted_points = script[tool["targetNode"].getValue()]
        point_records = painted_points.cacheSnapshot()["points"]
        self.assertEqual(len(point_records), committed)
        for point in point_records:
            self.assertAlmostEqual(point["pressureDensity"], 0.2)
            self.assertAlmostEqual(point["pressureSoftness"], 0.15)

    @_REQUIRES_SCENEVIEW
    def testCompiledToolDemoStrokeStoresRotationAndJitterFields(self):
        script = Gaffer.ScriptNode()
        view = GafferSceneUI.SceneView(script)

        tool = GafferScatterPaintUI.PaintPointsTool(view)
        tool["previewCount"].setValue(4)
        tool["rotationMode"].setValue(2)
        tool["scaleJitter"].setValue(0.5)
        tool["widthJitter"].setValue(0.25)

        committed = tool.commitDemoStroke()

        self.assertEqual(committed, 4)
        painted_points = script[tool["targetNode"].getValue()]
        point_records = painted_points.cacheSnapshot()["points"]
        self.assertEqual(len(point_records), committed)
        self.assertTrue(any(abs(point["normalSpin"]) > 1e-6 for point in point_records))
        self.assertTrue(any(abs(point["uniformScale"] - 1.0) > 1e-6 for point in point_records))
        self.assertTrue(any(abs(point["width"] - tool["brushSize"].getValue()) > 1e-6 for point in point_records))

    def testNativeBrushCommitStoresVariationAndAffectsAttachedOrient(self):
        _script, _plane, painted_points, attached_points = self._build_empty_graph()

        layer_id = painted_points.ensureLayer("Variation Layer")
        stroke_id = painted_points.ensureStroke(layer_id, "Variation Stroke")

        committed = painted_points.brushPaintCommitNative(
            stroke_id,
            [
                {
                    "point": [0.0, 0.0, 0.0],
                    "N": [0.0, 1.0, 0.0],
                    "barycentric": [0.5, 0.25, 0.25],
                    "width": 0.125,
                    "scale": 1.35,
                    "normalSpin": 1.25,
                    "tangentRotation": [0.2, -0.15],
                    "seed": 99,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "attachmentResolved": True,
                    "valid": True,
                }
            ],
            True,
        )

        self.assertEqual(committed["committed"], 1)
        point_records = painted_points.cacheSnapshot()["points"]
        self.assertEqual(len(point_records), 1)
        point = point_records[0]
        self.assertAlmostEqual(point["width"], 0.125)
        self.assertAlmostEqual(point["uniformScale"], 1.35)
        self.assertAlmostEqual(point["normalSpin"], 1.25)
        self.assertEqual(len(point["tangentRotation"]), 2)
        self.assertAlmostEqual(point["tangentRotation"][0], 0.2)
        self.assertAlmostEqual(point["tangentRotation"][1], -0.15)

        scatter = attached_points["out"].object("/paintPlane/scatter")
        orient = scatter["orient"].data[0]
        self.assertGreater(abs(orient.r() - 1.0) + orient.v().length(), 1e-6)

    @_REQUIRES_SCENEVIEW
    def testCompiledToolLayerEditMoveUsesCurrentSelection(self):
        script, _plane, painted_points, attached_points = self._build_empty_graph()

        layer_a = painted_points.ensureLayer("Layer A")
        layer_b = painted_points.ensureLayer("Layer B")
        stroke_id = painted_points.ensureStroke(layer_b, "Stroke B")
        painted_points.paintStrokeCommit(
            stroke_id,
            [
                {
                    "P": [0.0, 0.01, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 60,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                }
            ],
            append=False,
        )

        point_record = painted_points.pointRecords()[0]
        painted_points.setCurrentSelection([point_record["pointId"]], [stroke_id])

        view = GafferSceneUI.SceneView(script)
        view["in"].setInput(attached_points["out"])
        tool = GafferScatterPaintUI.PaintPointsTool(view)
        tool["mode"].setValue(6)
        tool["targetNode"].setValue(painted_points.relativeName(script))
        tool["layerEditAction"].setValue(5)
        tool["layerMoveToIndex"].setValue(0)

        changed = tool.commitDemoStroke()

        self.assertEqual(changed, 1)
        layer_records = painted_points.layerRecords()
        self.assertEqual(layer_records[0]["layerId"], layer_b)
        self.assertEqual(layer_records[1]["layerId"], layer_a)
        self.assertIn("LayerEdit moved layer", tool["status"].getValue())

    @_REQUIRES_SCENEVIEW
    def testCompiledToolLayerEditSetModeUsesCurrentSelection(self):
        script, _plane, painted_points, attached_points = self._build_empty_graph()

        layer_id = painted_points.ensureLayer("Mode Layer")
        stroke_id = painted_points.ensureStroke(layer_id, "Mode Stroke")
        painted_points.paintStrokeCommit(
            stroke_id,
            [
                {
                    "P": [0.0, 0.01, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 61,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                }
            ],
            append=False,
        )

        point_record = painted_points.pointRecords()[0]
        painted_points.setCurrentSelection([point_record["pointId"]], [stroke_id])

        view = GafferSceneUI.SceneView(script)
        view["in"].setInput(attached_points["out"])
        tool = GafferScatterPaintUI.PaintPointsTool(view)
        tool["mode"].setValue(6)
        tool["targetNode"].setValue(painted_points.relativeName(script))
        tool["layerEditAction"].setValue(6)
        tool["layerMode"].setValue(2)

        changed = tool.commitDemoStroke()

        self.assertEqual(changed, 1)
        layer_record = next(
            item
            for item in painted_points.layerRecords()
            if item["layerId"] == layer_id
        )
        stroke_record = next(
            item
            for item in painted_points.strokeRecords()
            if item["strokeId"] == stroke_id
        )
        self.assertEqual(layer_record["mode"], 2)
        self.assertEqual(stroke_record["mode"], 2)
        self.assertIn("LayerEdit set mode Override", tool["status"].getValue())

    def testStrokeEditBackendSupportsDeleteMoveAndMergeActions(self):
        script = Gaffer.ScriptNode()
        script["plane"] = GafferScene.Plane()
        script["plane"]["name"].setValue("paintPlane")
        script["paintedPoints"] = GafferScatterPaint.PaintedPoints()
        script["paintedPoints"]["in"].setInput(script["plane"]["out"])

        layer_id = script["paintedPoints"].ensureLayer("Layer 1")
        stroke_a = script["paintedPoints"].ensureStroke(layer_id, "Stroke A")
        stroke_b = script["paintedPoints"].ensureStroke(layer_id, "Stroke B")
        stroke_c = script["paintedPoints"].ensureStroke(layer_id, "Stroke C")

        script["paintedPoints"].paintStrokeCommit(
            stroke_a,
            [
                {
                    "P": [0.0, 0.01, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 40,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                }
            ],
            append=False,
        )
        script["paintedPoints"].paintStrokeCommit(
            stroke_b,
            [
                {
                    "P": [0.2, 0.01, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 41,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 1,
                    "barycentric": [0.25, 0.5, 0.25],
                    "attachmentResolved": True,
                }
            ],
            append=False,
        )
        script["paintedPoints"].paintStrokeCommit(
            stroke_c,
            [
                {
                    "P": [0.4, 0.01, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 42,
                    "sourcePath": "/paintPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                }
            ],
            append=False,
        )

        self.assertEqual(script["paintedPoints"].moveStroke(stroke_c, 0), stroke_c)
        stroke_records = {
            item["strokeId"]: item for item in script["paintedPoints"].strokeRecords()
        }
        self.assertEqual(stroke_records[stroke_c]["order"], 0)

        self.assertEqual(
            script["paintedPoints"].mergeStrokes(stroke_b, stroke_a), stroke_a
        )
        stroke_records = {
            item["strokeId"]: item for item in script["paintedPoints"].strokeRecords()
        }
        self.assertNotIn(stroke_b, stroke_records)
        point_records = script["paintedPoints"].pointRecords()
        merged_points = [item for item in point_records if item["strokeId"] == stroke_a]
        self.assertEqual(len(merged_points), 2)

        self.assertEqual(script["paintedPoints"].deleteStroke(stroke_c), stroke_c)
        stroke_records = {
            item["strokeId"]: item for item in script["paintedPoints"].strokeRecords()
        }
        self.assertNotIn(stroke_c, stroke_records)

    @_REQUIRES_SCENEVIEW
    def testCompiledToolClickPlacementFromSceneHit(self):
        script = Gaffer.ScriptNode()
        script["plane"] = GafferScene.Plane()

        view = GafferSceneUI.SceneView(script)
        view["in"].setInput(script["plane"]["out"])
        tool = GafferScatterPaintUI.PaintPointsTool(view)

        with GafferUI.Window() as window:
            gadget_widget = GafferUI.GadgetWidget(view.viewportGadget())

        window.setVisible(True)
        self.waitForIdle(1000)

        scene_gadget = view.viewportGadget().getPrimaryChild()
        scene_gadget.waitForCompletion()
        view.viewportGadget().frame(scene_gadget.bound())
        self.waitForIdle(1000)

        raster_position = imath.V2f(view.viewportGadget().getViewport()) * 0.5
        gadget_line = view.viewportGadget().rasterToGadgetSpace(
            raster_position, scene_gadget
        )
        path = scene_gadget.objectAt(gadget_line)
        self.assertEqual(self._scene_path_string(path), "/plane")

        event = GafferUI.ButtonEvent(
            GafferUI.ButtonEvent.Buttons.Left,
            GafferUI.ButtonEvent.Buttons.Left,
            IECore.LineSegment3f(gadget_line.p0, gadget_line.p1),
        )

        handled = view.viewportGadget().buttonPressSignal()(
            view.viewportGadget(), event
        )
        self.assertTrue(handled)

        drag_event = GafferUI.DragDropEvent()
        drag_event.button = GafferUI.ButtonEvent.Buttons.Left
        drag_event.buttons = GafferUI.ButtonEvent.Buttons.Left
        drag_event.line = IECore.LineSegment3f(gadget_line.p0, gadget_line.p1)

        payload = view.viewportGadget().dragBeginSignal()(
            view.viewportGadget(), drag_event
        )
        self.assertIsNotNone(payload)

        moved = view.viewportGadget().dragMoveSignal()(
            view.viewportGadget(), drag_event
        )
        self.assertTrue(moved)

        ended = view.viewportGadget().dragEndSignal()(view.viewportGadget(), drag_event)
        self.assertTrue(ended)

        painted_points = script[tool["targetNode"].getValue()]
        self.assertIsInstance(painted_points, GafferScatterPaint.PaintedPoints)
        self.assertEqual(len(painted_points.pointRecords()), 1)
        point_record = painted_points.pointRecords()[0]
        self.assertEqual(point_record["sourcePath"], "/plane")

    @_REQUIRES_SCENEVIEW
    def testCompiledToolClickPlacementUsesPointsCount(self):
        script = Gaffer.ScriptNode()
        script["plane"] = GafferScene.Plane()

        view = GafferSceneUI.SceneView(script)
        view["in"].setInput(script["plane"]["out"])
        tool = GafferScatterPaintUI.PaintPointsTool(view)
        tool["points"].setValue(3)
        tool["density"].setValue(1.0)

        with GafferUI.Window() as window:
            gadget_widget = GafferUI.GadgetWidget(view.viewportGadget())

        window.setVisible(True)
        self.waitForIdle(1000)

        scene_gadget = view.viewportGadget().getPrimaryChild()
        scene_gadget.waitForCompletion()
        view.viewportGadget().frame(scene_gadget.bound())
        self.waitForIdle(1000)

        raster_position = imath.V2f(view.viewportGadget().getViewport()) * 0.5
        gadget_line = view.viewportGadget().rasterToGadgetSpace(
            raster_position, scene_gadget
        )

        event = GafferUI.ButtonEvent(
            GafferUI.ButtonEvent.Buttons.Left,
            GafferUI.ButtonEvent.Buttons.Left,
            IECore.LineSegment3f(gadget_line.p0, gadget_line.p1),
        )
        self.assertTrue(
            view.viewportGadget().buttonPressSignal()(view.viewportGadget(), event)
        )

        drag_event = GafferUI.DragDropEvent()
        drag_event.button = GafferUI.ButtonEvent.Buttons.Left
        drag_event.buttons = GafferUI.ButtonEvent.Buttons.Left
        drag_event.line = IECore.LineSegment3f(gadget_line.p0, gadget_line.p1)

        payload = view.viewportGadget().dragBeginSignal()(
            view.viewportGadget(), drag_event
        )
        self.assertIsNotNone(payload)
        self.assertTrue(
            view.viewportGadget().dragEndSignal()(view.viewportGadget(), drag_event)
        )

        painted_points = script[tool["targetNode"].getValue()]
        self.assertEqual(len(painted_points.pointRecords()), 3)
        self.assertIn("Paint mode committed 3 points", tool["status"].getValue())

    @_REQUIRES_SCENEVIEW
    def testCompiledToolFilteredBrushVolumeUsesPointsCount(self):
        script, group, painted_points, _attached_points = (
            self._build_overlapping_planes_graph()
        )

        painted_points["targetFilter"].setValue(
            "/paintGroup/frontPlane /paintGroup/backPlane"
        )
        painted_points["paintThroughMode"].setValue(1)

        view = GafferSceneUI.SceneView(script)
        view["in"].setInput(group["out"])
        tool = GafferScatterPaintUI.PaintPointsTool(view)
        tool["targetNode"].setValue(painted_points.relativeName(script))
        tool["points"].setValue(3)
        tool["density"].setValue(1.0)

        with GafferUI.Window() as window:
            gadget_widget = GafferUI.GadgetWidget(view.viewportGadget())

        window.setVisible(True)
        self.waitForIdle(1000)

        scene_gadget = view.viewportGadget().getPrimaryChild()
        scene_gadget.waitForCompletion()
        view.viewportGadget().frame(scene_gadget.bound())
        self.waitForIdle(1000)

        raster_position = imath.V2f(view.viewportGadget().getViewport()) * 0.5
        gadget_line = view.viewportGadget().rasterToGadgetSpace(
            raster_position, scene_gadget
        )

        event = GafferUI.ButtonEvent(
            GafferUI.ButtonEvent.Buttons.Left,
            GafferUI.ButtonEvent.Buttons.Left,
            IECore.LineSegment3f(gadget_line.p0, gadget_line.p1),
        )
        self.assertTrue(
            view.viewportGadget().buttonPressSignal()(view.viewportGadget(), event)
        )

        drag_event = GafferUI.DragDropEvent()
        drag_event.button = GafferUI.ButtonEvent.Buttons.Left
        drag_event.buttons = GafferUI.ButtonEvent.Buttons.Left
        drag_event.line = IECore.LineSegment3f(gadget_line.p0, gadget_line.p1)

        payload = view.viewportGadget().dragBeginSignal()(
            view.viewportGadget(), drag_event
        )
        self.assertIsNotNone(payload)
        self.assertTrue(
            view.viewportGadget().dragMoveSignal()(view.viewportGadget(), drag_event)
        )
        self.assertTrue(
            view.viewportGadget().dragEndSignal()(view.viewportGadget(), drag_event)
        )

        point_records = painted_points.pointRecords()
        self.assertEqual(len(point_records), 6)
        self.assertEqual(
            {point["sourcePath"] for point in point_records},
            {"/paintGroup/frontPlane", "/paintGroup/backPlane"},
        )
        self.assertIn("Paint mode committed 6 points", tool["status"].getValue())

    def testPaintedPointsCachePackAcceptsLargePositiveSeeds(self):
        _script, _group, painted_points, _attached_points = (
            self._build_overlapping_planes_graph()
        )
        layer_id = painted_points.ensureLayer("Layer 1")
        stroke_id = painted_points.ensureStroke(layer_id, "Stroke 1")

        painted_points.paintStrokeCommit(
            stroke_id,
            [
                {
                    "P": [0.0, 0.01, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 0xFFFFFFFF,
                    "sourcePath": "/paintGroup/frontPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                }
            ],
            append=False,
        )

        store = painted_points.cacheStoreSnapshot()
        self.assertEqual(store["points"][0]["seed"], 0xFFFFFFFF)

        blob = GafferScatterPaintStoreBridge._pack_store_blob(store)
        restored, error = GafferScatterPaintStoreBridge._unpack_store_blob(blob)
        self.assertFalse(error)
        self.assertEqual(restored["points"][0]["seed"], 0xFFFFFFFF)

    def testAttachedPointsOutObjectAcceptsIntSafeGeneratedSeeds(self):
        _script, _group, painted_points, attached_points = (
            self._build_overlapping_planes_graph()
        )
        layer_id = painted_points.ensureLayer("Layer 1")
        stroke_id = painted_points.ensureStroke(layer_id, "Stroke 1")

        painted_points.paintStrokeCommit(
            stroke_id,
            [
                {
                    "P": [0.0, 0.01, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 0x7FFFFFFF,
                    "sourcePath": "/paintGroup/frontPlane",
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                }
            ],
            append=False,
        )

        scatter = attached_points["out"].object("/paintGroup/scatter")
        seed_primvar = scatter["seed"].data
        self.assertEqual(list(seed_primvar), [0x7FFFFFFF])

    def testBrushPaintPointsUsesPaintThroughMode(self):
        _script, _group, painted_points, _attached_points = (
            self._build_overlapping_planes_graph()
        )

        painted_points["targetFilter"].setValue(
            "/paintGroup/frontPlane /paintGroup/backPlane"
        )
        sample = self._front_plane_sample()

        painted_points["paintThroughMode"].setValue(0)
        front_most_points = painted_points.brushPaintPoints([sample])
        self.assertEqual(len(front_most_points), 1)
        self.assertEqual(front_most_points[0]["sourcePath"], "/paintGroup/frontPlane")
        self.assertTrue(front_most_points[0]["attachmentResolved"])

        painted_points["paintThroughMode"].setValue(1)
        painted_points["targetSetFilter"].setValue("")
        filtered_points = painted_points.brushPaintPoints([sample])
        self.assertEqual(len(filtered_points), 2)
        self.assertEqual(
            {point["sourcePath"] for point in filtered_points},
            {"/paintGroup/frontPlane", "/paintGroup/backPlane"},
        )
        for point in filtered_points:
            self.assertTrue(point["attachmentResolved"])
            self.assertNotEqual(point["P"], point["restWorldP"])

    def testBrushErasePointsDifferentiatesVisibleAndAttachmentSpace(self):
        def build_and_seed_points():
            script, _group, painted_points, _attached_points = (
                self._build_overlapping_planes_graph()
            )
            painted_points["targetFilter"].setValue(
                "/paintGroup/frontPlane /paintGroup/backPlane"
            )
            sample = self._front_plane_sample()
            painted_points["paintThroughMode"].setValue(1)
            authored_points = painted_points.brushPaintPoints([sample])

            layer_id = painted_points.ensureLayer("Layer 1")
            front_stroke = painted_points.ensureStroke(layer_id, "Front Stroke")
            back_stroke = painted_points.ensureStroke(layer_id, "Back Stroke")
            painted_points.paintStrokeCommit(
                front_stroke,
                [
                    point
                    for point in authored_points
                    if point["sourcePath"] == "/paintGroup/frontPlane"
                ],
                append=False,
            )
            painted_points.paintStrokeCommit(
                back_stroke,
                [
                    point
                    for point in authored_points
                    if point["sourcePath"] == "/paintGroup/backPlane"
                ],
                append=False,
            )
            return script, painted_points, sample

        _script, visible_points, sample = build_and_seed_points()
        visible_result = visible_points.brushErasePoints([sample], 0.2, erase_space=0)
        self.assertEqual(visible_result["removedCount"], 1)
        self.assertEqual(visible_result["strokeCount"], 1)
        self.assertEqual(
            [point["sourcePath"] for point in visible_points.pointRecords()],
            ["/paintGroup/backPlane"],
        )

        _script, attachment_points, sample = build_and_seed_points()
        attachment_result = attachment_points.brushErasePoints(
            [sample], 0.2, erase_space=1
        )
        self.assertEqual(attachment_result["removedCount"], 2)
        self.assertEqual(attachment_result["strokeCount"], 2)
        self.assertEqual(attachment_points.pointRecords(), [])

    @_REQUIRES_SCENEVIEW
    def testCompiledToolFilteredBrushVolumeCommitsAcrossFilteredTargets(self):
        script, group, painted_points, _attached_points = (
            self._build_overlapping_planes_graph()
        )

        painted_points["targetFilter"].setValue(
            "/paintGroup/frontPlane /paintGroup/backPlane"
        )
        painted_points["paintThroughMode"].setValue(1)

        view = GafferSceneUI.SceneView(script)
        view["in"].setInput(group["out"])
        tool = GafferScatterPaintUI.PaintPointsTool(view)
        tool["targetNode"].setValue(painted_points.relativeName(script))

        with GafferUI.Window() as window:
            gadget_widget = GafferUI.GadgetWidget(view.viewportGadget())

        window.setVisible(True)
        self.waitForIdle(1000)

        scene_gadget = view.viewportGadget().getPrimaryChild()
        scene_gadget.waitForCompletion()
        view.viewportGadget().frame(scene_gadget.bound())
        self.waitForIdle(1000)

        raster_position = imath.V2f(view.viewportGadget().getViewport()) * 0.5
        gadget_line = view.viewportGadget().rasterToGadgetSpace(
            raster_position, scene_gadget
        )

        event = GafferUI.ButtonEvent(
            GafferUI.ButtonEvent.Buttons.Left,
            GafferUI.ButtonEvent.Buttons.Left,
            IECore.LineSegment3f(gadget_line.p0, gadget_line.p1),
        )
        self.assertTrue(
            view.viewportGadget().buttonPressSignal()(view.viewportGadget(), event)
        )

        drag_event = GafferUI.DragDropEvent()
        drag_event.button = GafferUI.ButtonEvent.Buttons.Left
        drag_event.buttons = GafferUI.ButtonEvent.Buttons.Left
        drag_event.line = IECore.LineSegment3f(gadget_line.p0, gadget_line.p1)

        payload = view.viewportGadget().dragBeginSignal()(
            view.viewportGadget(), drag_event
        )
        self.assertIsNotNone(payload)
        self.assertTrue(
            view.viewportGadget().dragMoveSignal()(view.viewportGadget(), drag_event)
        )
        self.assertTrue(
            view.viewportGadget().dragEndSignal()(view.viewportGadget(), drag_event)
        )

        point_records = painted_points.pointRecords()
        self.assertEqual(len(point_records), 2)
        self.assertEqual(
            {point["sourcePath"] for point in point_records},
            {"/paintGroup/frontPlane", "/paintGroup/backPlane"},
        )
        self.assertIn("Paint mode committed 2 points", tool["status"].getValue())


if __name__ == "__main__":
    unittest.main()
