_INSTANCER_VARIATION_SYNC = {}


def _primitive_variable_plug(
    primitive_variables_plug, plug_name, variable_name, default_value
):
    if plug_name in primitive_variables_plug:
        return primitive_variables_plug[plug_name]

    plug = Gaffer.NameValuePlug(
        variable_name,
        default_value,
        True,
        plug_name,
    )
    primitive_variables_plug.addChild(plug)
    return plug


def _prototype_variant_fraction(index, count):
    if count <= 1:
        return 0.0
    return (float(index) / float(count - 1)) * 2.0 - 1.0


def _apply_instancer_variation(
    painted_points,
    attached_points,
    scatter_primvars,
    box_transforms,
    sphere_transforms,
    variation_controls,
):
    import random

    source_points = []
    scatter_path = attached_points["outputLocation"].getValue()
    try:
        scatter_object = attached_points["out"].object(scatter_path)
        positions = list(scatter_object["P"].data)
        seeds_primvar = (
            scatter_object["seed"].data if "seed" in scatter_object else None
        )
        for index, position in enumerate(positions):
            seed = int(seeds_primvar[index]) if seeds_primvar is not None else index
            source_points.append((imath.V3f(position), seed))
    except Exception:
        point_records = list(painted_points.pointRecords())
        point_records.sort(key=lambda point: int(point.get("pointId", 0)))
        for index, point in enumerate(point_records):
            position = imath.V3f(*point.get("P", [0.0, 0.0, 0.0]))
            seed = int(point.get("seed", index))
            source_points.append((position, seed))

    position_variation = variation_controls["positionVariation"].getValue()
    rotation_variation = variation_controls["rotationVariation"].getValue()
    scale_min = min(
        variation_controls["scaleMin"].getValue(),
        variation_controls["scaleMax"].getValue(),
    )
    scale_max = max(
        variation_controls["scaleMin"].getValue(),
        variation_controls["scaleMax"].getValue(),
    )
    sphere_probability = max(
        0.0, min(1.0, variation_controls["sphereProbability"].getValue())
    )

    variant_count = max(1, len(box_transforms), len(sphere_transforms))
    prototype_index = []
    instance_position = []
    instance_scale = []

    for index, (position, seed) in enumerate(source_points):
        rng = random.Random(seed * 10007 + index)

        offset = imath.V3f(
            (rng.random() * 2.0 - 1.0) * position_variation,
            0.0,
            (rng.random() * 2.0 - 1.0) * position_variation,
        )
        instance_position.append(position + offset)
        instance_scale.append(scale_min + (scale_max - scale_min) * rng.random())

        variant = int(rng.random() * variant_count) % variant_count
        is_sphere = rng.random() < sphere_probability
        prototype_index.append(variant_count + variant if is_sphere else variant)

    scatter_primvars["primitiveVariables"]["prototypeIndex"]["value"].setValue(
        IECore.IntVectorData(prototype_index)
    )
    scatter_primvars["primitiveVariables"]["instancePosition"]["value"].setValue(
        IECore.V3fVectorData(
            instance_position, IECore.GeometricData.Interpretation.Point
        )
    )
    scatter_primvars["primitiveVariables"]["instanceScale"]["value"].setValue(
        IECore.FloatVectorData(instance_scale)
    )

    for index, transform in enumerate(box_transforms):
        base = (
            _prototype_variant_fraction(index, len(box_transforms)) * rotation_variation
        )
        transform["transform"]["rotate"]["y"].setValue(base)
    for index, transform in enumerate(sphere_transforms):
        base = (
            _prototype_variant_fraction(index, len(sphere_transforms))
            * rotation_variation
        )
        transform["transform"]["rotate"]["y"].setValue(
            base + (rotation_variation * 0.35)
        )


def _install_instancer_variation_sync(
    painted_points,
    attached_points,
    scatter_primvars,
    box_transforms,
    sphere_transforms,
    variation_controls,
):
    key = id(painted_points)
    existing = _INSTANCER_VARIATION_SYNC.get(key)
    if existing is not None:
        for connection in existing.get("connections", []):
            try:
                connection.disconnect()
            except Exception:
                pass

    state = {"applying": False}

    def apply_variation(*_unused):
        if state["applying"]:
            return
        state["applying"] = True
        try:
            _apply_instancer_variation(
                painted_points,
                attached_points,
                scatter_primvars,
                box_transforms,
                sphere_transforms,
                variation_controls,
            )
        except Exception as exc:
            _log_warning(f"Failed to update scatter demo variation: {exc}")
        finally:
            state["applying"] = False

    connections = [
        painted_points.plugSetSignal().connect(apply_variation, scoped=True),
        variation_controls.plugSetSignal().connect(apply_variation, scoped=True),
    ]
    _INSTANCER_VARIATION_SYNC[key] = {
        "connections": connections,
        "apply": apply_variation,
    }
    return apply_variation


def _unique_node_name(script, base_name):
    node_name = base_name
    suffix = 1
    while node_name in script:
        suffix += 1
        node_name = f"{base_name}{suffix}"
    return node_name


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


def _create_plane_demo_network(script, prefix, scene_name, output_location=None):
    plane = GafferScene.Plane(_unique_node_name(script, f"{prefix}Plane"))
    plane["name"].setValue(scene_name)
    script.addChild(plane)

    painted_points = GafferScatterPaint.PaintedPoints(
        _unique_node_name(script, f"{prefix}PaintedPoints")
    )
    script.addChild(painted_points)
    painted_points["defaultColor"].setValue(_default_authored_color())
    painted_points["targetFilter"].setValue(f"* -*/{scene_name}/scatter*")
    painted_points["in"].setInput(plane["out"])

    attached_points = GafferScatterPaint.AttachedPoints(
        _unique_node_name(script, f"{prefix}AttachedPoints")
    )
    script.addChild(attached_points)
    attached_points["in"].setInput(plane["out"])
    attached_points["points"].setInput(painted_points["out"])
    attached_points["outputLocation"].setValue(
        output_location or f"/{scene_name}/scatter"
    )

    open_gl = _create_demo_opengl_attributes(
        script, f"{prefix}OpenGL", attached_points["out"]
    )
    return plane, painted_points, attached_points, open_gl


def _demo_asset_path(*relative_parts):
    return str(Path(__file__).resolve().parents[2].joinpath(*relative_parts))


def _create_scene_reader_demo_network(
    script,
    prefix,
    asset_path,
    mesh_path,
    output_location=None,
):
    reader = GafferScene.SceneReader(_unique_node_name(script, f"{prefix}Reader"))
    reader["fileName"].setValue(asset_path)
    script.addChild(reader)

    painted_points = GafferScatterPaint.PaintedPoints(
        _unique_node_name(script, f"{prefix}PaintedPoints")
    )
    script.addChild(painted_points)
    painted_points["defaultColor"].setValue(_default_authored_color())
    painted_points["targetFilter"].setValue(
        f"{mesh_path} -{output_location or f'{mesh_path}/scatter'}*"
    )
    painted_points["in"].setInput(reader["out"])

    attached_points = GafferScatterPaint.AttachedPoints(
        _unique_node_name(script, f"{prefix}AttachedPoints")
    )
    script.addChild(attached_points)
    attached_points["in"].setInput(reader["out"])
    attached_points["points"].setInput(painted_points["out"])
    attached_points["outputLocation"].setValue(
        output_location or f"{mesh_path}/scatter"
    )

    open_gl = _create_demo_opengl_attributes(
        script, f"{prefix}OpenGL", attached_points["out"]
    )
    return reader, painted_points, attached_points, open_gl


def _create_overlapping_demo_network(script, prefix, group_name):
    front_plane = GafferScene.Plane(_unique_node_name(script, f"{prefix}FrontPlane"))
    front_plane["name"].setValue("frontPlane")
    script.addChild(front_plane)

    back_plane = GafferScene.Plane(_unique_node_name(script, f"{prefix}BackPlane"))
    back_plane["name"].setValue("backPlane")
    script.addChild(back_plane)

    group = GafferScene.Group(_unique_node_name(script, f"{prefix}Group"))
    group["name"].setValue(group_name)
    script.addChild(group)
    group["in"][0].setInput(front_plane["out"])
    group["in"][1].setInput(back_plane["out"])

    painted_points = GafferScatterPaint.PaintedPoints(
        _unique_node_name(script, f"{prefix}PaintedPoints")
    )
    script.addChild(painted_points)
    painted_points["defaultColor"].setValue(_default_authored_color())
    painted_points["in"].setInput(group["out"])

    attached_points = GafferScatterPaint.AttachedPoints(
        _unique_node_name(script, f"{prefix}AttachedPoints")
    )
    script.addChild(attached_points)
    attached_points["in"].setInput(group["out"])
    attached_points["points"].setInput(painted_points["out"])
    attached_points["outputLocation"].setValue(f"/{group_name}/scatter")

    open_gl = _create_demo_opengl_attributes(
        script, f"{prefix}OpenGL", attached_points["out"]
    )
    return front_plane, back_plane, group, painted_points, attached_points, open_gl


def _create_instanced_demo_network(
    script, prefix, scene_name, prototype_name="scatterBox", output_location=None
):
    variant_count = 4

    plane = GafferScene.Plane(_unique_node_name(script, f"{prefix}Plane"))
    plane["name"].setValue(scene_name)
    plane["dimensions"].setValue(imath.V2f(4.0, 4.0))
    plane["divisions"].setValue(imath.V2i(20, 20))
    plane["transform"]["rotate"].setValue(imath.V3f(-90.0, 0.0, 0.0))
    script.addChild(plane)

    box_prototypes = []
    box_colors = []
    box_transforms = []
    sphere_prototypes = []
    sphere_colors = []
    sphere_transforms = []

    for index in range(variant_count):
        box_prototype = GafferScene.Cube(
            _unique_node_name(script, f"{prefix}BoxPrototype{index}")
        )
        box_prototype["name"].setValue(f"{prototype_name}{index}")
        box_prototype["dimensions"].setValue(imath.V3f(0.18, 0.18, 0.18))
        script.addChild(box_prototype)
        box_prototypes.append(box_prototype)

        box_color = GafferScene.PrimitiveVariables(
            _unique_node_name(script, f"{prefix}BoxColor{index}")
        )
        script.addChild(box_color)
        box_color["in"].setInput(box_prototype["out"])
        box_color["primitiveVariables"].addChild(
            Gaffer.NameValuePlug(
                "Cs",
                IECore.Color3fData(imath.Color3f(0.95, 0.45, 0.2)),
                True,
                f"boxColor{index}",
            )
        )
        box_colors.append(box_color)

        box_transform = GafferScene.Transform(
            _unique_node_name(script, f"{prefix}BoxRotate{index}")
        )
        script.addChild(box_transform)
        box_transform["in"].setInput(box_color["out"])
        box_transforms.append(box_transform)

        sphere_prototype = GafferScene.Sphere(
            _unique_node_name(script, f"{prefix}SpherePrototype{index}")
        )
        sphere_prototype["name"].setValue(f"scatterSphere{index}")
        sphere_prototype["radius"].setValue(0.1)
        script.addChild(sphere_prototype)
        sphere_prototypes.append(sphere_prototype)

        sphere_color = GafferScene.PrimitiveVariables(
            _unique_node_name(script, f"{prefix}SphereColor{index}")
        )
        script.addChild(sphere_color)
        sphere_color["in"].setInput(sphere_prototype["out"])
        sphere_color["primitiveVariables"].addChild(
            Gaffer.NameValuePlug(
                "Cs",
                IECore.Color3fData(imath.Color3f(0.2, 0.6, 0.95)),
                True,
                f"sphereColor{index}",
            )
        )
        sphere_colors.append(sphere_color)

        sphere_transform = GafferScene.Transform(
            _unique_node_name(script, f"{prefix}SphereRotate{index}")
        )
        script.addChild(sphere_transform)
        sphere_transform["in"].setInput(sphere_color["out"])
        sphere_transforms.append(sphere_transform)

    prototypes = GafferScene.Parent(_unique_node_name(script, f"{prefix}Prototypes"))
    script.addChild(prototypes)
    prototypes["in"].setInput(box_transforms[0]["out"])
    child_index = 0
    for transform in box_transforms[1:] + sphere_transforms:
        prototypes["children"][child_index].setInput(transform["out"])
        child_index += 1
    prototypes["parent"].setValue("/")

    painted_points = GafferScatterPaint.PaintedPoints(
        _unique_node_name(script, f"{prefix}PaintedPoints")
    )
    script.addChild(painted_points)
    painted_points["in"].setInput(plane["out"])

    attached_points = GafferScatterPaint.AttachedPoints(
        _unique_node_name(script, f"{prefix}AttachedPoints")
    )
    script.addChild(attached_points)
    attached_points["in"].setInput(plane["out"])
    attached_points["points"].setInput(painted_points["out"])
    attached_points["outputLocation"].setValue(
        output_location or f"/{scene_name}/scatter"
    )

    scatter_filter = GafferScene.PathFilter(
        _unique_node_name(script, f"{prefix}Filter")
    )
    scatter_filter["paths"].setValue(
        IECore.StringVectorData([attached_points["outputLocation"].getValue()])
    )
    script.addChild(scatter_filter)

    scatter_ground_transform = GafferScene.Transform(
        _unique_node_name(script, f"{prefix}ScatterGround")
    )
    script.addChild(scatter_ground_transform)
    scatter_ground_transform["in"].setInput(attached_points["out"])
    scatter_ground_transform["filter"].setInput(scatter_filter["out"])
    scatter_ground_transform["transform"]["rotate"].setValue(imath.V3f(90.0, 0.0, 0.0))

    scatter_primvars = GafferScene.PrimitiveVariables(
        _unique_node_name(script, f"{prefix}ScatterPrimvars")
    )
    script.addChild(scatter_primvars)
    scatter_primvars["in"].setInput(scatter_ground_transform["out"])
    scatter_primvars["filter"].setInput(scatter_filter["out"])

    _primitive_variable_plug(
        scatter_primvars["primitiveVariables"],
        "prototypeIndex",
        "prototypeIndex",
        IECore.IntVectorData(),
    )
    _primitive_variable_plug(
        scatter_primvars["primitiveVariables"],
        "instancePosition",
        "instancePosition",
        IECore.V3fVectorData(),
    )
    _primitive_variable_plug(
        scatter_primvars["primitiveVariables"],
        "instanceScale",
        "instanceScale",
        IECore.FloatVectorData(),
    )

    variation_controls = Gaffer.Node(_unique_node_name(script, f"{prefix}Variation"))
    script.addChild(variation_controls)
    variation_controls.addChild(
        Gaffer.FloatPlug("positionVariation", defaultValue=0.14, minValue=0.0)
    )
    variation_controls.addChild(
        Gaffer.FloatPlug("rotationVariation", defaultValue=45.0, minValue=0.0)
    )
    variation_controls.addChild(
        Gaffer.FloatPlug("scaleMin", defaultValue=0.75, minValue=0.01)
    )
    variation_controls.addChild(
        Gaffer.FloatPlug("scaleMax", defaultValue=1.35, minValue=0.01)
    )
    variation_controls.addChild(
        Gaffer.FloatPlug(
            "sphereProbability", defaultValue=0.5, minValue=0.0, maxValue=1.0
        )
    )

    instancer = GafferScene.Instancer(_unique_node_name(script, f"{prefix}Instancer"))
    script.addChild(instancer)
    instancer["in"].setInput(scatter_primvars["out"])
    instancer["prototypes"].setInput(prototypes["out"])
    instancer["filter"].setInput(scatter_filter["out"])
    instancer["prototypeIndex"].setValue("prototypeIndex")
    instancer["position"].setValue("instancePosition")
    instancer["orientation"].setValue("")
    instancer["scale"].setValue("instanceScale")
    instancer["seedEnabled"].setValue(True)

    open_gl = _create_demo_opengl_attributes(
        script, f"{prefix}OpenGL", instancer["out"]
    )
    return (
        plane,
        box_prototypes,
        box_colors,
        box_transforms,
        sphere_prototypes,
        sphere_colors,
        sphere_transforms,
        prototypes,
        scatter_filter,
        scatter_ground_transform,
        scatter_primvars,
        variation_controls,
        instancer,
        painted_points,
        attached_points,
        open_gl,
    )


def _make_demo_point(
    position,
    source_path,
    triangle_index,
    barycentric,
    seed,
    width=0.1,
    scale=1.0,
):
    return {
        "P": list(position),
        "width": width,
        "scale": scale,
        "seed": seed,
        "sourcePath": source_path,
        "triangleIndex": int(triangle_index),
        "barycentric": list(barycentric),
        "attachmentResolved": True,
    }


def _seed_demo_stroke(painted_points, layer_name, stroke_name, points):
    layer_id = painted_points.ensureLayer(layer_name)
    stroke_id = painted_points.ensureStroke(layer_id, stroke_name)
    painted_points.paintStrokeCommit(stroke_id, points, append=False)
    return layer_id, stroke_id


def _seed_benchmark_stroke(painted_points, layer_name, stroke_name, point_count):
    if hasattr(painted_points, "seedBenchmarkStroke"):
        result = painted_points.seedBenchmarkStroke(
            int(point_count), layer_name, stroke_name
        )
        return result["layerId"], result["strokeId"]

    return _seed_demo_stroke(
        painted_points,
        layer_name,
        stroke_name,
        _generate_benchmark_points(point_count),
    )


_BENCHMARK_POINT_COUNTS = {
    "small": 50,
    "medium": 200,
    "large": 1000,
    "xl": 25000,
    "xxl": 100000,
    "stress": 1000000,
}


def _benchmark_point(position_u, position_v, seed):
    x = position_u - 0.5
    z = position_v - 0.5

    if position_u + position_v <= 1.0:
        triangle_index = 0
        barycentric = [
            1.0 - position_u - position_v,
            position_u,
            position_v,
        ]
    else:
        triangle_index = 1
        barycentric = [
            1.0 - position_v,
            position_u + position_v - 1.0,
            1.0 - position_u,
        ]

    return _make_demo_point(
        [x, 0.01, z],
        "/benchmarkScatter",
        triangle_index,
        barycentric,
        seed,
        width=0.085,
        scale=1.0,
    )


def _generate_benchmark_points(count):
    columns = int(max(1, round(count**0.5)))
    rows = int((count + columns - 1) / columns)
    usable_columns = max(1, columns - 1)
    usable_rows = max(1, rows - 1)

    points = []
    for index in range(count):
        column = index % columns
        row = index // columns
        u = column / float(usable_columns) if usable_columns else 0.5
        v = row / float(usable_rows) if usable_rows else 0.5
        margin = 0.04
        u = margin + (1.0 - margin * 2.0) * u
        v = margin + (1.0 - margin * 2.0) * v
        points.append(_benchmark_point(u, v, 5000 + index))
    return points


def _reset_benchmark_scene(script):
    for name in (
        "ScatterBenchmarkOpenGL",
        "ScatterBenchmarkAttachedPoints",
        "ScatterBenchmarkPaintedPoints",
        "ScatterBenchmarkPlane",
    ):
        if name in script:
            script.removeChild(script[name])


def _create_benchmark_demo_network(script):
    _reset_benchmark_scene(script)

    plane = GafferScene.Plane("ScatterBenchmarkPlane")
    plane["name"].setValue("benchmarkScatter")
    script.addChild(plane)

    painted_points = GafferScatterPaint.PaintedPoints("ScatterBenchmarkPaintedPoints")
    script.addChild(painted_points)
    painted_points["defaultColor"].setValue(_default_authored_color())
    painted_points["targetFilter"].setValue("* -*/benchmarkScatter/scatter*")
    painted_points["in"].setInput(plane["out"])

    attached_points = GafferScatterPaint.AttachedPoints(
        "ScatterBenchmarkAttachedPoints"
    )
    script.addChild(attached_points)
    attached_points["in"].setInput(plane["out"])
    attached_points["points"].setInput(painted_points["out"])
    attached_points["outputLocation"].setValue("/benchmarkScatter/scatter")

    open_gl = _create_demo_opengl_attributes(
        script, "ScatterBenchmarkOpenGL", attached_points["out"]
    )
    return plane, painted_points, attached_points, open_gl


def _build_benchmark_scene_for_script(script, graph_editor=None, point_count=None):
    with Gaffer.UndoScope(script):
        plane, painted_points, attached_points, open_gl = (
            _create_benchmark_demo_network(script)
        )
        _require_demo_api(
            painted_points,
            "Scatter benchmark scene",
            "ensureLayer",
            "ensureStroke",
            "paintStrokeCommit",
        )

        guidance = "Built benchmark scene. Use the Benchmark menu to seed a deterministic stroke."
        if point_count is not None:
            _seed_benchmark_stroke(
                painted_points,
                "Benchmark Layer",
                f"Benchmark Stroke {point_count}",
                point_count,
            )
            guidance = (
                f"Built benchmark scene with deterministic {point_count}-point stroke. "
                "Run paint/erase timing checks against this seeded setup."
            )

    return _finalize_demo_scene(
        script,
        graph_editor,
        [plane, painted_points, attached_points, open_gl],
        open_gl,
        open_gl["out"],
        ["/benchmarkScatter"],
        painted_points,
        guidance,
        mode=0,
        known_point_count=point_count,
    )


def build_benchmark_scene_for_script(script, graph_editor=None):
    return _build_benchmark_scene_for_script(script, graph_editor=graph_editor)


def seed_small_benchmark_stroke_for_script(script, graph_editor=None):
    return _build_benchmark_scene_for_script(
        script,
        graph_editor=graph_editor,
        point_count=_BENCHMARK_POINT_COUNTS["small"],
    )


def seed_medium_benchmark_stroke_for_script(script, graph_editor=None):
    return _build_benchmark_scene_for_script(
        script,
        graph_editor=graph_editor,
        point_count=_BENCHMARK_POINT_COUNTS["medium"],
    )


def seed_large_benchmark_stroke_for_script(script, graph_editor=None):
    return _build_benchmark_scene_for_script(
        script,
        graph_editor=graph_editor,
        point_count=_BENCHMARK_POINT_COUNTS["large"],
    )


def seed_xl_benchmark_stroke_for_script(script, graph_editor=None):
    return _build_benchmark_scene_for_script(
        script,
        graph_editor=graph_editor,
        point_count=_BENCHMARK_POINT_COUNTS["xl"],
    )


def seed_xxl_benchmark_stroke_for_script(script, graph_editor=None):
    return _build_benchmark_scene_for_script(
        script,
        graph_editor=graph_editor,
        point_count=_BENCHMARK_POINT_COUNTS["xxl"],
    )


def seed_stress_benchmark_stroke_for_script(script, graph_editor=None):
    return _build_benchmark_scene_for_script(
        script,
        graph_editor=graph_editor,
        point_count=_BENCHMARK_POINT_COUNTS["stress"],
    )


def _front_plane_sample(group_name):
    return {
        "point": [0.0, 0.0, 0.0],
        "width": 0.1,
        "scale": 1.0,
        "pressureDensity": 1.0,
        "pressureSoftness": 1.0,
        "seed": 11,
        "sourcePath": f"/{group_name}/frontPlane",
        "triangleIndex": 0,
        "barycentric": [0.5, 0.25, 0.25],
        "attachmentResolved": True,
        "N": [0.0, 1.0, 0.0],
        "valid": True,
    }


def _paint_demo_samples(painted_points, layer_name, stroke_name, samples):
    layer_id = painted_points.ensureLayer(layer_name)
    stroke_id = painted_points.ensureStroke(layer_id, stroke_name)
    authored_points = painted_points.brushPaintPoints(samples)
    painted_points.paintStrokeCommit(stroke_id, authored_points, append=False)
    return layer_id, stroke_id


def _configure_demo_tool(
    script,
    painted_points,
    mode=None,
    status=None,
    layer_edit_action=None,
    stroke_edit_action=None,
    plug_values=None,
    known_point_count=None,
):
    plug_values = dict(plug_values or {})
    configured = 0
    for scene_view in _retarget_scene_views.__globals__.get(
        "_scene_views_for_script", _scene_views_for_script
    )(script)[0]:
        tool = None
        try:
            for candidate in scene_view["tools"].children():
                if isinstance(candidate, GafferScatterPaintUI.PaintPointsTool):
                    tool = candidate
                    break
        except Exception:
            tool = None
        if tool is None:
            continue

        try:
            tool["targetNode"].setValue(painted_points.relativeName(script))
            if mode is not None:
                tool["mode"].setValue(mode)
            if layer_edit_action is not None:
                tool["layerEditAction"].setValue(layer_edit_action)
            if stroke_edit_action is not None:
                tool["strokeEditAction"].setValue(stroke_edit_action)
            for plug_name, value in plug_values.items():
                if plug_name in tool:
                    tool[plug_name].setValue(value)
            if known_point_count is not None and "primedPointCount" in tool:
                tool["primedPointCount"].setValue(int(known_point_count))
            if status:
                tool["status"].setValue(status)
            tool["active"].setValue(True)
            configured += 1
        except Exception as exc:
            _log_warning(f"Failed to configure PaintPointsTool for demo: {exc}")
    return configured


def _finalize_demo_scene(
    script,
    graph_editor,
    nodes,
    focus,
    output_plug,
    frame_paths,
    painted_points,
    guidance,
    mode=None,
    layer_edit_action=None,
    stroke_edit_action=None,
    tool_values=None,
    known_point_count=None,
):
    with Gaffer.UndoScope(script):
        script.selection().clear()
        script.selection().add(nodes)
        script.setFocus(focus)

    if graph_editor is not None:
        graph_editor.frame(nodes, extend=True)

    _retarget_scene_views(script, output_plug, frame_paths=frame_paths)
    _configure_demo_tool(
        script,
        painted_points,
        mode=mode,
        status=guidance,
        layer_edit_action=layer_edit_action,
        stroke_edit_action=stroke_edit_action,
        plug_values=tool_values,
        known_point_count=known_point_count,
    )
    _log_info(guidance)
    return output_plug.node()


def build_paint_erase_basics_demo_for_script(script, graph_editor=None):
    with Gaffer.UndoScope(script):
        plane, painted_points, attached_points, open_gl = _create_plane_demo_network(
            script, "PaintEraseBasics", "paintBasics"
        )
        _require_demo_api(
            painted_points,
            "Paint + Erase Basics demo",
            "ensureLayer",
            "ensureStroke",
            "paintStrokeCommit",
            "eraseCommit",
            "lastStrokeId",
            "mutatePoints",
        )

    return _finalize_demo_scene(
        script,
        graph_editor,
        [plane, painted_points, attached_points, open_gl],
        open_gl,
        open_gl["out"],
        ["/paintBasics"],
        painted_points,
        "Built Paint + Erase Basics demo. Select the PaintPointsTool, click the canvas, and try drawing a new stroke!",
        mode=0,
    )


def build_paint_erase_cyclo_demo_for_script(script, graph_editor=None):
    with Gaffer.UndoScope(script):
        reader, painted_points, attached_points, open_gl = (
            _create_scene_reader_demo_network(
                script,
                "PaintEraseCyclo",
                _demo_asset_path("demo", "cyclo.abc"),
                "/cyclo",
                output_location="/cyclo/scatter",
            )
        )
        _require_demo_api(
            painted_points,
            "Paint + Erase Cyclo demo",
            "ensureLayer",
            "ensureStroke",
            "paintStrokeCommit",
            "eraseCommit",
            "lastStrokeId",
            "mutatePoints",
        )

    return _finalize_demo_scene(
        script,
        graph_editor,
        [reader, painted_points, attached_points, open_gl],
        open_gl,
        open_gl["out"],
        ["/cyclo"],
        painted_points,
        "Built Paint + Erase Cyclo demo from a known-good Alembic. Select the PaintPointsTool and paint directly on the imported mesh.",
        mode=0,
    )


def build_paint_through_erase_space_demo_for_script(script, graph_editor=None):
    with Gaffer.UndoScope(script):
        front_plane, back_plane, group, painted_points, attached_points, open_gl = (
            _create_overlapping_demo_network(
                script, "PaintThroughEraseSpace", "paintThroughDemo"
            )
        )
        _require_demo_api(
            painted_points,
            "Paint Through + Erase Space demo",
            "ensureLayer",
            "ensureStroke",
            "paintStrokeCommit",
            "brushPaintPoints",
        )
        painted_points["targetFilter"].setValue(
            "/paintThroughDemo/frontPlane /paintThroughDemo/backPlane"
        )
        painted_points["paintThroughMode"].setValue(1)

        authored_points = painted_points.brushPaintPoints(
            [_front_plane_sample("paintThroughDemo")]
        )
        front_points = [
            point
            for point in authored_points
            if point["sourcePath"] == "/paintThroughDemo/frontPlane"
        ]
        back_points = [
            point
            for point in authored_points
            if point["sourcePath"] == "/paintThroughDemo/backPlane"
        ]
        _seed_demo_stroke(
            painted_points, "Paint Through Layer", "Front Surface", front_points
        )
        _seed_demo_stroke(
            painted_points, "Paint Through Layer", "Back Surface", back_points
        )

    return _finalize_demo_scene(
        script,
        graph_editor,
        [front_plane, back_plane, group, painted_points, attached_points, open_gl],
        open_gl,
        open_gl["out"],
        [
            "/paintThroughDemo",
            "/paintThroughDemo/frontPlane",
            "/paintThroughDemo/backPlane",
        ],
        painted_points,
        "Built Paint Through + Erase Space demo. Toggle paintThroughMode and eraseSpace while testing overlapping front/back targets.",
        mode=1,
        tool_values={"eraseSpace": 1},
    )


def build_relax_reproject_demo_for_script(script, graph_editor=None):
    with Gaffer.UndoScope(script):
        plane, painted_points, attached_points, open_gl = _create_plane_demo_network(
            script, "RelaxReproject", "repairDemo"
        )
        _require_demo_api(
            painted_points,
            "Relax + Reproject demo",
            "ensureLayer",
            "ensureStroke",
            "paintStrokeCommit",
            "pointRecords",
            "setCurrentSelection",
            "mutatePoints",
        )
        attached_points["strictUnresolved"].setValue(False)

        layer_id, relax_stroke = _seed_demo_stroke(
            painted_points,
            "Repair Layer",
            "Relax Stroke",
            [
                _make_demo_point(
                    [-0.25, 0.01, -0.1], "/repairDemo", 0, [0.5, 0.25, 0.25], 200
                ),
                _make_demo_point(
                    [0.0, 0.06, 0.0], "/repairDemo", 1, [0.25, 0.5, 0.25], 201
                ),
                _make_demo_point(
                    [0.25, 0.01, 0.1], "/repairDemo", 0, [0.55, 0.2, 0.25], 202
                ),
            ],
        )
        _layer_id, broken_stroke = _seed_demo_stroke(
            painted_points,
            "Repair Layer",
            "Broken Attachment",
            [
                _make_demo_point(
                    [0.35, 0.01, -0.2], "/repairDemo", 1, [0.25, 0.5, 0.25], 203
                ),
            ],
        )

        point_records = painted_points.pointRecords()
        relax_point_id = next(
            point["pointId"]
            for point in point_records
            if point["strokeId"] == relax_stroke and point["seed"] == 201
        )
        broken_point_id = next(
            point["pointId"]
            for point in point_records
            if point["strokeId"] == broken_stroke
        )

        painted_points.setCurrentSelection([relax_point_id], [relax_stroke])

        def break_attachment(point):
            if point.get("pointId") != broken_point_id:
                return False
            point["sourcePath"] = "/repairDemoMissing"
            point["attachmentResolved"] = True
            return True

        painted_points.mutatePoints(break_attachment)

    return _finalize_demo_scene(
        script,
        graph_editor,
        [plane, painted_points, attached_points, open_gl],
        open_gl,
        open_gl["out"],
        ["/repairDemo"],
        painted_points,
        "Built Relax + Reproject demo. Relax is primed on the selected middle point; then select the Broken Attachment stroke point and switch to Reproject.",
        mode=4,
    )


def build_layer_modes_timing_demo_for_script(script, graph_editor=None):
    with Gaffer.UndoScope(script):
        plane, painted_points, attached_points, open_gl = _create_plane_demo_network(
            script, "LayerModesTiming", "timingDemo"
        )
        _require_demo_api(
            painted_points,
            "Layer Modes + Timing demo",
            "ensureLayer",
            "ensureStroke",
            "paintStrokeCommit",
            "pointRecords",
            "setCurrentSelection",
            "setLayerTimeRange",
            "setLayerMute",
            "setLayerSolo",
            "mutateCacheStore",
        )

        base_layer, base_stroke = _seed_demo_stroke(
            painted_points,
            "Persistent Base",
            "Base Coverage",
            [
                _make_demo_point(
                    [-0.3, 0.01, 0.0], "/timingDemo", 0, [0.5, 0.25, 0.25], 300
                ),
            ],
        )
        additive_layer, additive_stroke = _seed_demo_stroke(
            painted_points,
            "Additive Burst",
            "Burst Coverage",
            [
                _make_demo_point(
                    [0.0, 0.01, 0.15], "/timingDemo", 1, [0.25, 0.5, 0.25], 301
                ),
            ],
        )
        override_layer, override_stroke = _seed_demo_stroke(
            painted_points,
            "Override Punch",
            "Punch Coverage",
            [
                _make_demo_point(
                    [0.28, 0.01, -0.1], "/timingDemo", 0, [0.55, 0.2, 0.25], 302
                ),
            ],
        )

        painted_points.setLayerTimeRange(additive_layer, 10, 20)
        painted_points.setLayerTimeRange(override_layer, 15, 15)
        painted_points.setLayerMute(additive_layer, False)
        painted_points.setLayerSolo(override_layer, False)

        def set_modes(store):
            for layer in store.get("layers", []):
                if int(layer.get("layerId", 0)) == base_layer:
                    layer["mode"] = 0
                elif int(layer.get("layerId", 0)) == additive_layer:
                    layer["mode"] = 1
                elif int(layer.get("layerId", 0)) == override_layer:
                    layer["mode"] = 2
            for stroke in store.get("strokes", []):
                if int(stroke.get("strokeId", 0)) == base_stroke:
                    stroke["mode"] = 0
                elif int(stroke.get("strokeId", 0)) == additive_stroke:
                    stroke["mode"] = 1
                elif int(stroke.get("strokeId", 0)) == override_stroke:
                    stroke["mode"] = 2

        painted_points.mutateCacheStore(set_modes)
        selected_point = next(
            point
            for point in painted_points.pointRecords()
            if point["strokeId"] == additive_stroke
        )
        painted_points.setCurrentSelection(
            [selected_point["pointId"]], [selected_point["strokeId"]]
        )

    return _finalize_demo_scene(
        script,
        graph_editor,
        [plane, painted_points, attached_points, open_gl],
        open_gl,
        open_gl["out"],
        ["/timingDemo"],
        painted_points,
        "Built Layer Modes + Timing demo. Try LayerEdit SetMode and SetTimeRange using the pre-seeded Persistent, Additive, and Override layers.",
        mode=6,
        layer_edit_action=6,
        tool_values={"frameMode": 1, "frameStart": 10, "frameEnd": 20},
    )


def build_layer_stroke_edit_demo_for_script(script, graph_editor=None):
    with Gaffer.UndoScope(script):
        plane, painted_points, attached_points, open_gl = _create_plane_demo_network(
            script, "LayerStrokeEdit", "editDemo"
        )
        _require_demo_api(
            painted_points,
            "Layer + Stroke Editing demo",
            "ensureLayer",
            "ensureStroke",
            "paintStrokeCommit",
            "pointRecords",
            "setCurrentSelection",
        )

        layer_a, stroke_a = _seed_demo_stroke(
            painted_points,
            "Edit Layer A",
            "Stroke A",
            [
                _make_demo_point(
                    [-0.35, 0.01, 0.0], "/editDemo", 0, [0.5, 0.25, 0.25], 400
                ),
            ],
        )
        _layer_a, stroke_b = _seed_demo_stroke(
            painted_points,
            "Edit Layer A",
            "Stroke B",
            [
                _make_demo_point(
                    [0.0, 0.01, 0.0], "/editDemo", 1, [0.25, 0.5, 0.25], 401
                ),
            ],
        )
        layer_b, stroke_c = _seed_demo_stroke(
            painted_points,
            "Edit Layer B",
            "Stroke C",
            [
                _make_demo_point(
                    [0.35, 0.01, 0.0], "/editDemo", 0, [0.55, 0.2, 0.25], 402
                ),
            ],
        )

        selected_point = next(
            point
            for point in painted_points.pointRecords()
            if point["strokeId"] == stroke_b
        )
        painted_points.setCurrentSelection([selected_point["pointId"]], [stroke_b])

    return _finalize_demo_scene(
        script,
        graph_editor,
        [plane, painted_points, attached_points, open_gl],
        open_gl,
        open_gl["out"],
        ["/editDemo"],
        painted_points,
        "Built Layer + Stroke Editing demo. The current selection targets Stroke B so you can try StrokeEdit move/merge/delete or LayerEdit move/set-mode next.",
        mode=7,
        stroke_edit_action=2,
        tool_values={"strokeMoveToIndex": 0, "layerMoveToIndex": 0},
    )


def build_box_instance_scatter_demo_for_script(script, graph_editor=None):
    with Gaffer.UndoScope(script):
        (
            plane,
            box_prototypes,
            box_colors,
            box_transforms,
            sphere_prototypes,
            sphere_colors,
            sphere_transforms,
            prototypes,
            scatter_filter,
            scatter_ground_transform,
            scatter_primvars,
            variation_controls,
            instancer,
            painted_points,
            attached_points,
            open_gl,
        ) = _create_instanced_demo_network(
            script,
            "BoxInstanceScatter",
            "boxScatterDemo",
            prototype_name="scatterBox",
            output_location="/boxScatterDemoScatter",
        )
        _require_demo_api(
            painted_points,
            "Box Instance Scatter demo",
            "ensureLayer",
            "ensureStroke",
            "paintStrokeCommit",
            "brushPaintPoints",
        )
        painted_points["targetFilter"].setValue("/boxScatterDemo")
        painted_points["paintThroughMode"].setValue(1)

        _paint_demo_samples(
            painted_points,
            "Box Scatter Layer",
            "Box Coverage",
            [
                {
                    "point": [-0.85, 0.0, -0.75],
                    "width": 0.12,
                    "scale": 1.0,
                    "pressureDensity": 1.0,
                    "pressureSoftness": 1.0,
                    "seed": 500,
                    "sourcePath": "/boxScatterDemo",
                    "valid": True,
                },
                {
                    "point": [-0.35, 0.0, -0.1],
                    "width": 0.12,
                    "scale": 1.0,
                    "pressureDensity": 1.0,
                    "pressureSoftness": 1.0,
                    "seed": 501,
                    "sourcePath": "/boxScatterDemo",
                    "valid": True,
                },
                {
                    "point": [0.12, 0.0, 0.35],
                    "width": 0.12,
                    "scale": 1.0,
                    "pressureDensity": 1.0,
                    "pressureSoftness": 1.0,
                    "seed": 502,
                    "sourcePath": "/boxScatterDemo",
                    "valid": True,
                },
                {
                    "point": [0.68, 0.0, -0.55],
                    "width": 0.12,
                    "scale": 1.0,
                    "pressureDensity": 1.0,
                    "pressureSoftness": 1.0,
                    "seed": 503,
                    "sourcePath": "/boxScatterDemo",
                    "valid": True,
                },
                {
                    "point": [0.9, 0.0, 0.8],
                    "width": 0.12,
                    "scale": 1.0,
                    "pressureDensity": 1.0,
                    "pressureSoftness": 1.0,
                    "seed": 504,
                    "sourcePath": "/boxScatterDemo",
                    "valid": True,
                },
                {
                    "point": [-0.55, 0.0, 0.72],
                    "width": 0.12,
                    "scale": 1.0,
                    "pressureDensity": 1.0,
                    "pressureSoftness": 1.0,
                    "seed": 505,
                    "sourcePath": "/boxScatterDemo",
                    "valid": True,
                },
            ],
        )

        variation_sync = _install_instancer_variation_sync(
            painted_points,
            attached_points,
            scatter_primvars,
            box_transforms,
            sphere_transforms,
            variation_controls,
        )
        variation_sync()

    return _finalize_demo_scene(
        script,
        graph_editor,
        [
            plane,
            *box_prototypes,
            *box_colors,
            *box_transforms,
            *sphere_prototypes,
            *sphere_colors,
            *sphere_transforms,
            prototypes,
            scatter_filter,
            scatter_ground_transform,
            scatter_primvars,
            variation_controls,
            instancer,
            painted_points,
            attached_points,
            open_gl,
        ],
        open_gl,
        open_gl["out"],
        [
            "/boxScatterDemo",
            "/boxScatterDemoScatter",
            "/boxScatterDemoScatter/instances",
            "/boxScatterDemoScatter/instances/scatterBox0",
            "/boxScatterDemoScatter/instances/scatterSphere0",
        ],
        painted_points,
        f"Built Box Instance Scatter demo. Paint-through is preconfigured so you can paint through the visible box and sphere instances onto the horizontal ground plane /boxScatterDemo. The scattered instances now live at /boxScatterDemoScatter so they stay aligned to the ground plane. Tweak {variation_controls.getName()}.positionVariation, rotationVariation, scaleMin, scaleMax, and sphereProbability for random variation.",
        mode=0,
        tool_values={"paintThroughMode": 1, "targetFilter": "/boxScatterDemo"},
    )
