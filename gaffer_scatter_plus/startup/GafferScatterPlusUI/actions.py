import Gaffer
import GafferImage
import GafferScene
import GafferSceneUI
import GafferUI
import IECore
import imath

import GafferScatterPlus


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
        raise RuntimeError("Scatter Plus actions require a ScriptWindow.")
    return script_window.scriptNode()


def _graph_editor(menu):
    if isinstance(menu, Gaffer.ScriptNode) or menu is None:
        return None
    return menu.ancestor(GafferUI.GraphEditor)


def _log_warning(message):
    IECore.msg(IECore.Msg.Level.Warning, "GafferScatterPlusUI", message)
    print(f"WARNING : GafferScatterPlusUI : {message}", flush=True)


def _scene_views_for_script(script):
    script_window = GafferUI.ScriptWindow.acquire(script, createIfNecessary=False)
    if script_window is None:
        return [], []
    layout = script_window.getLayout()
    viewers = [viewer for viewer in layout.editors(type=GafferUI.Viewer) if isinstance(viewer.view(), GafferSceneUI.SceneView)]
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
        _log_warning(f"Failed to acquire SceneViews for scatter-plus demo output: {exc}")
        return []

    if not scene_views:
        return []

    frame_filter = IECore.PathMatcher(frame_paths or []) if frame_paths else None
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
    return scene_views


def _create_demo_opengl_attributes(script, base_name, upstream_scene):
    node = GafferScene.OpenGLAttributes(_unique_node_name(script, base_name))
    script.addChild(node)
    node["in"].setInput(upstream_scene)

    primitive_points = _attribute_plug(node["attributes"], "gl:primitive:points", True)
    primitive_points["enabled"].setValue(True)
    primitive_points["value"].setValue(True)

    primitive_point_width = _attribute_plug(node["attributes"], "gl:primitive:pointWidth", 12.0)
    primitive_point_width["enabled"].setValue(True)
    primitive_point_width["value"].setValue(12.0)

    use_gl_points = _attribute_plug(node["attributes"], "gl:pointsPrimitive:useGLPoints", "forAll")
    use_gl_points["enabled"].setValue(True)
    use_gl_points["value"].setValue("forAll")

    gl_point_width = _attribute_plug(node["attributes"], "gl:pointsPrimitive:glPointWidth", 24.0)
    gl_point_width["enabled"].setValue(True)
    gl_point_width["value"].setValue(24.0)
    return node


def _finalize_demo_scene(script, graph_editor, nodes, focus, output_plug, frame_paths):
    with Gaffer.UndoScope(script):
        script.selection().clear()
        script.selection().add(nodes)
        script.setFocus(focus)
    if graph_editor is not None:
        graph_editor.frame(nodes, extend=True)
    _retarget_scene_views(script, output_plug, frame_paths=frame_paths)


def create_scatter_plus(menu=None):
    script = _script_node(menu)
    node = GafferScatterPlus.ScatterPlus(_unique_node_name(script, "ScatterPlus"))
    script.addChild(node)
    with Gaffer.UndoScope(script):
        script.selection().clear()
        script.selection().add(node)
        script.setFocus(node)
    return node


def _prototype_library(script):
    sphere = GafferScene.Sphere(_unique_node_name(script, "ScatterPlusPrototypeSphere"))
    sphere["name"].setValue("prototypeSphere")
    script.addChild(sphere)

    plane = GafferScene.Plane(_unique_node_name(script, "ScatterPlusPrototypePlane"))
    plane["name"].setValue("prototypePlane")
    script.addChild(plane)

    group = GafferScene.Group(_unique_node_name(script, "ScatterPlusPrototypeGroup"))
    group["name"].setValue("prototypeLibrary")
    group["in"][0].setInput(sphere["out"])
    group["in"][1].setInput(plane["out"])
    script.addChild(group)
    return sphere, plane, group


def build_image_demo(menu=None):
    script = _script_node(menu)
    graph_editor = _graph_editor(menu)

    support = GafferScene.Plane(_unique_node_name(script, "ScatterPlusSupport"))
    support["name"].setValue("support")
    support["divisions"].setValue(imath.V2i(32, 32))
    script.addChild(support)

    sphere, plane, prototypes = _prototype_library(script)

    checker = GafferImage.Checkerboard(_unique_node_name(script, "ScatterPlusDensityImage"))
    script.addChild(checker)
    checker["format"].setValue(GafferImage.Format(256, 256, 1.0))
    checker["size"].setValue(imath.V2f(0.08, 0.08))
    checker["colorA"].setValue(imath.Color4f(0.1, 0.1, 0.1, 1.0))
    checker["colorB"].setValue(imath.Color4f(1.0, 1.0, 1.0, 1.0))

    scatter = GafferScatterPlus.ScatterPlus(_unique_node_name(script, "ScatterPlusGeometry"))
    script.addChild(scatter)
    scatter["in"][0].setInput(support["out"])
    scatter["in"][1].setInput(prototypes["out"])
    scatter["image"].setInput(checker["out"])
    scatter["outputLocation"].setValue("/scatterPlus")
    scatter["support"].setValue("/support")
    scatter["prototypeRoots"].setValue("/prototypeLibrary/prototypeSphere /prototypeLibrary/prototypePlane")
    scatter["distribution"].setValue(2)
    scatter["prototypeMode"].setValue(5)
    scatter["pointCount"].setValue(250)
    scatter["density"].setValue(1.0)
    scatter["useSupportNormals"].setValue(True)
    scatter["rotationVariance"].setValue(imath.V3f(15.0, 180.0, 15.0))
    scatter["scale"].setValue(imath.V3f(0.35, 0.35, 0.35))
    scatter["scaleVariance"].setValue(imath.V3f(0.15, 0.15, 0.15))

    open_gl = _create_demo_opengl_attributes(script, "ScatterPlusOpenGL", scatter["out"])
    _finalize_demo_scene(
        script,
        graph_editor,
        [support, sphere, plane, prototypes, checker, scatter, open_gl],
        open_gl,
        open_gl["out"],
        ["/scatterPlus", "/scatterPlus/points", "/scatterPlus/instances"],
    )
    return scatter


def build_geometry_demo(menu=None):
    return build_image_demo(menu)
