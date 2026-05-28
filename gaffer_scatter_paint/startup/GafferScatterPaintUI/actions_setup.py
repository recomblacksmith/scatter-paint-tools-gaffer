def _default_authored_color():
    return imath.Color3f(0.0, 0.0, 120.0 / 255.0)


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
def ensure_painted_points(script):
    node = _find_painted_points(script)
    if node is not None:
        return node

    node_name = "PaintedPoints"
    suffix = 1
    while node_name in script:
        suffix += 1
        node_name = f"PaintedPoints{suffix}"

    with Gaffer.UndoScope(script):
        node = GafferScatterPaint.PaintedPoints(node_name)
        script.addChild(node)
        node["defaultColor"].setValue(_default_authored_color())
        script.selection().clear()
        script.selection().add(node)
        script.setFocus(node)

    return node


def ensure_attached_points(script, painted_points=None):
    for node in _all_nodes(script):
        if isinstance(node, GafferScatterPaint.AttachedPoints):
            if painted_points is not None:
                node["points"].setInput(painted_points["out"])
            return node

    node_name = "AttachedPoints"
    suffix = 1
    while node_name in script:
        suffix += 1
        node_name = f"AttachedPoints{suffix}"

    with Gaffer.UndoScope(script):
        node = GafferScatterPaint.AttachedPoints(node_name)
        script.addChild(node)
        if painted_points is not None:
            node["points"].setInput(painted_points["out"])

    return node
def ensure_demo_opengl_attributes(script, upstream_scene=None):
    for node in _all_nodes(script):
        if isinstance(node, GafferScene.OpenGLAttributes):
            if upstream_scene is not None:
                node["in"].setInput(upstream_scene)
            return node

    node_name = "ScatterPaintOpenGL"
    suffix = 1
    while node_name in script:
        suffix += 1
        node_name = f"ScatterPaintOpenGL{suffix}"

    node = GafferScene.OpenGLAttributes(node_name)
    script.addChild(node)
    if upstream_scene is not None:
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
    _log_info(
        "Configured ScatterPaintOpenGL with gl:primitive:points=On, gl:primitive:pointWidth=12, gl:pointsPrimitive:useGLPoints=forAll, gl:pointsPrimitive:glPointWidth=24."
    )
    return node


def _ensure_demo_setup(script):
    plane_name = "ScatterPaintPlane"
    with Gaffer.UndoScope(script):
        if plane_name not in script:
            plane = GafferScene.Plane(plane_name)
            plane["name"].setValue("paintPlane")
            script.addChild(plane)
        else:
            plane = script[plane_name]

        painted_points = ensure_painted_points(script)
        attached_points = ensure_attached_points(script, painted_points=painted_points)
        open_gl_attributes = ensure_demo_opengl_attributes(
            script, upstream_scene=attached_points["out"]
        )

        painted_points["in"].setInput(plane["out"])
        attached_points["in"].setInput(plane["out"])
        attached_points["points"].setInput(painted_points["out"])
        attached_points["outputLocation"].setValue("/paintPlane/scatter")
        open_gl_attributes["in"].setInput(attached_points["out"])

    _retarget_scene_views(
        script,
        open_gl_attributes["out"],
        frame_paths=["/paintPlane", "/paintPlane/scatter"],
    )

    return plane, painted_points, attached_points, open_gl_attributes


def _mutate_authored_points(script, mutator):
    painted_points = _find_painted_points(script)
    if painted_points is None:
        raise RuntimeError("No PaintedPoints node found to mutate.")
    with Gaffer.UndoScope(script):
        updated_count = painted_points.mutatePoints(mutator)

    return painted_points, updated_count


def _ensure_attachment_backup(point):
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
