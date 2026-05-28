# ruff: noqa
from . import _core_shared as _shared

import copy

globals().update(
    {name: value for name, value in vars(_shared).items() if not name.startswith("__")}
)


class AttachedPoints(GafferScene.SceneProcessor):
    def __init__(self, name="AttachedPoints"):
        GafferScene.SceneProcessor.__init__(self, name)

        self.__syncing = False
        self.__lastValidPointRecords = []
        self.__lastValidFrame = 0

        self["points"] = GafferScene.ScenePlug("points")
        self["outputLocation"] = Gaffer.StringPlug(defaultValue="/scatter")
        self["pointType"] = Gaffer.StringPlug(defaultValue="gl:point")
        self["includeAttributes"] = Gaffer.StringPlug(defaultValue="")
        self["exportPreset"] = Gaffer.IntPlug(defaultValue=1)
        self["surfaceSolveMode"] = Gaffer.IntPlug(defaultValue=0)
        self["allowCrossMeshReproject"] = Gaffer.BoolPlug(defaultValue=False)
        self["keepLastValidOutput"] = Gaffer.BoolPlug(defaultValue=True)
        self["strictUnresolved"] = Gaffer.BoolPlug(defaultValue=True)
        self["debugColor"] = Gaffer.BoolPlug(defaultValue=False)
        self["cacheVersion"] = _out_int(SCHEMA_VERSION)
        self["resolvedPointCount"] = _out_int(0)
        self["unresolvedPointCount"] = _out_int(0)
        self["lastValidFrame"] = _out_int(0)
        self["invalidPointCount"] = _out_int(0)
        self["invalidStrokeCount"] = _out_int(0)
        self["failingFrame"] = _out_int(0)
        self["failingTargetPaths"] = _out_string_vector()
        self["attachmentFailureReasons"] = _out_string_vector()
        self["topologyMismatchCount"] = _out_int(0)
        self["validationCategories"] = _out_string_vector(
            IECore.StringVectorData(
                [
                    "cache",
                    "lock",
                    "topology",
                    "attachment",
                    "exportReadiness",
                    "upgradeState",
                    "diagnostics",
                ]
            )
        )
        self["solveStatus"] = _out_string(SCHEMA_SUMMARY)

        self["__emptyScene"] = GafferScene.ScenePlug()
        self["__generatedObject"] = Gaffer.ObjectPlug(defaultValue=IECore.NullObject())
        self["__outputGate"] = _AttachedPointsOutputGate()
        self["__objectToScene"] = GafferScene.ObjectToScene()
        self["__objectToScene"]["name"].setValue("scatter")
        self["__parent"] = GafferScene.Parent()
        self["__parent"]["in"].setInput(self["in"])
        self["__outputGate"]["inObject"].setInput(self["__generatedObject"])
        self["__objectToScene"]["object"].setInput(self["__outputGate"]["out"])
        self["__parent"]["children"][0].setInput(self["__objectToScene"]["out"])
        self["out"].setInput(self["__parent"]["out"])

        self.__plugSetConnection = self.plugSetSignal().connect(
            Gaffer.WeakMethod(self.__plugSet),
            scoped=True,
        )
        self.__plugDirtiedConnection = self.plugDirtiedSignal().connect(
            Gaffer.WeakMethod(self.__plugDirtied),
            scoped=True,
        )
        self.__parentChangedConnection = self.parentChangedSignal().connect(
            Gaffer.WeakMethod(self.__parentChanged),
            scoped=True,
        )
        self.__contextChangedConnection = None
        self.__connectContextChanged()
        self.__syncFromAuthoredData()

    def __authoredStore(self):
        input_plug = self["points"].getInput()
        if input_plug is None:
            return None, ""

        node = input_plug.node()
        if node is None or not hasattr(node, "cacheSnapshot"):
            return None, ""

        try:
            return node.cacheSnapshot(), ""
        except Exception as exc:
            return None, str(exc)

    def __resolveAttachedPoint(self, point):
        source_path = str(point.get("sourcePath", "")).strip()
        if not source_path:
            return None, "missingSourcePath"

        if not self["in"].exists(source_path):
            return None, "missingTargetPath"

        try:
            scene_object = self["in"].object(source_path, _copy=False)
            full_transform = self["in"].fullTransform(source_path)
        except Exception:
            return None, "missingTargetPath"

        triangle_data = _mesh_triangle_data(scene_object)
        if not triangle_data:
            return None, "unsupportedTargetObject"

        triangle_index = int(point.get("triangleIndex", -1))
        barycentric = list(point.get("barycentric", [1.0, 0.0, 0.0]))
        if len(barycentric) != 3:
            barycentric = [1.0, 0.0, 0.0]

        triangle = None
        for candidate in triangle_data["triangles"]:
            if candidate[0] == triangle_index:
                triangle = candidate
                break

        if triangle is None:
            return None, "missingTriangle"

        try:
            positions = triangle_data["positions"]
            a = positions[triangle[1]]
            b = positions[triangle[2]]
            c = positions[triangle[3]]
        except Exception:
            return None, "invalidTriangleVertices"

        object_point = _triangle_point(a, b, c, barycentric)
        face_normal = _normalized(_cross(b - a, c - a), imath.V3f(0, 1, 0))
        object_normal = _triangle_normal(
            scene_object, triangle, barycentric, face_normal
        )
        object_up = _triangle_up(a, b, c, object_normal)
        world_point = object_point * full_transform
        world_normal = _normalized(
            full_transform.multDirMatrix(object_normal), imath.V3f(0, 1, 0)
        )
        world_up = _orthogonalized(
            full_transform.multDirMatrix(object_up),
            world_normal,
            imath.V3f(0, 0, 1),
        )

        resolved_point = dict(point)
        resolved_point["P"] = [
            float(world_point.x),
            float(world_point.y),
            float(world_point.z),
        ]
        resolved_point["N"] = [
            float(world_normal.x),
            float(world_normal.y),
            float(world_normal.z),
        ]
        resolved_point["up"] = [float(world_up.x), float(world_up.y), float(world_up.z)]
        resolved_point["orient"] = _orient_from_normal_up(world_normal, world_up)
        resolved_point["attachmentResolved"] = True
        return resolved_point, None

    def __syncFromAuthoredData(self):
        store, load_error = self.__authoredStore()
        point_records = []
        if store is None:
            layer_count = 0
            stroke_count = 0
            total_point_count = 0
            failing_target_paths = IECore.StringVectorData()
            attachment_failure_reasons = IECore.StringVectorData()
            solve_status = "No authored scatter data connected."
        else:
            layer_count = len(store.get("layers", []))
            stroke_count = 0
            total_point_count = 0
            invalid_point_count = 0
            resolved_point_count = 0
            unresolved_point_count = 0
            failing_target_paths = set()
            attachment_failure_counts = {}
            layers_by_id = {
                int(layer.get("layerId", 0)): layer for layer in store.get("layers", [])
            }
            strokes_by_id = {
                int(stroke.get("strokeId", 0)): stroke
                for stroke in store.get("strokes", [])
            }
            point_records_by_stroke = {}
            for point in store.get("points", []):
                point_records_by_stroke.setdefault(
                    int(point.get("strokeId", 0)), []
                ).append(point)

            for layer in store.get("layers", []):
                layer_strokes = [
                    stroke
                    for stroke in store.get("strokes", [])
                    if int(stroke.get("layerId", 0)) == int(layer.get("layerId", 0))
                ]
                stroke_count += len(layer_strokes)
                for stroke in layer_strokes:
                    points = point_records_by_stroke.get(
                        int(stroke.get("strokeId", 0)), []
                    )
                    total_point_count += len(points)
                    for raw_point in points:
                        point = dict(raw_point)
                        point["authoredColor"] = _color_list(
                            _resolve_authored_color(
                                store.get("node", {}),
                                layers_by_id.get(int(point.get("layerId", 0))),
                                strokes_by_id.get(int(point.get("strokeId", 0))),
                                point,
                            )
                        )
                        point["scatterColor"] = list(point["authoredColor"])
                        target_path_id = int(point.get("targetPathId", 0))
                        if target_path_id:
                            scene_paths = list(store.get("scenePaths", []))
                            if 0 < target_path_id <= len(scene_paths):
                                point["sourcePath"] = scene_paths[target_path_id - 1]
                            else:
                                point["sourcePath"] = ""
                        point["P"] = list(point.get("restWorldP", [0.0, 0.0, 0.0]))
                        point["N"] = list(point.get("restNormal", [0.0, 1.0, 0.0]))
                        point["up"] = list(point.get("restUp", [0.0, 0.0, 1.0]))
                        point["scale"] = float(point.get("uniformScale", 1.0))
                        point["attachmentResolved"] = (
                            int(point.get("anchorModeUsed", 3)) != 3
                        )
                        if not point.get("valid", True):
                            invalid_point_count += 1
                            point_records.append(dict(point))
                            continue

                        if point.get("attachmentResolved", False):
                            resolved_point, failure = self.__resolveAttachedPoint(point)
                            if resolved_point is not None:
                                point_records.append(resolved_point)
                                resolved_point_count += 1
                                continue
                            failure_reason = failure or "unknownAttachmentFailure"
                            attachment_failure_counts[failure_reason] = (
                                attachment_failure_counts.get(failure_reason, 0) + 1
                            )
                            if failure_reason in (
                                "missingTargetPath",
                                "unsupportedTargetObject",
                                "missingTriangle",
                                "invalidTriangleVertices",
                            ):
                                failing_target_paths.add(
                                    str(point.get("sourcePath", ""))
                                )

                        fallback_point = dict(point)
                        fallback_point["attachmentResolved"] = False
                        point_records.append(fallback_point)
                        unresolved_point_count += 1

            failing_target_paths = IECore.StringVectorData(
                sorted(str(path) for path in failing_target_paths if path)
            )
            attachment_failure_reasons = IECore.StringVectorData(
                [
                    f"{reason}:{attachment_failure_counts[reason]}"
                    for reason in sorted(attachment_failure_counts)
                ]
            )
            validation_categories = []
            diagnostics = dict(store.get("diagnostics", {}))
            for category_id in diagnostics.get("categories", []):
                validation_categories.append(
                    VALIDATION_CATEGORY_NAMES.get(
                        int(category_id), f"unknown:{category_id}"
                    )
                )
            if attachment_failure_counts:
                validation_categories.append("attachment")
            solve_status = (
                f"{layer_count} layers, {stroke_count} strokes, "
                f"{resolved_point_count} resolved points, "
                f"{unresolved_point_count} fallback points, "
                f"{total_point_count} authored points ready for attachment solve"
            )
            if attachment_failure_counts:
                solve_status = f"{solve_status}. Failures: " + ", ".join(
                    attachment_failure_reasons
                )
            if load_error:
                solve_status = f"Invalid authored data. {solve_status}"
        if store is None:
            invalid_point_count = 0
            resolved_point_count = 0
            unresolved_point_count = 0
            attachment_failure_reasons = IECore.StringVectorData()
            validation_categories = []

        if (
            self["strictUnresolved"].getValue()
            and store is None
            and self["points"].getInput() is not None
        ):
            unresolved_point_count = 1

        last_valid_used = False
        healthy_output = bool(point_records) and unresolved_point_count == 0
        if self["keepLastValidOutput"].getValue() and healthy_output:
            self.__lastValidPointRecords = copy.deepcopy(point_records)
            self.__lastValidFrame = _current_frame(self)
        elif (
            self["keepLastValidOutput"].getValue()
            and self.__lastValidPointRecords
            and (not point_records or self["strictUnresolved"].getValue())
        ):
            point_records = copy.deepcopy(self.__lastValidPointRecords)
            last_valid_used = True
            solve_status = f"{solve_status}. Using last valid output from frame {self.__lastValidFrame}."

        self.__syncing = True
        try:
            output_location = self["outputLocation"].getValue().strip() or "/scatter"
            parent_location, _, leaf_name = output_location.rpartition("/")
            parent_location = parent_location or "/"
            leaf_name = leaf_name or "scatter"
            self["__parent"]["parent"].setValue(parent_location)
            self["__objectToScene"]["name"].setValue(leaf_name)
            self["__generatedObject"].setValue(
                _points_primitive_from_records(
                    point_records,
                    self["pointType"].getValue(),
                    self["exportPreset"].getValue(),
                    self["includeAttributes"].getValue(),
                    self["debugColor"].getValue(),
                )
            )
            self["__outputGate"]["solveStatus"].setValue(load_error or solve_status)
            self["__outputGate"]["strictUnresolved"].setValue(
                self["strictUnresolved"].getValue()
            )
            self["__outputGate"]["unresolvedPointCount"].setValue(
                unresolved_point_count
            )

            self["cacheVersion"].setValue(SCHEMA_VERSION)
            self["resolvedPointCount"].setValue(
                resolved_point_count if store is not None else 0
            )
            self["unresolvedPointCount"].setValue(unresolved_point_count)
            self["lastValidFrame"].setValue(
                self.__lastValidFrame if self.__lastValidFrame else 0
            )
            self["invalidPointCount"].setValue(invalid_point_count)
            self["invalidStrokeCount"].setValue(
                0 if store is not None else unresolved_point_count
            )
            self["failingFrame"].setValue(0)
            self["failingTargetPaths"].setValue(failing_target_paths)
            self["attachmentFailureReasons"].setValue(attachment_failure_reasons)
            self["validationCategories"].setValue(
                _validation_category_strings(sorted(set(validation_categories)))
            )
            self["topologyMismatchCount"].setValue(
                sum(
                    int(entry.split(":", 1)[1])
                    for entry in attachment_failure_reasons
                    if entry.startswith("missingTriangle:")
                    or entry.startswith("invalidTriangleVertices:")
                )
            )
            self["solveStatus"].setValue(load_error or solve_status)
        finally:
            self.__syncing = False

    def __plugSet(self, plug):
        if self.__syncing:
            return

        if (
            plug.isSame(self["strictUnresolved"])
            or plug.isSame(self["outputLocation"])
            or plug.isSame(self["pointType"])
            or plug.isSame(self["includeAttributes"])
            or plug.isSame(self["exportPreset"])
            or plug.isSame(self["debugColor"])
        ):
            self.__syncFromAuthoredData()

    def __plugDirtied(self, plug):
        if self.__syncing:
            return

        if (
            plug.isSame(self["points"])
            or self["points"].isAncestorOf(plug)
            or plug.isAncestorOf(self["points"])
        ):
            self.__syncFromAuthoredData()
            return

        points_input = self["points"].getInput()
        if points_input is not None and (
            points_input.isSame(plug)
            or points_input.isAncestorOf(plug)
            or plug.isAncestorOf(points_input)
        ):
            self.__syncFromAuthoredData()
            return

        if (
            plug.isSame(self["in"])
            or self["in"].isAncestorOf(plug)
            or plug.isAncestorOf(self["in"])
        ):
            self.__syncFromAuthoredData()
            return

        in_input = self["in"].getInput()
        if in_input is not None and (
            in_input.isSame(plug)
            or in_input.isAncestorOf(plug)
            or plug.isAncestorOf(in_input)
        ):
            self.__syncFromAuthoredData()

    def __contextChanged(self, context, variable_name):
        if self.__syncing or variable_name != "frame":
            return
        self.__syncFromAuthoredData()

    def __parentChanged(self, component, old_parent):
        self.__connectContextChanged()

    def __connectContextChanged(self):
        if self.__contextChangedConnection is not None:
            return
        script_node = self.scriptNode()
        if script_node is None:
            return
        self.__contextChangedConnection = (
            script_node.context()
            .changedSignal()
            .connect(
                Gaffer.WeakMethod(self.__contextChanged),
                scoped=True,
            )
        )
