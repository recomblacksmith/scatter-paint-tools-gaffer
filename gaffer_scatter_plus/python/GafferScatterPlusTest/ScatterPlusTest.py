import importlib.util
from pathlib import Path
import unittest

import Gaffer
import GafferImage
import IECore
import IECoreScene
import GafferScene
import GafferScatterPlus
import imath


def _load_actions_module():
    actions_path = Path(__file__).resolve().parents[2] / "startup" / "GafferScatterPlusUI" / "actions.py"
    spec = importlib.util.spec_from_file_location("_scatterPlusTestActions", actions_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load scatter plus actions from {actions_path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


scatterPlusActions = _load_actions_module()


class ScatterPlusTest(unittest.TestCase):
    def _build_prototype_scene(self, script=None):
        sphere = GafferScene.Sphere("PrototypeSphere")
        sphere["name"].setValue("prototypeSphere")
        plane = GafferScene.Plane("PrototypePlane")
        plane["name"].setValue("prototypePlane")
        group = GafferScene.Group("PrototypeGroup")
        group["name"].setValue("prototypeLibrary")
        group["in"][0].setInput(sphere["out"])
        group["in"][1].setInput(plane["out"])
        if script is not None:
            script.addChild(sphere)
            script.addChild(plane)
            script.addChild(group)
        # Keep upstream nodes alive for tests that only retain the grouped output.
        self._prototype_scene_nodes = (sphere, plane, group)
        return sphere, plane, group

    def _num_points(self, points_primitive):
        return len(points_primitive["P"].data)

    def _scatter_points(self, node):
        return node["out"].object("/scatter/points")

    def _build_animated_support(self, script):
        support = GafferScene.Plane("Support")
        support["name"].setValue("support")
        script.addChild(support)

        support_filter = GafferScene.PathFilter("SupportFilter")
        support_filter["paths"].setValue(IECore.StringVectorData(["/support"]))
        script.addChild(support_filter)

        support_transform = GafferScene.Transform("AnimatedSupportTransform")
        support_transform["in"].setInput(support["out"])
        support_transform["filter"].setInput(support_filter["out"])
        script.addChild(support_transform)

        expression = Gaffer.Expression("AnimatedSupportExpression")
        script.addChild(expression)
        expression.setExpression(
            'parent["AnimatedSupportTransform"]["transform"]["translate"]["x"] = int(context.getFrame()) * 2.0',
            "python",
        )

        return support, support_filter, support_transform, expression

    def testNodeLoads(self):
        self.assertIsNotNone(GafferScatterPlus.ScatterPlus)
        node = GafferScatterPlus.ScatterPlus()
        self.assertEqual(node.typeName(), "GafferScatterPlus::ScatterPlus")

    def testCreatesPointsAndInstancesBranches(self):
        script = Gaffer.ScriptNode()
        support = GafferScene.Plane("Support")
        support["name"].setValue("support")
        script.addChild(support)
        _, _, prototypes = self._build_prototype_scene(script)

        node = GafferScatterPlus.ScatterPlus("ScatterPlus")
        script.addChild(node)
        node["in"][0].setInput(support["out"])
        node["in"][1].setInput(prototypes["out"])
        node["outputLocation"].setValue("/scatter")
        node["support"].setValue("/support")
        node["prototypeRoots"].setValue("/prototypeLibrary/prototypeSphere /prototypeLibrary/prototypePlane")
        node["pointCount"].setValue(10)

        self.assertTrue(node["out"].exists("/scatter"))
        self.assertTrue(node["out"].exists("/scatter/points"))
        self.assertTrue(node["out"].exists("/scatter/instances"))

        points = node["out"].object("/scatter/points")
        self.assertEqual(points.typeName(), "PointsPrimitive")
        self.assertEqual(self._num_points(points), 10)
        self.assertIn("prototypeIndex", points)
        self.assertIn("orientation", points)
        self.assertIn("scale", points)
        self.assertIn("scatter_position", points)
        self.assertIn("scatter_rotation", points)
        self.assertIn("scatter_scale", points)
        self.assertIn("scatter_normal", points)
        self.assertIn("scatter_rotation_order", points)
        self.assertIn("scatter_time_offset", points)

        child_names = [str(x) for x in node["out"].childNames("/scatter/instances")]
        self.assertEqual(len(child_names), 10)
        self.assertEqual(child_names[0], "instance0000")

    def testRandomPrototypeModeWritesPrototypeAttribute(self):
        support = GafferScene.Plane("Support")
        support["name"].setValue("support")
        _, _, prototypes = self._build_prototype_scene()

        node = GafferScatterPlus.ScatterPlus()
        node["in"][0].setInput(support["out"])
        node["in"][1].setInput(prototypes["out"])
        node["outputLocation"].setValue("/scatter")
        node["support"].setValue("/support")
        node["prototypeRoots"].setValue("/prototypeLibrary/prototypeSphere /prototypeLibrary/prototypePlane")
        node["pointCount"].setValue(16)
        node["prototypeMode"].setValue(1)
        node["seed"].setValue(17)

        prototype_paths = set()
        for child in node["out"].childNames("/scatter/instances"):
            attrs = node["out"].attributes(f"/scatter/instances/{child}")
            prototype_paths.add(attrs["scatterPlus:prototypePath"].value)

        self.assertIn("/prototypeLibrary/prototypeSphere", prototype_paths)
        self.assertIn("/prototypeLibrary/prototypePlane", prototype_paths)

    def testCreatesIntermediateOutputBranches(self):
        support = GafferScene.Plane("Support")
        support["name"].setValue("support")
        _, _, prototypes = self._build_prototype_scene()

        node = GafferScatterPlus.ScatterPlus()
        node["in"][0].setInput(support["out"])
        node["in"][1].setInput(prototypes["out"])
        node["outputLocation"].setValue("/generated/cloud/scatter")
        node["support"].setValue("/support")
        node["prototypeRoots"].setValue("/prototypeLibrary/prototypeSphere")
        node["pointCount"].setValue(4)

        self.assertTrue(node["out"].exists("/generated"))
        self.assertTrue(node["out"].exists("/generated/cloud"))
        self.assertTrue(node["out"].exists("/generated/cloud/scatter/points"))
        self.assertTrue(node["out"].exists("/generated/cloud/scatter/instances"))
        self.assertFalse(node["out"].bound("/").isEmpty())

    def testImageDistributionAndLuminancePrototypeMode(self):
        support = GafferScene.Plane("Support")
        support["name"].setValue("support")
        support["divisions"].setValue(imath.V2i(20, 20))
        _, _, prototypes = self._build_prototype_scene()

        image = GafferImage.Checkerboard("Density")
        image["format"].setValue(GafferImage.Format(128, 128, 1.0))
        image["size"].setValue(imath.V2f(0.12, 0.12))

        node = GafferScatterPlus.ScatterPlus()
        node["in"][0].setInput(support["out"])
        node["in"][1].setInput(prototypes["out"])
        node["image"].setInput(image["out"])
        node["outputLocation"].setValue("/scatter")
        node["support"].setValue("/support")
        node["prototypeRoots"].setValue("/prototypeLibrary/prototypeSphere /prototypeLibrary/prototypePlane")
        node["distribution"].setValue(2)
        node["prototypeMode"].setValue(5)
        node["pointCount"].setValue(64)

        points = node["out"].object("/scatter/points")
        self.assertGreater(self._num_points(points), 0)
        attrs = node["out"].attributes("/scatter/instances/instance0000")
        self.assertIn(attrs["scatterPlus:prototypePath"].value, {"/prototypeLibrary/prototypeSphere", "/prototypeLibrary/prototypePlane"})
        self.assertIn("density", points)
        self.assertIn("prototypeIndex", points)
        self.assertIn("orientation", points)
        self.assertIn("scale", points)

    def testIdPrototypeModeUsesSupportIdPrimitiveVariable(self):
        support_source = GafferScene.Plane("SupportSource")
        support_source["name"].setValue("support")
        support_source["divisions"].setValue(imath.V2i(2, 1))

        support_mesh = support_source["out"].object("/support").copy()
        support_mesh["scatterId"] = IECoreScene.PrimitiveVariable(
            IECoreScene.PrimitiveVariable.Interpolation.Uniform,
            IECore.IntVectorData([10, 22]),
        )

        support = GafferScene.ObjectToScene("SupportMesh")
        support["object"].setValue(support_mesh)
        support["name"].setValue("support")

        sphere, plane, prototypes = self._build_prototype_scene()

        sphere_attrs = GafferScene.CustomAttributes("SphereAttrs")
        sphere_attrs["in"].setInput(sphere["out"])
        sphere_attrs["attributes"].addChild(
            Gaffer.NameValuePlug("geometryId", IECore.IntData(10), True, "sphereGeometryId")
        )

        plane_attrs = GafferScene.CustomAttributes("PlaneAttrs")
        plane_attrs["in"].setInput(plane["out"])
        plane_attrs["attributes"].addChild(
            Gaffer.NameValuePlug("geometryId", IECore.IntData(22), True, "planeGeometryId")
        )

        prototype_group = GafferScene.Group("PrototypeGroupWithIds")
        prototype_group["name"].setValue("prototypeLibrary")
        prototype_group["in"][0].setInput(sphere_attrs["out"])
        prototype_group["in"][1].setInput(plane_attrs["out"])
        self._prototype_scene_nodes = (support_source, support, sphere, plane, prototypes, sphere_attrs, plane_attrs, prototype_group)

        node = GafferScatterPlus.ScatterPlus()
        node["in"][0].setInput(support["out"])
        node["in"][1].setInput(prototype_group["out"])
        node["outputLocation"].setValue("/scatter")
        node["support"].setValue("/support")
        node["prototypeRoots"].setValue("/prototypeLibrary/prototypeSphere /prototypeLibrary/prototypePlane")
        node["prototypeMode"].setValue(4)
        node["idVariable"].setValue("scatterId")
        node["geometryIdAttribute"].setValue("geometryId")
        node["distribution"].setValue(1)

        points = node["out"].object("/scatter/points")
        self.assertEqual(sorted(set(points["scatterId"].data)), [10, 22])

        prototype_paths = set()
        for child in node["out"].childNames("/scatter/instances"):
            attrs = node["out"].attributes(f"/scatter/instances/{child}")
            prototype_paths.add(attrs["scatterPlus:prototypePath"].value)

        self.assertEqual(prototype_paths, {"/prototypeLibrary/prototypeSphere", "/prototypeLibrary/prototypePlane"})

    def testTimeVarianceSamplesProduceQuantizedOffsets(self):
        support = GafferScene.Plane("Support")
        support["name"].setValue("support")
        _, _, prototypes = self._build_prototype_scene()

        node = GafferScatterPlus.ScatterPlus()
        node["in"][0].setInput(support["out"])
        node["in"][1].setInput(prototypes["out"])
        node["outputLocation"].setValue("/scatter")
        node["support"].setValue("/support")
        node["prototypeRoots"].setValue("/prototypeLibrary/prototypeSphere")
        node["pointCount"].setValue(24)
        node["seed"].setValue(9)
        node["timeOffset"].setValue(1.0)
        node["timeVariance"].setValue(0.6)
        node["timeVarianceSamples"].setValue(3)

        points = node["out"].object("/scatter/points")
        offsets = {round(value, 4) for value in points["scatter_time_offset"].data}
        self.assertEqual(offsets, {0.4, 1.0, 1.6})

    def testDecimationSeedAffectsThresholdFilteringWithoutImage(self):
        support = GafferScene.Plane("Support")
        support["name"].setValue("support")
        _, _, prototypes = self._build_prototype_scene()

        first = GafferScatterPlus.ScatterPlus("First")
        first["in"][0].setInput(support["out"])
        first["in"][1].setInput(prototypes["out"])
        first["outputLocation"].setValue("/scatter")
        first["support"].setValue("/support")
        first["prototypeRoots"].setValue("/prototypeLibrary/prototypeSphere")
        first["pointCount"].setValue(40)
        first["seed"].setValue(3)
        first["decimateValue"].setValue(0.5)
        first["decimationSeed"].setValue(1)

        second = GafferScatterPlus.ScatterPlus("Second")
        second["in"][0].setInput(support["out"])
        second["in"][1].setInput(prototypes["out"])
        second["outputLocation"].setValue("/scatter")
        second["support"].setValue("/support")
        second["prototypeRoots"].setValue("/prototypeLibrary/prototypeSphere")
        second["pointCount"].setValue(40)
        second["seed"].setValue(3)
        second["decimateValue"].setValue(0.5)
        second["decimationSeed"].setValue(99)

        first_ids = list(first["out"].object("/scatter/points")["id"].data)
        second_ids = list(second["out"].object("/scatter/points")["id"].data)
        self.assertNotEqual(first_ids, second_ids)

    def testEllipsoidCollisionKeepsMoreThanBoundsMode(self):
        support = GafferScene.Plane("Support")
        support["name"].setValue("support")
        support["divisions"].setValue(imath.V2i(2, 2))

        support_filter = GafferScene.PathFilter("SupportFilter")
        support_filter["paths"].setValue(IECore.StringVectorData(["/support"]))

        support_transform = GafferScene.Transform("SupportTransform")
        support_transform["in"].setInput(support["out"])
        support_transform["filter"].setInput(support_filter["out"])
        support_transform["transform"]["scale"].setValue(imath.V3f(3.0, 3.0, 1.0))

        sphere, _, prototypes = self._build_prototype_scene()
        sphere["radius"].setValue(0.9)

        bounds_node = GafferScatterPlus.ScatterPlus("BoundsCollision")
        bounds_node["in"][0].setInput(support_transform["out"])
        bounds_node["in"][1].setInput(prototypes["out"])
        bounds_node["outputLocation"].setValue("/scatter")
        bounds_node["support"].setValue("/support")
        bounds_node["prototypeRoots"].setValue("/prototypeLibrary/prototypeSphere")
        bounds_node["distribution"].setValue(1)
        bounds_node["supportSpace"].setValue(1)
        bounds_node["collisionMode"].setValue(1)
        bounds_node["collisionScaleMultiplier"].setValue(1.0)

        ellipsoid_node = GafferScatterPlus.ScatterPlus("EllipsoidCollision")
        ellipsoid_node["in"][0].setInput(support_transform["out"])
        ellipsoid_node["in"][1].setInput(prototypes["out"])
        ellipsoid_node["outputLocation"].setValue("/scatter")
        ellipsoid_node["support"].setValue("/support")
        ellipsoid_node["prototypeRoots"].setValue("/prototypeLibrary/prototypeSphere")
        ellipsoid_node["distribution"].setValue(1)
        ellipsoid_node["supportSpace"].setValue(1)
        ellipsoid_node["collisionMode"].setValue(2)
        ellipsoid_node["collisionScaleMultiplier"].setValue(1.0)

        bounds_count = self._num_points(bounds_node["out"].object("/scatter/points"))
        ellipsoid_count = self._num_points(ellipsoid_node["out"].object("/scatter/points"))

        self.assertEqual(bounds_count, 1)
        self.assertGreater(ellipsoid_count, bounds_count)
        self.assertEqual(ellipsoid_count, 2)

    def testReferenceSupportSpaceIgnoresSupportTransform(self):
        script = Gaffer.ScriptNode()
        _, support_filter, support_transform, expression = self._build_animated_support(script)
        _, _, prototypes = self._build_prototype_scene(script)
        self._prototype_scene_nodes = self._prototype_scene_nodes + (support_filter, support_transform, expression)

        world_node = GafferScatterPlus.ScatterPlus("WorldSpace")
        world_node["in"][0].setInput(support_transform["out"])
        world_node["in"][1].setInput(prototypes["out"])
        world_node["outputLocation"].setValue("/scatter")
        world_node["support"].setValue("/support")
        world_node["prototypeRoots"].setValue("/prototypeLibrary/prototypeSphere")
        world_node["distribution"].setValue(1)
        world_node["supportSpace"].setValue(1)

        reference_node = GafferScatterPlus.ScatterPlus("ReferenceSpace")
        reference_node["in"][0].setInput(support_transform["out"])
        reference_node["in"][1].setInput(prototypes["out"])
        reference_node["outputLocation"].setValue("/scatter")
        reference_node["support"].setValue("/support")
        reference_node["prototypeRoots"].setValue("/prototypeLibrary/prototypeSphere")
        reference_node["distribution"].setValue(1)
        reference_node["supportSpace"].setValue(2)

        script.addChild(world_node)
        script.addChild(reference_node)

        context = Gaffer.Context.current()
        previous_frame = context.getFrame()
        try:
            context.setFrame(5.0)
            world_points = world_node["out"].object("/scatter/points")
            reference_points = reference_node["out"].object("/scatter/points")
        finally:
            context.setFrame(previous_frame)

        self.assertEqual(len(world_points["P"].data), 1)
        self.assertEqual(len(reference_points["P"].data), 1)
        self.assertAlmostEqual(world_points["P"].data[0].x, 10.0, places=5)
        self.assertAlmostEqual(reference_points["P"].data[0].x, 0.0, places=5)

    def testReferenceDecimationSpaceChangesProceduralDecimation(self):
        support = GafferScene.Plane("Support")
        support["name"].setValue("support")
        support["divisions"].setValue(imath.V2i(8, 1))

        support_filter = GafferScene.PathFilter("SupportFilter")
        support_filter["paths"].setValue(IECore.StringVectorData(["/support"]))

        support_transform = GafferScene.Transform("SupportTransform")
        support_transform["in"].setInput(support["out"])
        support_transform["filter"].setInput(support_filter["out"])
        support_transform["transform"]["translate"].setValue(imath.V3f(7.25, 0.0, 0.0))

        _, _, prototypes = self._build_prototype_scene()

        object_node = GafferScatterPlus.ScatterPlus("ObjectDecimation")
        object_node["in"][0].setInput(support_transform["out"])
        object_node["in"][1].setInput(prototypes["out"])
        object_node["outputLocation"].setValue("/scatter")
        object_node["support"].setValue("/support")
        object_node["prototypeRoots"].setValue("/prototypeLibrary/prototypeSphere")
        object_node["distribution"].setValue(1)
        object_node["supportSpace"].setValue(2)
        object_node["decimationSpace"].setValue(0)
        object_node["decimateValue"].setValue(0.5)
        object_node["decimationSeed"].setValue(11)

        reference_node = GafferScatterPlus.ScatterPlus("ReferenceDecimation")
        reference_node["in"][0].setInput(support_transform["out"])
        reference_node["in"][1].setInput(prototypes["out"])
        reference_node["outputLocation"].setValue("/scatter")
        reference_node["support"].setValue("/support")
        reference_node["prototypeRoots"].setValue("/prototypeLibrary/prototypeSphere")
        reference_node["distribution"].setValue(1)
        reference_node["supportSpace"].setValue(1)
        reference_node["decimationSpace"].setValue(2)
        reference_node["decimateValue"].setValue(0.5)
        reference_node["decimationSeed"].setValue(11)

        object_ids = list(object_node["out"].object("/scatter/points")["id"].data)
        reference_ids = list(reference_node["out"].object("/scatter/points")["id"].data)

        self.assertNotEqual(object_ids, reference_ids)

    def testReferencePositionPrimvarTracksFrameZeroSupport(self):
        script = Gaffer.ScriptNode()
        _, support_filter, support_transform, expression = self._build_animated_support(script)
        _, _, prototypes = self._build_prototype_scene(script)
        self._prototype_scene_nodes = self._prototype_scene_nodes + (support_filter, support_transform, expression)

        node = GafferScatterPlus.ScatterPlus("ReferencePosition")
        node["in"][0].setInput(support_transform["out"])
        node["in"][1].setInput(prototypes["out"])
        node["outputLocation"].setValue("/scatter")
        node["support"].setValue("/support")
        node["prototypeRoots"].setValue("/prototypeLibrary/prototypeSphere")
        node["distribution"].setValue(1)
        node["supportSpace"].setValue(1)
        script.addChild(node)

        context = Gaffer.Context.current()
        previous_frame = context.getFrame()
        try:
            context.setFrame(5.0)
            points = self._scatter_points(node)
        finally:
            context.setFrame(previous_frame)

        self.assertAlmostEqual(points["P"].data[0].x, 10.0, places=5)
        self.assertAlmostEqual(points["referencePosition"].data[0].x, 0.0, places=5)

    def testReferenceFramePlugControlsReferenceEvaluation(self):
        script = Gaffer.ScriptNode()
        _, support_filter, support_transform, expression = self._build_animated_support(script)
        _, _, prototypes = self._build_prototype_scene(script)
        self._prototype_scene_nodes = self._prototype_scene_nodes + (support_filter, support_transform, expression)

        node = GafferScatterPlus.ScatterPlus("ReferenceFrame")
        node["in"][0].setInput(support_transform["out"])
        node["in"][1].setInput(prototypes["out"])
        node["outputLocation"].setValue("/scatter")
        node["support"].setValue("/support")
        node["prototypeRoots"].setValue("/prototypeLibrary/prototypeSphere")
        node["distribution"].setValue(1)
        node["supportSpace"].setValue(2)
        node["referenceFrame"].setValue(3.0)
        script.addChild(node)

        context = Gaffer.Context.current()
        previous_frame = context.getFrame()
        try:
            context.setFrame(5.0)
            points = self._scatter_points(node)
        finally:
            context.setFrame(previous_frame)

        self.assertAlmostEqual(points["P"].data[0].x, 6.0, places=5)
        self.assertAlmostEqual(points["referencePosition"].data[0].x, 6.0, places=5)

    def testDensityPrimitiveVariableDrivesDecimationWithoutImage(self):
        support_source = GafferScene.Plane("SupportSource")
        support_source["name"].setValue("support")
        support_source["divisions"].setValue(imath.V2i(4, 1))

        support_mesh = support_source["out"].object("/support").copy()
        support_mesh["density"] = IECoreScene.PrimitiveVariable(
            IECoreScene.PrimitiveVariable.Interpolation.Uniform,
            IECore.FloatVectorData([1.0, 0.0, 1.0, 0.0]),
        )

        support = GafferScene.ObjectToScene("SupportMesh")
        support["object"].setValue(support_mesh)
        support["name"].setValue("support")

        _, _, prototypes = self._build_prototype_scene()
        self._prototype_scene_nodes = self._prototype_scene_nodes + (support_source, support)

        node = GafferScatterPlus.ScatterPlus("DensityDriven")
        node["in"][0].setInput(support["out"])
        node["in"][1].setInput(prototypes["out"])
        node["outputLocation"].setValue("/scatter")
        node["support"].setValue("/support")
        node["prototypeRoots"].setValue("/prototypeLibrary/prototypeSphere")
        node["distribution"].setValue(1)
        node["densityPrimitiveVariable"].setValue("density")
        node["decimateValue"].setValue(0.5)

        points = self._scatter_points(node)
        densities = list(points["density"].data)
        point_ids = list(points["id"].data)

        self.assertEqual(densities, [1.0, 1.0])
        self.assertEqual(point_ids, [0, 2])

    def testDemoActionsCreateDisplayNode(self):
        script = Gaffer.ScriptNode()
        scatter = scatterPlusActions.build_image_demo(script)
        self.assertEqual(scatter.typeName(), "GafferScatterPlus::ScatterPlus")

        open_gl_nodes = [child for child in script.children() if isinstance(child, GafferScene.OpenGLAttributes)]
        self.assertEqual(len(open_gl_nodes), 1)
        self.assertTrue(open_gl_nodes[0]["in"].getInput().isSame(scatter["out"]))
        self.assertTrue(script.getFocus().isSame(open_gl_nodes[0]))
        self.assertIn(script["ScatterPlusSupport"], set(script.selection()))

        images = [child for child in script.children() if isinstance(child, GafferImage.Checkerboard)]
        self.assertEqual(len(images), 1)
        self.assertTrue(scatter["image"].getInput().isSame(images[0]["out"]))
