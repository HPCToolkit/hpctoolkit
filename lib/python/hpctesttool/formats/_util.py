import contextlib
import struct


@contextlib.contextmanager
def preserve_filepos(file):
    oldpos = file.tell()
    try:
        yield file
    finally:
        file.seek(oldpos)


def read_nbytes(file, sz: int, offset: int):
    with preserve_filepos(file):
        file.seek(offset)
        src = file.read(sz)
    if len(src) < sz:
        raise OSError("EOF reached before end of structure!")
    return src


class VersionedStructure:
    """Series of binary fields with an associated (single) version number. Somewhat like
    struct.Struct but with versions and a few extra methods. Unlike struct.Struct the "unpacked"
    form is always a dict, where keys are the field names and values are the values.

    Most struct.Struct single format characters (but not endians or counts) can be used, the
    exceptions are 'e' (half-float), 's' (fixed string), 'p' (Pascal-style string), and 'P' (void*).
    These cannot be used under any circumstances.
    """

    __slots__ = ["_fields"]

    @staticmethod
    def _structfor(endian, form):
        if form in tuple("cbB?hHiIlLqQfd"):
            return struct.Struct(endian + form)
        raise ValueError(f"Invalid format specification: {form}")

    def __init__(self, endian: str, /, **fields: tuple[int, int, str]):
        """Create a new VersionedStructure with the given fields."""
        assert endian in (">", "<", "=")
        self._fields = {
            name: (version, offset, self._structfor(endian, form))
            for name, (version, offset, form) in fields.items()
        }

    def size(self, version):
        """Return the total size in bytes of this structure for the given version."""
        return max(o + s.size for _, (v, o, s) in self._fields.items() if v <= version)

    def fields(self, version):
        """Return the names of fields available for the given version."""
        return (n for n, (v, _, _) in self._fields.items() if v <= version)

    def unpack(self, version, buffer):
        """Unpack the given buffer based on the given version number."""
        return self.unpack_from(version, buffer, offset=0)

    def unpack_from(self, version, buffer, offset=0):
        """Unpack from given buffer (at offset) based on the given version number."""
        sz = self.size(version)
        src = memoryview(buffer)[offset : offset + sz]
        if len(src) != sz:
            raise ValueError(
                f"buffer is not large enough, must be at least {offset + sz} bytes: got {len(buffer)} bytes"
            )
        out = {}
        for k, (v, o, s) in self._fields.items():
            if v <= version:
                (out[k],) = s.unpack_from(src, o)
        return out

    def unpack_file(self, version: int, file, offset: int):
        """Unpack from the given file at the given offset."""
        return self.unpack(version, read_nbytes(file, self.size(version), offset))


def read_ntstring(file, offset):
    """Read a null-terminated string from the file starting at offset, or the current stream
    position if None.
    """
    oldpos = file.tell()
    file.seek(offset)
    out = b""
    last = file.read(1)
    if not isinstance(last, bytes):
        raise TypeError("read_ntstring only supports binary files")
    while last and last != b"\0":
        out += last
        last = file.read(1)
    if last != b"\0":
        raise ValueError("EOF reached before end of string")
    file.seek(oldpos)
    return out.decode("utf-8")
