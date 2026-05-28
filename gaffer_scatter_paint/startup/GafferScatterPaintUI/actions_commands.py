def build_paint_erase_basics_demo(menu):
    script = _script_node(menu)
    return build_paint_erase_basics_demo_for_script(
        script, graph_editor=_graph_editor(menu)
    )


def build_paint_erase_cyclo_demo(menu):
    script = _script_node(menu)
    return build_paint_erase_cyclo_demo_for_script(
        script, graph_editor=_graph_editor(menu)
    )


def build_paint_through_erase_space_demo(menu):
    script = _script_node(menu)
    return build_paint_through_erase_space_demo_for_script(
        script, graph_editor=_graph_editor(menu)
    )


def build_relax_reproject_demo(menu):
    script = _script_node(menu)
    return build_relax_reproject_demo_for_script(
        script, graph_editor=_graph_editor(menu)
    )


def build_layer_modes_timing_demo(menu):
    script = _script_node(menu)
    return build_layer_modes_timing_demo_for_script(
        script, graph_editor=_graph_editor(menu)
    )


def build_layer_stroke_edit_demo(menu):
    script = _script_node(menu)
    return build_layer_stroke_edit_demo_for_script(
        script, graph_editor=_graph_editor(menu)
    )


def build_box_instance_scatter_demo(menu):
    script = _script_node(menu)
    return build_box_instance_scatter_demo_for_script(
        script, graph_editor=_graph_editor(menu)
    )


def build_benchmark_scene(menu):
    script = _script_node(menu)
    return build_benchmark_scene_for_script(script, graph_editor=_graph_editor(menu))


def seed_small_benchmark_stroke(menu):
    script = _script_node(menu)
    return seed_small_benchmark_stroke_for_script(
        script, graph_editor=_graph_editor(menu)
    )


def seed_medium_benchmark_stroke(menu):
    script = _script_node(menu)
    return seed_medium_benchmark_stroke_for_script(
        script, graph_editor=_graph_editor(menu)
    )


def seed_large_benchmark_stroke(menu):
    script = _script_node(menu)
    return seed_large_benchmark_stroke_for_script(
        script, graph_editor=_graph_editor(menu)
    )


def seed_xl_benchmark_stroke(menu):
    script = _script_node(menu)
    return seed_xl_benchmark_stroke_for_script(script, graph_editor=_graph_editor(menu))


def seed_xxl_benchmark_stroke(menu):
    script = _script_node(menu)
    return seed_xxl_benchmark_stroke_for_script(
        script, graph_editor=_graph_editor(menu)
    )


def seed_stress_benchmark_stroke(menu):
    script = _script_node(menu)
    return seed_stress_benchmark_stroke_for_script(
        script, graph_editor=_graph_editor(menu)
    )


def create_painted_points(menu):
    script = _script_node(menu)
    node = ensure_painted_points(script)
    graph_editor = _graph_editor(menu)
    if graph_editor is not None:
        graph_editor.frame([node], extend=True)
    return node


def create_attached_points(menu):
    script = _script_node(menu)
    painted_points = ensure_painted_points(script)
    node = ensure_attached_points(script, painted_points=painted_points)
    graph_editor = _graph_editor(menu)
    if graph_editor is not None:
        graph_editor.frame([node], extend=True)
    return node


def commit_demo_stroke(menu):
    script = _script_node(menu)
    tool = _tool_for_menu(menu, require_active=True)
    if tool is None:
        try:
            painted_points, attached_points = _require_scatter_nodes(script)
        except RuntimeError as exc:
            IECore.msg(
                IECore.Msg.Level.Warning,
                "GafferScatterPaintUI",
                f"Commit Demo Stroke requires an existing scatter graph: {exc}",
            )
            return 0

        _log_info(
            "No active PaintPointsTool was available for Commit Demo Stroke; using authored-store fallback instead."
        )
        if not _supports_authored_store_api(painted_points):
            IECore.msg(
                IECore.Msg.Level.Warning,
                "GafferScatterPaintUI",
                "Compiled PaintedPoints node does not expose the Python authored-store demo API yet.",
            )
            return 0

        layer_id = painted_points.ensureLayer("Layer 1")
        stroke_id = painted_points.ensureStroke(layer_id, "Stroke 1")
        output_location = attached_points["outputLocation"].getValue().strip()
        source_path = "/"
        if output_location and output_location != "/":
            source_path = output_location.rsplit("/", 1)[0] or "/"

        committed = painted_points.paintStrokeCommit(
            stroke_id,
            [
                {
                    "P": [0.0, 0.0, 0.0],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 0,
                    "sourcePath": source_path,
                    "triangleIndex": 0,
                    "barycentric": [0.5, 0.25, 0.25],
                    "attachmentResolved": True,
                },
                {
                    "P": [0.2, 0.0, 0.2],
                    "width": 0.1,
                    "scale": 1.0,
                    "seed": 1,
                    "sourcePath": source_path,
                    "triangleIndex": 1,
                    "barycentric": [0.25, 0.5, 0.25],
                    "attachmentResolved": True,
                },
            ],
            append=True,
        )
        IECore.msg(
            IECore.Msg.Level.Info,
            "GafferScatterPaintUI",
            f"Committed {committed} demo points via fallback action.",
        )
        return committed

    committed = tool.commitDemoStroke()
    IECore.msg(
        IECore.Msg.Level.Info,
        "GafferScatterPaintUI",
        f"Committed {committed} placeholder points via compiled tool action.",
    )
    return committed


def erase_last_stroke(menu):
    tool = _tool_for_menu(menu, require_active=True)
    if tool is None:
        script = _script_node(menu)
        painted_points = _find_painted_points(script)
        if painted_points is None:
            IECore.msg(
                IECore.Msg.Level.Warning,
                "GafferScatterPaintUI",
                "No PaintedPoints node found to erase from.",
            )
            return 0

        if not _supports_authored_store_api(painted_points):
            IECore.msg(
                IECore.Msg.Level.Warning,
                "GafferScatterPaintUI",
                "Compiled PaintedPoints node does not expose the Python erase demo API yet.",
            )
            return 0

        last_stroke_id = painted_points.lastStrokeId()
        if last_stroke_id is None:
            return 0

        removed = painted_points.eraseCommit(last_stroke_id, fraction=1.0)
        IECore.msg(
            IECore.Msg.Level.Info,
            "GafferScatterPaintUI",
            f"Erased {removed} authored points via fallback action.",
        )
        return removed

    removed = tool.eraseLastStroke()
    IECore.msg(
        IECore.Msg.Level.Info,
        "GafferScatterPaintUI",
        f"Erased {removed} points via compiled tool action.",
    )
    return removed


def frame_scatter_nodes(menu):
    script = _script_node(menu)
    graph_editor = _graph_editor(menu)
    if graph_editor is None:
        return []

    nodes = [
        node
        for node in _all_nodes(script)
        if isinstance(
            node, (GafferScatterPaint.PaintedPoints, GafferScatterPaint.AttachedPoints)
        )
    ]
    if nodes:
        graph_editor.frame(nodes, extend=True)
    return nodes


def build_demo_graph(menu):
    script = _script_node(menu)
    graph_editor = _graph_editor(menu)
    return build_demo_graph_for_script(script, graph_editor=graph_editor)


def build_demo_graph_for_script(script, graph_editor=None):
    plane, painted_points, attached_points, open_gl_attributes = _ensure_demo_setup(
        script
    )

    if not _supports_authored_store_api(painted_points):
        _log_warning(
            "Built demo graph without authored-store helper support because the compiled PaintedPoints node does not expose the Python authored-store API yet."
        )

    with Gaffer.UndoScope(script):
        script.selection().clear()
        script.selection().add(
            [plane, painted_points, attached_points, open_gl_attributes]
        )
        script.setFocus(open_gl_attributes)

    if graph_editor is not None:
        graph_editor.frame(
            [plane, painted_points, attached_points, open_gl_attributes], extend=True
        )

    _log_info(
        "Built scatter paint demo graph with plane, PaintedPoints, AttachedPoints, and OpenGLAttributes, retargeted open SceneViews, and framed /paintPlane plus /paintPlane/scatter. The graph now starts empty and waits for user paint input."
    )

    return open_gl_attributes


def _require_scatter_nodes(script):
    painted_points = _find_painted_points(script)
    if painted_points is None:
        raise RuntimeError("No PaintedPoints node found.")

    attached_points = _find_attached_points(script, painted_points=painted_points)
    if attached_points is None:
        raise RuntimeError("No AttachedPoints node found for the current PaintedPoints.")

    return painted_points, attached_points


def validate_attachments(menu):
    script = _script_node(menu)
    painted_points, attached_points = _require_scatter_nodes(script)

    output_location = attached_points["outputLocation"].getValue().strip() or "/scatter"
    scatter_object = None
    scatter_count = 0
    scatter_bound = None
    try:
        scatter_object = attached_points["out"].object(output_location)
    except Exception as exc:
        if "does not exist" not in str(exc):
            IECore.msg(
                IECore.Msg.Level.Warning,
                "GafferScatterPaintUI",
                f"Attachment validation evaluation failed at {output_location}: {exc}",
            )

    if scatter_object is not None and "P" in scatter_object:
        scatter_count = len(scatter_object["P"].data)
        scatter_bound = _box_from_positions(scatter_object["P"].data)

    summary = attached_points["solveStatus"].getValue()
    resolved = attached_points["resolvedPointCount"].getValue()
    unresolved = attached_points["unresolvedPointCount"].getValue()
    invalid = attached_points["invalidPointCount"].getValue()
    topology = attached_points["topologyMismatchCount"].getValue()
    failure_reasons = list(attached_points["attachmentFailureReasons"].getValue())
    failing_paths = list(attached_points["failingTargetPaths"].getValue())
    authored_count = len(list(painted_points.pointRecords()))

    message = (
        f"Attachment validation: {summary} | "
        f"authored={authored_count}, scatter={scatter_count}, resolved={resolved}, unresolved={unresolved}, invalid={invalid}, topologyMismatches={topology}"
    )
    if scatter_bound is not None:
        message += f" | scatterBound={_format_box3f(scatter_bound)}"
    if failure_reasons:
        message += f" | failureReasons={', '.join(failure_reasons)}"
    if failing_paths:
        message += f" | failingPaths={', '.join(failing_paths)}"

    IECore.msg(
        IECore.Msg.Level.Info,
        "GafferScatterPaintUI",
        message,
    )
    return {
        "paintedPoints": painted_points.getName(),
        "attachedPoints": attached_points.getName(),
        "outputLocation": output_location,
        "authored": authored_count,
        "scatter": scatter_count,
        "summary": summary,
        "resolved": resolved,
        "unresolved": unresolved,
        "invalid": invalid,
        "topologyMismatches": topology,
        "failureReasons": failure_reasons,
        "failingPaths": failing_paths,
    }


def inspect_scatter_positions(menu):
    script = _script_node(menu)
    painted_points, attached_points = _require_scatter_nodes(script)

    point_records = list(painted_points.pointRecords())
    point_records.sort(key=lambda point: int(point.get("pointId", 0)))
    authored_positions = [
        imath.V3f(*point.get("P", [0.0, 0.0, 0.0])) for point in point_records
    ]
    authored_box = _box_from_positions(authored_positions)

    output_location = attached_points["outputLocation"].getValue().strip() or "/scatter"
    scatter_positions = []
    scatter_box = None
    scatter_missing = False
    try:
        scatter_object = attached_points["out"].object(output_location)
    except Exception as exc:
        scatter_object = None
        scatter_missing = "does not exist" in str(exc)
        if not scatter_missing:
            raise
    if scatter_object is not None and "P" in scatter_object:
        scatter_positions = [imath.V3f(value) for value in scatter_object["P"].data]
        scatter_box = _box_from_positions(scatter_positions)

    inspection_node, inspection_scene = _find_inspection_scene_output(
        script, output_location, attached_points=attached_points
    )
    scene = inspection_scene if inspection_scene is not None else attached_points["out"]
    inspected_node_name = (
        inspection_node.getName()
        if inspection_node is not None
        else attached_points.getName()
    )
    try:
        attached_bound = scene.bound(output_location)
    except Exception as exc:
        if "does not exist" not in str(exc):
            raise
        attached_bound = None
    scatter_hierarchy = []
    if attached_bound is not None:
        for path in _scene_descendant_paths(scene, output_location):
            try:
                path_bound = scene.bound(path)
            except Exception:
                path_bound = None
            try:
                path_transform = scene.transform(path)
            except Exception:
                path_transform = None
            scatter_hierarchy.append(
                {
                    "path": path,
                    "bound": path_bound,
                    "transform": path_transform,
                }
            )

    lines = [
        f"PaintedPoints node : {painted_points.getName()}",
        f"AttachedPoints node: {attached_points.getName()}",
        f"Inspect scene node  : {inspected_node_name}",
        f"Output location    : {output_location}",
        f"Authored points    : {len(authored_positions)}",
        f"Scatter points     : {len(scatter_positions)}",
        f"Solve status       : {attached_points['solveStatus'].getValue()}",
        f"Resolved count     : {attached_points['resolvedPointCount'].getValue()}",
        f"Unresolved count   : {attached_points['unresolvedPointCount'].getValue()}",
        f"Invalid count      : {attached_points['invalidPointCount'].getValue()}",
        f"Authored bounds    : {_format_box3f(authored_box)}",
        f"Scatter bounds     : {_format_box3f(scatter_box) if scatter_box is not None else 'n/a'}",
        f"Scene bound        : {_format_box3f(attached_bound)}",
        "Scatter hierarchy  :",
    ]
    if scatter_missing:
        lines.append("  output location does not exist in the current scene yet")
    lines.extend(
        f"  {entry['path']} | bound={_format_box3f(entry['bound']) if entry['bound'] is not None else 'n/a'} | transform={entry['transform']}"
        for entry in scatter_hierarchy
    )
    lines.extend(
        [
            "Authored positions :",
        ]
    )
    lines.extend(f"  {line}" for line in _summarize_point_positions(authored_positions))
    lines.append("Scatter positions  :")
    lines.extend(f"  {line}" for line in _summarize_point_positions(scatter_positions))
    message = "\n".join(lines)
    IECore.msg(IECore.Msg.Level.Info, "GafferScatterPaintUI", message)
    print(message, flush=True)
    return {
        "paintedPoints": painted_points.getName(),
        "attachedPoints": attached_points.getName(),
        "inspectSceneNode": inspected_node_name,
        "outputLocation": output_location,
        "authoredCount": len(authored_positions),
        "scatterCount": len(scatter_positions),
        "solveStatus": attached_points["solveStatus"].getValue(),
        "authoredBounds": authored_box,
        "scatterBounds": scatter_box,
        "sceneBound": attached_bound,
        "scatterHierarchy": scatter_hierarchy,
        "authoredPositions": authored_positions,
        "scatterPositions": scatter_positions,
    }


def break_attachment_paths(menu):
    script = _script_node(menu)

    def mutate(point):
        source_path = point.get("sourcePath")
        if not source_path:
            return False
        _ensure_attachment_backup(point)
        if source_path == "/":
            point["sourcePath"] = "/Missing"
        else:
            point["sourcePath"] = f"{source_path.rstrip('/')}Missing"
        point["attachmentResolved"] = True
        return True

    _painted_points, updated = _mutate_authored_points(script, mutate)
    IECore.msg(
        IECore.Msg.Level.Info,
        "GafferScatterPaintUI",
        f"Mutated {updated} authored points to use a missing source path.",
    )
    return updated


def break_attachment_triangles(menu):
    script = _script_node(menu)

    def mutate(point):
        _ensure_attachment_backup(point)
        point["triangleIndex"] = int(point.get("triangleIndex", 0)) + 100000
        point["attachmentResolved"] = True
        return True

    _painted_points, updated = _mutate_authored_points(script, mutate)
    IECore.msg(
        IECore.Msg.Level.Info,
        "GafferScatterPaintUI",
        f"Mutated {updated} authored points to use invalid triangle indices.",
    )
    return updated


def repair_demo_attachments(menu):
    script = _script_node(menu)

    def mutate(point):
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

    _painted_points, updated = _mutate_authored_points(script, mutate)
    IECore.msg(
        IECore.Msg.Level.Info,
        "GafferScatterPaintUI",
        f"Repaired {updated} authored points from demo attachment backups.",
    )
    return updated
