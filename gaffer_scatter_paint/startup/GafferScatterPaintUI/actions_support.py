import os


def _diagnostics_enabled():
    value = os.getenv("GAFFER_SCATTER_PAINT_DIAGNOSTICS", "")
    return bool(value) and value not in {"0", "false", "FALSE"}


def _log_info(message):
    if not _diagnostics_enabled():
        return
    IECore.msg(IECore.Msg.Level.Info, "GafferScatterPaintUI", message)
    print(f"INFO : GafferScatterPaintUI : {message}", flush=True)


def _log_warning(message):
    IECore.msg(IECore.Msg.Level.Warning, "GafferScatterPaintUI", message)
    print(f"WARNING : GafferScatterPaintUI : {message}", flush=True)


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
        _log_info(f"Primed SceneView after {reason}.")
    except Exception as exc:
        _log_warning(f"Failed to prime SceneView after {reason}: {exc}")


def _script_node(menu):
    script_window = menu.ancestor(GafferUI.ScriptWindow)
    if script_window is None:
        raise RuntimeError("Scatter paint actions require a ScriptWindow.")
    return script_window.scriptNode()


def _graph_editor(menu):
    return menu.ancestor(GafferUI.GraphEditor)


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
    scene_views = [viewer.view() for viewer in viewers]
    return scene_views, viewers


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
        _log_warning(
            "Created Viewer but it did not provide a SceneView yet; demo retarget will retry existing viewers only."
        )
        return _scene_views_for_script(script)

    _log_info("Created a new Viewer for scatter-paint demo output.")
    return [view], [viewer]


def _retarget_scene_views(script, output_plug, frame_paths=None):
    try:
        scene_views, viewers = _ensure_scene_viewers(script)
    except Exception as exc:
        _log_warning(f"Failed to acquire SceneViews for demo output: {exc}")
        return []
    if not scene_views:
        _log_info("No open SceneViews found to retarget.")
        return []

    frame_filter = None
    if frame_paths:
        frame_filter = IECore.PathMatcher(frame_paths)
        _log_info(f"Requested SceneView frame paths: {', '.join(frame_paths)}")

    retargeted = []
    output_node = output_plug.node()

    for scene_view, viewer in zip(scene_views, viewers):
        if output_node is not None:
            try:
                viewer.setNodeSet(Gaffer.StandardSet([output_node]))
                _log_info(f"Viewer node set now targets {output_node.getName()}.")
            except Exception as exc:
                _log_warning(f"Failed to retarget Viewer node set: {exc}")

        scene_view["in"].setInput(output_plug)
        _log_info(
            f"SceneView input now connected to {output_plug.node().getName()}.{output_plug.getName()}"
        )
        try:
            current_depth = scene_view["minimumExpansionDepth"].getValue()
            if current_depth < 2:
                scene_view["minimumExpansionDepth"].setValue(2)
        except Exception as exc:
            _log_warning(f"Failed to raise SceneView minimumExpansionDepth: {exc}")
        if frame_filter is not None:
            try:
                scene_view.frame(frame_filter, direction=imath.V3f(0, 0, 1))
            except Exception as exc:
                _log_warning(f"Failed to frame SceneView after retargeting: {exc}")
        try:
            input_node = scene_view["in"].source().node()
            input_name = input_node.getName() if input_node is not None else "<none>"
        except Exception:
            input_name = "<unknown>"
        _log_info(
            f"Retargeted SceneView to {input_name}.out with minimumExpansionDepth={scene_view['minimumExpansionDepth'].getValue()}"
        )
        _prime_scene_view(scene_view, f"retargeting to {input_name}.out")
        retargeted.append(scene_view)

    return retargeted


def _all_nodes(parent):
    for child in parent.children():
        yield child
        if isinstance(child, Gaffer.GraphComponent):
            for descendant in _all_nodes(child):
                yield descendant


def _find_painted_points(script):
    focus = script.getFocus()
    if isinstance(focus, GafferScatterPaint.PaintedPoints):
        return focus

    for node in script.selection():
        if isinstance(node, GafferScatterPaint.PaintedPoints):
            return node

    for node in _all_nodes(script):
        if isinstance(node, GafferScatterPaint.PaintedPoints):
            return node

    return None


def _scene_descendant_paths(scene, root_path, max_depth=4, limit=32):
    paths = []

    def walk(path, depth):
        if len(paths) >= limit:
            return
        paths.append(path)
        if depth >= max_depth:
            return
        try:
            child_names = list(scene.childNames(path))
        except Exception:
            child_names = []
        for child_name in child_names:
            child_path = f"{path}/{child_name}"
            walk(child_path, depth + 1)

    walk(root_path, 0)
    return paths


def _scene_output_plug(node):
    if node is None:
        return None
    try:
        output = node["out"]
    except Exception:
        return None
    if isinstance(output, GafferScene.ScenePlug):
        return output
    return None


def _scene_has_path(scene, path):
    try:
        return bool(scene.exists(path))
    except Exception:
        return False


def _find_inspection_scene_output(script, output_location, attached_points=None):
    candidates = []

    focus = script.getFocus()
    if focus is not None:
        candidates.append(focus)

    candidates.extend(list(script.selection()))
    candidates.extend(list(_all_nodes(script)))

    attached_output = attached_points["out"] if attached_points is not None else None
    seen = set()

    for node in candidates:
        if node is None:
            continue
        node_id = id(node)
        if node_id in seen:
            continue
        seen.add(node_id)

        scene = _scene_output_plug(node)
        if scene is None or scene.isSame(attached_output):
            continue
        if _scene_has_path(scene, output_location):
            return node, scene

    return None, None


def _find_attached_points(script, painted_points=None):
    selected = []
    if painted_points is not None:
        selected.append(painted_points)

    focus = script.getFocus()
    if isinstance(focus, GafferScatterPaint.AttachedPoints):
        return focus

    for node in script.selection():
        if isinstance(node, GafferScatterPaint.AttachedPoints):
            return node

    for node in _all_nodes(script):
        if not isinstance(node, GafferScatterPaint.AttachedPoints):
            continue
        if painted_points is None:
            return node
        try:
            source = node["points"].source().node()
        except Exception:
            source = None
        if source is painted_points:
            return node

    return None


def _box_from_positions(positions):
    box = imath.Box3f()
    box.makeEmpty()
    for position in positions:
        box.extendBy(imath.V3f(position))
    return box


def _format_v3f(value):
    vector = imath.V3f(value)
    return f"({vector.x:.4f}, {vector.y:.4f}, {vector.z:.4f})"


def _format_box3f(box):
    return f"min={_format_v3f(box.min())} max={_format_v3f(box.max())}"


def _summarize_point_positions(points, limit=8):
    summary = []
    for index, point in enumerate(list(points)[:limit]):
        summary.append(f"{index}: {_format_v3f(point)}")
    return summary


def _tool_for_menu(menu, require_active=False):
    viewer = menu.ancestor(GafferUI.Viewer)
    if viewer is not None and viewer.view() is not None:
        try:
            tools = viewer.view()["tools"]
            for tool in tools.children():
                if isinstance(tool, GafferScatterPaintUI.PaintPointsTool):
                    if require_active and not tool["active"].getValue():
                        continue
                    return tool
        except Exception:
            pass

    try:
        script = _script_node(menu)
        scene_views, _viewers = _scene_views_for_script(script)
        fallback = None
        for scene_view in scene_views:
            tools = scene_view["tools"]
            for tool in tools.children():
                if isinstance(tool, GafferScatterPaintUI.PaintPointsTool):
                    if tool["active"].getValue():
                        return tool
                    if fallback is None and not require_active:
                        fallback = tool
        return fallback
    except Exception:
        return None


def _supports_authored_store_api(node):
    required = (
        "ensureLayer",
        "ensureStroke",
        "paintStrokeCommit",
        "eraseCommit",
        "lastStrokeId",
        "mutatePoints",
    )
    return all(callable(getattr(node, name, None)) for name in required)


def _supports_demo_api(node, *required):
    return all(callable(getattr(node, name, None)) for name in required)


def _require_demo_api(painted_points, label, *required):
    if _supports_demo_api(painted_points, *required):
        return
    missing = [
        name for name in required if not callable(getattr(painted_points, name, None))
    ]
    raise RuntimeError(
        f"{label} requires PaintedPoints demo methods: {', '.join(missing)}"
    )


def _supports_showcase_demo_api(node):
    required = (
        "ensureLayer",
        "ensureStroke",
        "paintStrokeCommit",
        "mutatePoints",
        "pointRecords",
        "strokeRecords",
        "layerRecords",
        "setCurrentSelection",
        "setLayerMute",
        "setLayerSolo",
        "setLayerTimeRange",
        "brushPaintPoints",
        "brushErasePoints",
        "moveStroke",
        "mergeStrokes",
        "deleteStroke",
    )
    return all(callable(getattr(node, name, None)) for name in required)


def _require_showcase_demo_api(painted_points, label):
    if _supports_showcase_demo_api(painted_points):
        return
    raise RuntimeError(
        f"{label} requires the authored-store demo API on PaintedPoints."
    )
