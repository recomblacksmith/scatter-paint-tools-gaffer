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
env.Append(
    CPPPATH=[
        str(plugin_root / "include"),
        str(gaffer_header_root / "include"),
        str(gaffer_runtime_root / "include"),
        str(gaffer_runtime_root / "include" / "Imath"),
        str(python_include_dir),
    ]
)
env.Append(LIBPATH=[str(gaffer_lib_dir)])
env.Append(CXXFLAGS=["-std=c++17"])
env.Append(LINKFLAGS=[f"-Wl,-rpath,{gaffer_lib_dir}"])

common_libs = ["Gaffer", "GafferScene", "GafferImage", "IECore", "IECoreScene"]

core_lib = env.SharedLibrary(
    target=str(plugin_root / "python" / extension_name / "_GafferScatterPlus"),
    source=[
        str(plugin_root / "src" / "GafferScatterPlus" / "ScatterPlus.cpp"),
        str(plugin_root / "src" / "GafferScatterPlus" / "Module.cpp"),
    ],
    LIBS=common_libs + ["GafferBindings", boost_python_lib, python_lib],
)

ui_lib = env.SharedLibrary(
    target=str(plugin_root / "python" / f"{extension_name}UI" / "_GafferScatterPlusUI"),
    source=[str(plugin_root / "src" / "GafferScatterPlusUI" / "Module.cpp")],
    LIBS=common_libs + ["GafferUI", "GafferSceneUI", "GafferImage", "GafferBindings", boost_python_lib, python_lib],
)

Default([core_lib, ui_lib])
