# ruff: noqa
from . import _core_shared as _shared

globals().update(
    {name: value for name, value in vars(_shared).items() if not name.startswith("__")}
)


class StaticPoints(GafferScene.SceneProcessor):
    def __init__(self, name="StaticPoints"):
        GafferScene.SceneProcessor.__init__(self, name)

        self["pointData"] = Gaffer.ObjectPlug(defaultValue=IECore.CompoundObject())
        self["outputLocation"] = Gaffer.StringPlug(defaultValue="/scatter")
        self["pointType"] = Gaffer.StringPlug(defaultValue="gl:point")
        self["debugColor"] = Gaffer.BoolPlug(defaultValue=False)
        self["__objectToScene"] = GafferScene.ObjectToScene()
        self["__parent"] = GafferScene.Parent()
        self["__parent"]["in"].setInput(self["in"])
        self["__parent"]["children"][0].setInput(self["__objectToScene"]["out"])
        self["out"].setInput(self["__parent"]["out"])

        self.__plugSetConnection = self.plugSetSignal().connect(
            Gaffer.WeakMethod(self.__plugSet),
            scoped=True,
        )
        self.__parentChangedConnection = self.parentChangedSignal().connect(
            Gaffer.WeakMethod(self.__parentChanged),
            scoped=True,
        )
        self.__contextChangedConnection = None
        self.__connectContextChanged()
        self.__syncOutput()

    def __syncOutput(self):
        data = self["pointData"].getValue()
        output_location = self["outputLocation"].getValue().strip() or "/scatter"
        parent_location, _, leaf_name = output_location.rpartition("/")
        parent_location = parent_location or "/"
        leaf_name = leaf_name or "scatter"
        self["__parent"]["parent"].setValue(parent_location)
        self["__objectToScene"]["name"].setValue(leaf_name)

        primitive = _sampled_point_primitive(data, _current_frame(self))
        if primitive is None:
            primitive = IECoreScene.PointsPrimitive(IECore.V3fVectorData())
        else:
            primitive = _primitive_with_display_color(
                primitive,
                self["debugColor"].getValue(),
            )
        primitive["type"] = IECoreScene.PrimitiveVariable(
            IECoreScene.PrimitiveVariable.Interpolation.Constant,
            IECore.StringData(self["pointType"].getValue()),
        )
        self["__objectToScene"]["object"].setValue(primitive)

    def __plugSet(self, plug):
        if (
            plug.isSame(self["pointData"])
            or plug.isSame(self["outputLocation"])
            or plug.isSame(self["pointType"])
            or plug.isSame(self["debugColor"])
        ):
            self.__syncOutput()

    def setPointRecords(self, point_records):
        records = []
        for point in list(point_records or []):
            records.append(dict(point))
        self["pointData"].setValue(
            IECore.CompoundObject(
                {
                    "pointCount": IECore.IntData(len(records)),
                    "pointsPrimitive": _points_primitive_from_records(
                        records,
                        self["pointType"].getValue(),
                        debug_color=self["debugColor"].getValue(),
                    ),
                }
            )
        )
        self.__syncOutput()

    def setFramePointRecords(self, frame_records):
        frame_samples = {}
        frame_numbers = []
        first_primitive = None
        first_count = 0
        for record in list(frame_records or []):
            frame = int(record.get("frame", 0))
            records = [dict(point) for point in list(record.get("records", []))]
            primitive = _points_primitive_from_records(
                records,
                self["pointType"].getValue(),
                debug_color=self["debugColor"].getValue(),
            )
            if first_primitive is None:
                first_primitive = primitive.copy()
                first_count = len(records)
            frame_numbers.append(frame)
            frame_samples[str(frame)] = primitive
        self["pointData"].setValue(
            IECore.CompoundObject(
                {
                    "pointCount": IECore.IntData(first_count),
                    "pointsPrimitive": (
                        first_primitive
                        if first_primitive is not None
                        else IECoreScene.PointsPrimitive(IECore.V3fVectorData())
                    ),
                    "frameNumbers": IECore.IntVectorData(frame_numbers),
                    "frameSamples": IECore.CompoundObject(frame_samples),
                }
            )
        )
        self.__syncOutput()

    def setFramePointData(self, frame_samples):
        samples = list(frame_samples or [])
        frame_numbers = []
        sample_members = {}
        first_primitive = None
        first_count = 0
        for sample in samples:
            frame = int(sample.get("frame", 0))
            primitive = sample.get("pointsPrimitive")
            if not isinstance(primitive, IECoreScene.PointsPrimitive):
                primitive = IECoreScene.PointsPrimitive(IECore.V3fVectorData())
            if first_primitive is None:
                first_primitive = primitive.copy()
                point_count = sample.get(
                    "pointCount", IECore.IntData(int(primitive.numPoints))
                )
                first_count = int(
                    point_count.value
                    if isinstance(point_count, IECore.IntData)
                    else point_count
                )
            frame_numbers.append(frame)
            sample_members[str(frame)] = primitive.copy()
        self["pointData"].setValue(
            IECore.CompoundObject(
                {
                    "pointCount": IECore.IntData(first_count),
                    "pointsPrimitive": (
                        first_primitive
                        if first_primitive is not None
                        else IECoreScene.PointsPrimitive(IECore.V3fVectorData())
                    ),
                    "frameNumbers": IECore.IntVectorData(frame_numbers),
                    "frameSamples": IECore.CompoundObject(sample_members),
                }
            )
        )
        self.__syncOutput()

    def __contextChanged(self, context, variable_name):
        if variable_name == "frame":
            self.__syncOutput()

    def __parentChanged(self, component, old_parent):
        self.__connectContextChanged()

    def __connectContextChanged(self):
        if self.__contextChangedConnection is not None:
            return
        script_node = _script_node(self)
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
