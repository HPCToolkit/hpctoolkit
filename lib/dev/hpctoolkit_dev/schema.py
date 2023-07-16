import importlib.resources
import json

from . import schema_data

__all__ = ("request",)

schema_files = importlib.resources.files(schema_data)

with (schema_files / "request.schema.json").open(encoding="utf-8") as schemaf:
    request = json.load(schemaf)
