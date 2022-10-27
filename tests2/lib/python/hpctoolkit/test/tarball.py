import contextlib
import tarfile
import tempfile
from pathlib import Path


@contextlib.contextmanager
def extracted(file, **kwargs):
    file = Path(file)
    with tempfile.TemporaryDirectory(**kwargs) as d:
        d = Path(d)
        with tarfile.open(file, mode="r:*") as tf:
            tf.extractall(d, numeric_owner=True)

        yield d
