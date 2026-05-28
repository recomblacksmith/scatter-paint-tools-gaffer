import importlib.util
import pathlib


def _load_metadata_module():
    metadata_path = (
        pathlib.Path(__file__).resolve().parent.parent
        / "GafferScatterPaintUI"
        / "metadata.py"
    )
    spec = importlib.util.spec_from_file_location(
        "_scatterPaintMetadata", metadata_path
    )
    if spec is None or spec.loader is None:
        raise RuntimeError(
            f"Unable to load scatter paint metadata from {metadata_path}"
        )
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


try:
    _metadata = _load_metadata_module()
except TypeError:
    _metadata = None
