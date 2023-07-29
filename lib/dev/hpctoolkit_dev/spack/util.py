import re
import typing


@typing.final
class Version:
    """Single version for a package, as understood by Spack.

    Ordered comparisons are weakly ordered based on Spack's understanding of
    satisfaction: x <= y only if x satisfies @:y or one is a prefix of the other.
    """

    VERSION_REGEX: typing.ClassVar = re.compile(r"[A-Za-z0-9_.-][A-Za-z0-9_.-]*")
    SEGMENT_REGEX: typing.ClassVar = re.compile(r"(?:([0-9]+)|([a-zA-Z]+))([_.-]*)")
    SPEC_VERSION_REGEX: typing.ClassVar = re.compile(r"@=?([A-Za-z0-9_.-:,]+)")
    INFINITY_SEGMENTS: typing.ClassVar = ("stable", "trunk", "head", "master", "main", "develop")

    __slots__ = ["_original", "_parsed"]
    _original: str
    _parsed: tuple[int | str, ...]

    def __init__(self, version: str) -> None:
        """Create a new Version from the specified version number."""
        if not self.VERSION_REGEX.fullmatch(version):
            raise ValueError(version)
        self._original = version
        self._parsed = tuple(
            alpha or int(num) for num, alpha, _ in self.SEGMENT_REGEX.findall(version)
        )

    @classmethod
    def from_spec(
        cls, spec: str
    ) -> list[tuple[typing.Optional["Version"], typing.Optional["Version"]]]:
        """Parse a full Spack spec for its version constraints, as pairs of Versions."""
        result = []
        for con in cls.SPEC_VERSION_REGEX.findall(spec):
            for rang in con.split(","):
                vers = rang.split(":")
                if len(vers) > 2:
                    raise ValueError(f"Invalid Spack version range: {rang!r}")
                if len(vers) == 2:
                    a = cls(vers[0]) if vers[0] else None
                    b = cls(vers[1]) if vers[1] else None
                    if a and b and a > b:
                        raise ValueError(f"Invalid Spack version range, {a} > {b}: {rang!r}")
                    result.append((a, b))
                else:
                    ver = cls(vers[0]) if vers[0] else None
                    result.append((ver, ver))
        return sorted(result)

    def __str__(self) -> str:
        return self._original

    @classmethod
    def _seg_le(cls, a: int | str, b: int | str) -> bool:
        """Determine if a <= b as version segments."""
        if isinstance(a, int):
            # If a and b are ints, compare directly.
            # If b is a str, a < b if b is an infinity and b < a otherwise.
            return a <= b if isinstance(b, int) else b in cls.INFINITY_SEGMENTS

        if a in cls.INFINITY_SEGMENTS:
            # If a and b are both infinities, compare by infinity "degree."
            # If b is not an infinity, b < a.
            return (
                cls.INFINITY_SEGMENTS.index(a) <= cls.INFINITY_SEGMENTS.index(b)
                if b in cls.INFINITY_SEGMENTS
                else False
            )

        # If a and b are both strings, compare lexicographically.
        # If b is an infinity or is an int, a < b.
        return isinstance(b, int) or b in cls.INFINITY_SEGMENTS or a <= b

    def __le__(self, other: typing.Any) -> bool:
        if not isinstance(other, Version):
            return NotImplemented

        # If the parsed forms differ in a segment, that segment determines the comparison
        for pa, pb in zip(self._parsed, other._parsed, strict=False):
            if pa != pb:
                return self._seg_le(pa, pb)
        # One is a prefix of the other, they compare equivalent.
        return True

    def __ge__(self, other: typing.Any) -> bool:
        if not isinstance(other, Version):
            return NotImplemented

        # If the parsed forms differ in a segment, that segment determines the comparison
        for pa, pb in zip(self._parsed, other._parsed, strict=False):
            if pa != pb:
                return not self._seg_le(pa, pb)
        # One is a prefix of the other, they compare equivalent.
        return True

    def __lt__(self, other: typing.Any) -> bool:
        return not self.__ge__(other)

    def __gt__(self, other: typing.Any) -> bool:
        return not self.__le__(other)

    def __eq__(self, other: typing.Any) -> bool:
        if not isinstance(other, Version):
            return NotImplemented
        return self._parsed == other._parsed

    def __ne__(self, other: typing.Any) -> bool:
        return not self.__eq__(other)
