import os
import pathlib


Import("plugin_env")


plugin_root = pathlib.Path(plugin_env["GAFFER_EXTENSION_ROOT"])
gaffer_header_root = pathlib.Path(plugin_env["GAFFER_HEADER_ROOT"])
gaffer_runtime_root = pathlib.Path(plugin_env["GAFFER_RUNTIME_ROOT"])
gaffer_lib_dir = pathlib.Path(plugin_env["GAFFER_LIB_DIR"])
extension_name = plugin_env["GAFFER_EXTENSION_NAME"]
python_include_dir = pathlib.Path(plugin_env["GAFFER_PYTHON_INCLUDE_DIR"])
python_lib = plugin_env["GAFFER_PYTHON_LIBRARY"]
boost_python_lib = plugin_env["GAFFER_BOOST_PYTHON_LIBRARY"]


env = Environment(ENV=os.environ)
env.Decider("MD5-timestamp")
env.Replace(SHLIBPREFIX="")

include_paths = [
    str(plugin_root / "include"),
    str(gaffer_header_root / "include"),
    str(gaffer_runtime_root / "include"),
    str(gaffer_runtime_root / "include" / "Imath"),
    str(python_include_dir),
]

lib_paths = [
    str(gaffer_lib_dir),
]

common_libs = [
    "Gaffer",
    "GafferScene",
    "IECore",
    "IECoreScene",
]

env.Append(CPPPATH=include_paths)
env.Append(LIBPATH=lib_paths)
env.Append(CXXFLAGS=["-std=c++17"])
env.Append(LINKFLAGS=[f"-Wl,-rpath,{gaffer_lib_dir}"])

sse_env = env.Clone()
sse_env.Append(CXXFLAGS=["-msse4.2"])

avx2_env = env.Clone()
avx2_env.Append(CXXFLAGS=["-mavx2", "-mfma"])

core_sources = [
    str(plugin_root / "src" / "GafferScatterPaint" / "CacheFormat.cpp"),
    str(plugin_root / "src" / "GafferScatterPaint" / "CacheFormatDict.cpp"),
    str(plugin_root / "src" / "GafferScatterPaint" / "CacheValidation.cpp"),
    str(plugin_root / "src" / "GafferScatterPaint" / "PaintedPoints.cpp"),
    str(plugin_root / "src" / "GafferScatterPaint" / "PaintedPointsPlugs.cpp"),
    str(plugin_root / "src" / "GafferScatterPaint" / "PaintedPointsInteractiveStore.cpp"),
    str(plugin_root / "src" / "GafferScatterPaint" / "PaintedPointsStore.cpp"),
    str(plugin_root / "src" / "GafferScatterPaint" / "PaintedPointsEdit.cpp"),
    str(plugin_root / "src" / "GafferScatterPaint" / "PaintedPointsSolve.cpp"),
    str(plugin_root / "src" / "GafferScatterPaint" / "PaintedPointsExport.cpp"),
    str(plugin_root / "src" / "GafferScatterPaint" / "PaintedPointsBenchmark.cpp"),
    str(plugin_root / "src" / "GafferScatterPaint" / "PaintedPointsBrushOps.cpp"),
    str(plugin_root / "src" / "GafferScatterPaint" / "BrushCpuFeaturesLinux.cpp"),
    str(plugin_root / "src" / "GafferScatterPaint" / "BrushSimdDispatch.cpp"),
    str(plugin_root / "src" / "GafferScatterPaint" / "BrushResolveScalar.cpp"),
    str(plugin_root / "src" / "GafferScatterPaint" / "AttachedPoints.cpp"),
    str(plugin_root / "src" / "GafferScatterPaint" / "AttachedPointsPlugs.cpp"),
    str(plugin_root / "src" / "GafferScatterPaint" / "AttachedPointsSolve.cpp"),
    str(plugin_root / "src" / "GafferScatterPaint" / "AttachedPointsSolveStore.cpp"),
    str(plugin_root / "src" / "GafferScatterPaint" / "AttachedPointsSolveGeometry.cpp"),
    str(plugin_root / "src" / "GafferScatterPaint" / "AttachedPointsScene.cpp"),
    str(plugin_root / "src" / "GafferScatterPaint" / "StaticPoints.cpp"),
]

sse_sources = [
    str(plugin_root / "src" / "GafferScatterPaint" / "BrushResolveSSE42.cpp"),
]

avx2_sources = [
    str(plugin_root / "src" / "GafferScatterPaint" / "BrushResolveAVX2.cpp"),
]

ui_sources = [
    str(plugin_root / "src" / "GafferScatterPaintUI" / "PaintPointsTool.cpp"),
    str(plugin_root / "src" / "GafferScatterPaintUI" / "PaintPointsToolPlugs.cpp"),
    str(plugin_root / "src" / "GafferScatterPaintUI" / "PaintPointsToolNodeSync.cpp"),
    str(
        plugin_root
        / "src"
        / "GafferScatterPaintUI"
        / "PaintPointsToolViewportSignals.cpp"
    ),
    str(plugin_root / "src" / "GafferScatterPaintUI" / "PaintPointsToolViewport.cpp"),
    str(plugin_root / "src" / "GafferScatterPaintUI" / "PaintPointsToolHit.cpp"),
    str(plugin_root / "src" / "GafferScatterPaintUI" / "PaintPointsToolStroke.cpp"),
    str(plugin_root / "src" / "GafferScatterPaintUI" / "PaintPointsToolSelection.cpp"),
]

module_sources = [
    str(plugin_root / "src" / "GafferScatterPaint" / "Module.cpp"),
]

ui_module_sources = [
    str(plugin_root / "src" / "GafferScatterPaintUI" / "Module.cpp"),
]

core_lib = env.SharedLibrary(
    target=str(plugin_root / "python" / extension_name / "_GafferScatterPaint"),
    source=core_sources
    + sse_env.SharedObject(sse_sources)
    + avx2_env.SharedObject(avx2_sources)
    + module_sources,
    LIBS=common_libs + ["GafferBindings", boost_python_lib, python_lib],
)

ui_lib = env.SharedLibrary(
    target=str(
        plugin_root / "python" / f"{extension_name}UI" / "_GafferScatterPaintUI"
    ),
    source=ui_sources + ui_module_sources,
    LIBS=common_libs
    + ["GafferUI", "GafferSceneUI", "GafferBindings", boost_python_lib, python_lib],
)

Default([core_lib, ui_lib])
