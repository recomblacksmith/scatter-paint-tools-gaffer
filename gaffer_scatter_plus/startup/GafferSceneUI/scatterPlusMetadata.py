import importlib.util
from pathlib import Path


def _load_metadata_module():
    metadata_path = Path(__file__).resolve().parent.parent / "GafferScatterPlusUI" / "metadata.py"
    spec = importlib.util.spec_from_file_location("_scatterPlusMetadata", metadata_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load scatter plus metadata from {metadata_path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


_metadata = _load_metadata_module()
