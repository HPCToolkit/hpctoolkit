import contextlib
import tarfile
import tempfile
from pathlib import Path


@contextlib.contextmanager
def extracted(file, **kwargs):
    file = Path(file)
    with tempfile.TemporaryDirectory(**kwargs) as d_str:
        d = Path(d_str)
        with tarfile.open(file, mode="r:*") as tf:
            tf.extractall(d, numeric_owner=True)

        yield d
