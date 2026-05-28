import Gaffer
import GafferScene
import GafferSceneUI
import GafferUI
import IECore
import imath

import GafferPointCloudPlus


def _unique_node_name(script, base_name):
    node_name = base_name
    suffix = 1
    while node_name in script:
        suffix += 1
        node_name = f"{base_name}{suffix}"
    return node_name


def _attribute_plug(attributes_plug, name, default_value):
    if name in attributes_plug:
        return attributes_plug[name]

    plug_name = name.replace(":", "_")
    index = 1
    while plug_name in attributes_plug:
        index += 1
        plug_name = f"{name.replace(':', '_')}{index}"

    plug = Gaffer.NameValuePlug(
        name,
        default_value,
        defaultEnabled=False,
        name=plug_name,
        flags=Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic,
    )
    attributes_plug.addChild(plug)
    return plug


def _script_node(menu):
    if isinstance(menu, Gaffer.ScriptNode):
        return menu

    if menu is None:
        return Gaffer.ScriptNode()

    script_window = menu.ancestor(GafferUI.ScriptWindow)
    if script_window is None:
        raise RuntimeError("PointCloud Plus actions require a ScriptWindow.")
    return script_window.scriptNode()


def _graph_editor(menu):
    if isinstance(menu, Gaffer.ScriptNode) or menu is None:
        return None
    return menu.ancestor(GafferUI.GraphEditor)


def _log_warning(message):
    IECore.msg(IECore.Msg.Level.Warning, "GafferPointCloudPlusUI", message)
    print(f"WARNING : GafferPointCloudPlusUI : {message}", flush=True)


def _scene_views_for_script(script):
    script_window = GafferUI.ScriptWindow.acquire(script, createIfNecessary=False)
    if script_window is None:
        return [], []

    layout = script_window.getLayout()
    viewers = [
        viewer
        for viewer in layout.editors(type=GafferUI.Viewer)
        if isinstance(viewer.view(), GafferSceneUI.SceneView)
    ]
    return [viewer.view() for viewer in viewers], viewers


def _ensure_scene_viewers(script):
    scene_views, viewers = _scene_views_for_script(script)
    if scene_views:
        return scene_views, viewers

    script_window = GafferUI.ScriptWindow.acquire(script, createIfNecessary=True)
    layout = script_window.getLayout()
    viewer = GafferUI.Viewer(script)
    layout.addEditor(viewer)
    view = viewer.view()
    if not isinstance(view, GafferSceneUI.SceneView):
        return _scene_views_for_script(script)
    return [view], [viewer]


def _prime_scene_view(scene_view, reason):
    try:
        viewport = scene_view.viewportGadget()
        scene_gadget = viewport.getPrimaryChild()
        if scene_gadget is not None and hasattr(scene_gadget, "waitForCompletion"):
            scene_gadget.waitForCompletion()
        viewport.preRenderSignal()(viewport)
        viewport.renderRequestSignal()(viewport)
        if scene_gadget is not None and hasattr(scene_gadget, "waitForCompletion"):
            scene_gadget.waitForCompletion()
        viewport.renderRequestSignal()(viewport)
    except Exception as exc:
        _log_warning(f"Failed to prime SceneView after {reason}: {exc}")


def _retarget_scene_views(script, output_plug, frame_paths=None):
    try:
        scene_views, viewers = _ensure_scene_viewers(script)
    except Exception as exc:
        _log_warning(f"Failed to acquire SceneViews for point-cloud demo output: {exc}")
        return []

    if not scene_views:
        return []

    frame_filter = IECore.PathMatcher(frame_paths or []) if frame_paths else None
    retargeted = []
    output_node = output_plug.node()
    for scene_view, viewer in zip(scene_views, viewers):
        try:
            viewer.setNodeSet(Gaffer.StandardSet([output_node]))
        except Exception:
            pass

        scene_view["in"].setInput(output_plug)
        try:
            if scene_view["minimumExpansionDepth"].getValue() < 2:
                scene_view["minimumExpansionDepth"].setValue(2)
        except Exception:
            pass
        if frame_filter is not None:
            try:
                scene_view.frame(frame_filter, direction=imath.V3f(0, 0, 1))
            except Exception as exc:
                _log_warning(f"Failed to frame SceneView after retargeting: {exc}")
        _prime_scene_view(scene_view, output_node.getName())
        retargeted.append(scene_view)
    return retargeted


def _create_demo_opengl_attributes(script, base_name, upstream_scene):
    node = GafferScene.OpenGLAttributes(_unique_node_name(script, base_name))
    script.addChild(node)
    node["in"].setInput(upstream_scene)

    primitive_points = _attribute_plug(node["attributes"], "gl:primitive:points", True)
    primitive_points["enabled"].setValue(True)
    primitive_points["value"].setValue(True)

    primitive_point_width = _attribute_plug(
        node["attributes"], "gl:primitive:pointWidth", 12.0
    )
    primitive_point_width["enabled"].setValue(True)
    primitive_point_width["value"].setValue(12.0)

    use_gl_points = _attribute_plug(
        node["attributes"], "gl:pointsPrimitive:useGLPoints", "forAll"
    )
    use_gl_points["enabled"].setValue(True)
    use_gl_points["value"].setValue("forAll")

    gl_point_width = _attribute_plug(
        node["attributes"], "gl:pointsPrimitive:glPointWidth", 24.0
    )
    gl_point_width["enabled"].setValue(True)
    gl_point_width["value"].setValue(24.0)
    return node


def _finalize_demo(script, scene_output, output_location):
    _retarget_scene_views(script, scene_output, frame_paths=[output_location])


def _finalize_demo_scene(script, graph_editor, nodes, focus, output_plug, frame_paths):
    with Gaffer.UndoScope(script):
        script.selection().clear()
        script.selection().add(nodes)
        script.setFocus(focus)

    if graph_editor is not None:
        graph_editor.frame(nodes, extend=True)

    _retarget_scene_views(script, output_plug, frame_paths=frame_paths)


def create_pointcloud_plus(menu=None):
    script = _script_node(menu)
    node = GafferPointCloudPlus.PointCloudPlus(
        _unique_node_name(script, "PointCloudPlus")
    )
    script.addChild(node)
    with Gaffer.UndoScope(script):
        script.selection().clear()
        script.selection().add(node)
        script.setFocus(node)
    return node


def build_geometry_demo(menu=None):
    script = _script_node(menu)
    graph_editor = _graph_editor(menu)

    plane = GafferScene.Plane(_unique_node_name(script, "PointCloudPlusPlane"))
    plane["name"].setValue("pointCloudSupport")
    script.addChild(plane)

    point_cloud = GafferPointCloudPlus.PointCloudPlus(
        _unique_node_name(script, "PointCloudPlusGeometry")
    )
    script.addChild(point_cloud)
    point_cloud["in"].setInput(plane["out"])
    point_cloud["outputLocation"].setValue("/pointCloudSupport/pointCloud")
    point_cloud["filter"].setValue("/pointCloudSupport")
    point_cloud["distribution"].setValue(0)
    point_cloud["pointCount"].setValue(128)
    open_gl = _create_demo_opengl_attributes(
        script, "PointCloudPlusGeometryOpenGL", point_cloud["out"]
    )
    _finalize_demo_scene(
        script,
        graph_editor,
        [plane, point_cloud, open_gl],
        open_gl,
        open_gl["out"],
        ["/pointCloudSupport", "/pointCloudSupport/pointCloud"],
    )
    return point_cloud


def build_primitive_center_demo(menu=None):
    script = _script_node(menu)
    graph_editor = _graph_editor(menu)

    sphere = GafferScene.Sphere(_unique_node_name(script, "PointCloudPlusSphere"))
    sphere["name"].setValue("pointCloudSupport")
    script.addChild(sphere)

    point_cloud = GafferPointCloudPlus.PointCloudPlus(
        _unique_node_name(script, "PointCloudPlusPrimitiveCenter")
    )
    script.addChild(point_cloud)
    point_cloud["in"].setInput(sphere["out"])
    point_cloud["outputLocation"].setValue("/pointCloudSupport/pointCloud")
    point_cloud["filter"].setValue("/pointCloudSupport")
    point_cloud["distribution"].setValue(1)
    point_cloud["jittering"].setValue(0.05)
    open_gl = _create_demo_opengl_attributes(
        script, "PointCloudPlusPrimitiveCenterOpenGL", point_cloud["out"]
    )
    _finalize_demo_scene(
        script,
        graph_editor,
        [sphere, point_cloud, open_gl],
        open_gl,
        open_gl["out"],
        ["/pointCloudSupport", "/pointCloudSupport/pointCloud"],
    )
    return point_cloud


def build_file_demo(menu=None):
    script = _script_node(menu)
    graph_editor = _graph_editor(menu)

    plane = GafferScene.Plane(_unique_node_name(script, "PointCloudPlusFilePlane"))
    plane["name"].setValue("pointCloudSupport")
    script.addChild(plane)

    source_points = GafferPointCloudPlus.PointCloudPlus(
        _unique_node_name(script, "PointCloudPlusSource")
    )
    script.addChild(source_points)
    source_points["in"].setInput(plane["out"])
    source_points["outputLocation"].setValue("/pointCloudSupport/sourcePoints")
    source_points["filter"].setValue("/pointCloudSupport")
    source_points["pointCount"].setValue(64)

    republish = GafferPointCloudPlus.PointCloudPlus(
        _unique_node_name(script, "PointCloudPlusFile")
    )
    script.addChild(republish)
    republish["in"].setInput(source_points["out"])
    republish["mode"].setValue(1)
    republish["outputLocation"].setValue("/pointCloudSupport/filePoints")
    republish["primPath"].setValue("/pointCloudSupport/sourcePoints")
    open_gl = _create_demo_opengl_attributes(
        script, "PointCloudPlusFileOpenGL", republish["out"]
    )
    _finalize_demo_scene(
        script,
        graph_editor,
        [plane, source_points, republish, open_gl],
        open_gl,
        open_gl["out"],
        [
            "/pointCloudSupport",
            "/pointCloudSupport/sourcePoints",
            "/pointCloudSupport/filePoints",
        ],
    )
    return republish
