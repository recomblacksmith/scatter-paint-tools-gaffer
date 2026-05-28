import importlib
import importlib.machinery
import importlib.util
import pathlib
import sys

import GafferSceneUI  # noqa: F401


def _load_extension(module_basename, legacy_basename=None):
    module_name = f"{__name__}.{module_basename}"
    try:
        return importlib.import_module(module_name), None
    except ModuleNotFoundError as exc:
        if exc.name != module_name or not legacy_basename:
            return None, exc
    except Exception as exc:
        return None, exc

    package_dir = pathlib.Path(__file__).resolve().parent
    for suffix in importlib.machinery.EXTENSION_SUFFIXES:
        legacy_path = package_dir / f"{legacy_basename}{suffix}"
        if not legacy_path.exists():
            continue
        try:
            loader = importlib.machinery.ExtensionFileLoader(module_name, str(legacy_path))
            spec = importlib.util.spec_from_loader(module_name, loader)
            if spec is None:
                continue
            module = importlib.util.module_from_spec(spec)
            sys.modules[module_name] = module
            loader.exec_module(module)
            return module, None
        except Exception as exc:
            sys.modules.pop(module_name, None)
            return None, exc
    return None, ModuleNotFoundError(module_name)


_GafferScatterPlusUI, _GAFFER_SCATTER_PLUS_UI_IMPORT_ERROR = _load_extension(
    "_GafferScatterPlusUI", "lib_GafferScatterPlusUI"
)

__all__ = []
