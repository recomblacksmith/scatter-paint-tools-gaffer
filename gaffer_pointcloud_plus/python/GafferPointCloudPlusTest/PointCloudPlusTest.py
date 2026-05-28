import unittest
import importlib.util
from pathlib import Path

import Gaffer
import GafferScene
import GafferPointCloudPlus
import GafferPointCloudPlusUI
import IECore
import IECoreScene
import imath


def _load_actions_module():
    actions_path = Path(__file__).resolve().parents[2] / "startup" / "GafferPointCloudPlusUI" / "actions.py"
    spec = importlib.util.spec_from_file_location("_pointCloudPlusTestActions", actions_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load point cloud plus actions from {actions_path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


pointCloudActions = _load_actions_module()


class PointCloudPlusTest(unittest.TestCase):
    def _num_points(self, points_primitive):
        return len(points_primitive["P"].data)

    def testNodeLoads(self):
        self.assertIsNotNone(GafferPointCloudPlus.PointCloudPlus)
        node = GafferPointCloudPlus.PointCloudPlus()
        self.assertEqual(node.typeName(), "GafferPointCloudPlus::PointCloudPlus")

    def testGeometryModeProducesPointsAtOutputLocation(self):
        script = Gaffer.ScriptNode()

        plane = GafferScene.Plane("Plane")
        plane["name"].setValue("support")
        script.addChild(plane)

        node = GafferPointCloudPlus.PointCloudPlus("PointCloudPlus")
        script.addChild(node)
        node["in"].setInput(plane["out"])
        node["outputLocation"].setValue("/support/points")
        node["filter"].setValue("/support")
        node["mode"].setValue(0)
        node["distribution"].setValue(0)
        node["pointCount"].setValue(12)

        obj = node["out"].object("/support/points")
        self.assertEqual(obj.typeName(), "PointsPrimitive")
        self.assertEqual(self._num_points(obj), 12)
        self.assertIn("type", obj)
        self.assertEqual(obj["type"].data.value, "gl:point")
        self.assertIn("Cs", obj)
        self.assertIn("scatterColor", obj)
        self.assertEqual(obj["Cs"].data[0], imath.Color3f(0.0, 0.0, 120.0 / 255.0))

    def testPrimitiveCenterModeProducesPoints(self):
        plane = GafferScene.Plane()
        plane["name"].setValue("support")

        node = GafferPointCloudPlus.PointCloudPlus()
        node["in"].setInput(plane["out"])
        node["outputLocation"].setValue("/support/points")
        node["filter"].setValue("/support")
        node["distribution"].setValue(1)

        obj = node["out"].object("/support/points")
        self.assertEqual(obj.typeName(), "PointsPrimitive")
        self.assertGreater(self._num_points(obj), 0)

    def testFileModeRepublishesInputPoints(self):
        upstream = GafferScene.Sphere("Sphere")
        upstream["name"].setValue("source")

        source_points = GafferPointCloudPlus.PointCloudPlus("SourcePoints")
        source_points["in"].setInput(upstream["out"])
        source_points["outputLocation"].setValue("/source/points")
        source_points["filter"].setValue("/source")
        source_points["pointCount"].setValue(7)

        republish = GafferPointCloudPlus.PointCloudPlus("Republish")
        republish["in"].setInput(source_points["out"])
        republish["mode"].setValue(1)
        republish["outputLocation"].setValue("/republished")
        republish["primPath"].setValue("/source/points")

        obj = republish["out"].object("/republished")
        self.assertEqual(obj.typeName(), "PointsPrimitive")
        self.assertEqual(self._num_points(obj), 7)

    def testFileModeAutoFindsInputPointsWhenPrimPathIsEmpty(self):
        upstream = GafferScene.Sphere("Sphere")
        upstream["name"].setValue("source")

        source_points = GafferPointCloudPlus.PointCloudPlus("SourcePoints")
        source_points["in"].setInput(upstream["out"])
        source_points["outputLocation"].setValue("/source/points")
        source_points["filter"].setValue("/source")
        source_points["pointCount"].setValue(9)

        republish = GafferPointCloudPlus.PointCloudPlus("Republish")
        republish["in"].setInput(source_points["out"])
        republish["mode"].setValue(1)
        republish["outputLocation"].setValue("/republished")

        obj = republish["out"].object("/republished")
        self.assertEqual(obj.typeName(), "PointsPrimitive")
        self.assertEqual(self._num_points(obj), 9)

    def testFileModePlaybackControlsDriveEvaluationFrame(self):
        script = Gaffer.ScriptNode()

        source = GafferScene.ObjectToScene("Source")
        source["name"].setValue("sourcePoints")
        script.addChild(source)

        expression = Gaffer.Expression("AnimatedPoints")
        script.addChild(expression)
        expression.setExpression(
            'import IECore\nimport IECoreScene\nimport imath\nframe = float(context.getFrame())\nparent["Source"]["object"] = IECoreScene.PointsPrimitive( IECore.V3fVectorData( [ imath.V3f( frame, 0, 0 ) ], IECore.GeometricData.Interpretation.Point ), IECore.FloatVectorData( [ 0.25 ] ) )',
            "python",
        )

        hold = GafferPointCloudPlus.PointCloudPlus("Hold")
        hold["in"].setInput(source["out"])
        hold["mode"].setValue(1)
        hold["outputLocation"].setValue("/republished")
        hold["primPath"].setValue("/sourcePoints")
        hold["frame"].setValue(2.0)
        hold["frameOffset"].setValue(1.0)
        hold["animationBehavior"].setValue(0)
        script.addChild(hold)

        repeat = GafferPointCloudPlus.PointCloudPlus("Repeat")
        repeat["in"].setInput(source["out"])
        repeat["mode"].setValue(1)
        repeat["outputLocation"].setValue("/republished")
        repeat["primPath"].setValue("/sourcePoints")
        repeat["frame"].setValue(2.0)
        repeat["frameOffset"].setValue(1.0)
        repeat["animationBehavior"].setValue(1)
        script.addChild(repeat)

        context = Gaffer.Context.current()
        previous_frame = context.getFrame()
        try:
            context.setFrame(10.0)
            hold_points = hold["out"].object("/republished")
            repeat_points = repeat["out"].object("/republished")
        finally:
            context.setFrame(previous_frame)

        self.assertEqual(hold_points["P"].data[0], imath.V3f(3.0, 0.0, 0.0))
        self.assertEqual(repeat_points["P"].data[0], imath.V3f(13.0, 0.0, 0.0))

    def testGeometryModeAggregatesMultipleMeshes(self):
        sphere = GafferScene.Sphere("Sphere")
        sphere["name"].setValue("sphere")

        plane = GafferScene.Plane("Plane")
        plane["name"].setValue("plane")

        group = GafferScene.Group("Group")
        group["name"].setValue("support")
        group["in"][0].setInput(sphere["out"])
        group["in"][1].setInput(plane["out"])

        node = GafferPointCloudPlus.PointCloudPlus()
        node["in"].setInput(group["out"])
        node["outputLocation"].setValue("/generated/points")
        node["filter"].setValue("/support/*")
        node["distribution"].setValue(1)

        obj = node["out"].object("/generated/points")
        source_paths = {str(path) for path in obj["sourcePath"].data}
        self.assertIn("/support/sphere", source_paths)
        self.assertIn("/support/plane", source_paths)

    def testGeneratedLeafKeepsExistingAncestorTransform(self):
        plane = GafferScene.Plane("Plane")
        plane["name"].setValue("support")

        filter_node = GafferScene.PathFilter("Filter")
        filter_node["paths"].setValue(IECore.StringVectorData(["/support"]))

        parent_transform = GafferScene.Transform("ParentTransform")
        parent_transform["in"].setInput(plane["out"])
        parent_transform["filter"].setInput(filter_node["out"])
        parent_transform["transform"]["translate"].setValue(imath.V3f(1.0, 2.0, 3.0))

        attrs = GafferScene.CustomAttributes("Attrs")
        attrs["in"].setInput(parent_transform["out"])
        attrs["filter"].setInput(filter_node["out"])
        attrs["attributes"].addChild(Gaffer.NameValuePlug("test:flag", IECore.IntData(7), True, "test_flag"))

        node = GafferPointCloudPlus.PointCloudPlus()
        node["in"].setInput(attrs["out"])
        node["outputLocation"].setValue("/support/points")
        node["filter"].setValue("/support")
        node["pointCount"].setValue(3)

        self.assertEqual(node["out"].transform("/support"), attrs["out"].transform("/support"))
        self.assertEqual(node["out"].attributes("/support"), attrs["out"].attributes("/support"))

    def testCreatesIntermediateOutputBranches(self):
        plane = GafferScene.Plane("Plane")
        plane["name"].setValue("support")

        node = GafferPointCloudPlus.PointCloudPlus("PointCloudPlus")
        node["in"].setInput(plane["out"])
        node["outputLocation"].setValue("/generated/cloud/points")
        node["filter"].setValue("/support")
        node["pointCount"].setValue(5)

        self.assertTrue(node["out"].exists("/generated"))
        self.assertTrue(node["out"].exists("/generated/cloud"))
        self.assertTrue(node["out"].exists("/generated/cloud/points"))

        obj = node["out"].object("/generated/cloud/points")
        self.assertEqual(obj.typeName(), "PointsPrimitive")
        self.assertEqual(self._num_points(obj), 5)

        root_bound = node["out"].bound("/")
        self.assertFalse(root_bound.isEmpty())

    def testDemoActionsCreateOpenGLDisplayNode(self):
        script = Gaffer.ScriptNode()
        point_cloud = pointCloudActions.build_geometry_demo(script)

        self.assertEqual(point_cloud.typeName(), "GafferPointCloudPlus::PointCloudPlus")
        open_gl_nodes = [
            child
            for child in script.children()
            if isinstance(child, GafferScene.OpenGLAttributes)
        ]
        self.assertEqual(len(open_gl_nodes), 1)
        self.assertTrue(open_gl_nodes[0]["in"].getInput().isSame(point_cloud["out"]))
        self.assertEqual(
            open_gl_nodes[0]["attributes"]["gl_pointsPrimitive_glPointWidth"]["value"].getValue(),
            24.0,
        )
        self.assertTrue(script.getFocus().isSame(open_gl_nodes[0]))
        self.assertEqual(set(script.selection()), {point_cloud, open_gl_nodes[0], script["PointCloudPlusPlane"]})
