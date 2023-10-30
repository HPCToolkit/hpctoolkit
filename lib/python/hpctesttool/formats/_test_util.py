import io
import os
from pathlib import Path

import pytest
import ruamel.yaml

from . import base

testdatadir = Path(os.environ["TEST_DATA_DIR"])


@pytest.fixture()
def yaml():
    return ruamel.yaml.YAML(typ="safe")


def dump_to_string(yaml, data) -> str:
    sf = io.StringIO()
    yaml.dump(data, sf)
    return sf.getvalue()


def assert_good_traversal(obj, seen=None):
    if seen is None:
        seen = set()

    if obj is None or isinstance(obj, (str, int, float, base.Enumeration, base.BitFlags)):
        pass
    elif isinstance(obj, base.StructureBase):
        assert obj not in seen, f"Object {obj!r} was observed twice during the traversal!"
        seen.add(obj)
        for f in obj.owning_fields():
            assert_good_traversal(getattr(obj, f.name), seen=seen)
    elif isinstance(obj, dict):
        for k, v in obj.items():
            assert isinstance(
                k, (str, int, float, base.Enumeration, base.BitFlags)
            ), f"Bad traversal, observed invalid key {k!r}"
            assert_good_traversal(v, seen=seen)
    elif isinstance(obj, list):
        for v in obj:
            assert_good_traversal(v, seen=seen)
    else:
        raise AssertionError(f"Bad traversal, encountered non-Structure object {obj!r}")
