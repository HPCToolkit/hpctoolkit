import collections.abc
import dataclasses
import re
import typing
from xml.etree import ElementTree as XmlET


def iter_xml(root: XmlET.Element) -> collections.abc.Iterator[tuple[XmlET.Element, bool]]:
    """Iterate over an XML element tree in depth-first order with exit callbacks. Returns each
    element in the tree exactly twice, along with a boolean indicating with this is the first time
    incountering the element.
    """
    stack: list[tuple[XmlET.Element, bool]] = [(root, True)]
    while stack:
        elem, enter = stack.pop()
        yield elem, enter
        if enter:
            stack.append((elem, False))
            for subelem in elem:
                stack.append((subelem, True))


@dataclasses.dataclass(eq=True, order=True, frozen=True)
class VRange:
    min_addr: int
    max_addr: int

    def __add__(self, other: typing.Union["VRange", None]) -> "VRange":
        if other is None:
            return self
        return self.__class__(
            min(self.min_addr, other.min_addr), max(self.max_addr, other.max_addr)
        )

    __radd__ = __add__

    @classmethod
    def parse(cls, v: str | None) -> typing.Union["VRange", None]:
        if v is None or v == "{}":
            return None
        if v[0] != "{" or v[-1] != "}":
            raise ValueError(f"Invalid v=* attribute: {v!r}")

        result = None
        for part in v[1:-1].split():
            mat = re.fullmatch(r"\[0x([0-9a-f]+)-0x([0-9a-f]+)\)", part)
            if not mat:
                raise ValueError(f"Invalid v=* range: {part!r} (from {v!r})")
            result += cls(int(mat.group(1), base=16), int(mat.group(2), base=16))
        return result


def canonical_form(file: typing.BinaryIO) -> list[str]:  # noqa: C901
    data = XmlET.parse(file)

    # 1. Calculate a VRange for each element, based on the address range ("v") and children
    ranges: dict[XmlET.Element, VRange] = {}
    for elem, enter in iter_xml(data.getroot()):
        if enter:
            continue  # Post-order to ensure we have ranges for children
        vrange = VRange.parse(elem.get("v"))
        for child in elem:
            vrange += ranges[child]
        if vrange is None:
            raise ValueError(f"No address range for element {elem}")
        ranges[elem] = vrange

    # 2. Sort the children of each element by their address ranges
    children: dict[XmlET.Element, list[XmlET.Element]] = {}
    for elem in data.iter():
        children[elem] = sorted(elem, key=lambda e: ranges[e], reverse=True)  # noqa: F821
    del ranges
    for parent, new_children in children.items():
        parent[:] = new_children
    del children

    # 3a. Strip and canonicalize the identifier attribute ("i") from all tags
    for elem in data.iter():
        if elem.get("i") is not None:
            elem.set("i", "<<ID>>")
    # 3b. Strip the static identifier attribute ("s") from <P> tags
    for elem in data.iter("P"):
        if elem.get("s") is not None:
            del elem.attrib["s"]
    # 3c. Strip and canonicalize the load module path ("n") from <LM> tags
    for elem in data.iter("LM"):
        if elem.get("n") is not None:
            elem.set("n", "<<BINARY PATH>>")
    # 3d. Strip parent paths from source file paths ("n" or "f") from <F>, <A> and <L> tags
    for elem in data.iter("F"):
        fn = elem.get("n")
        if fn is not None and "/" in fn:
            elem.set("n", "<<PARENT PATH>>/" + fn.rsplit("/", 1)[1])
    for tag in ("A", "L"):
        for elem in data.iter(tag):
            fn = elem.get("f")
            if fn is not None and "/" in fn:
                elem.set("f", "<<PARENT PATH>>/" + fn.rsplit("/", 1)[1])

    # 4. Output the canonical form of the element tree
    depth = 0
    result: list[str] = []
    for elem, enter in iter_xml(data.getroot()):
        if enter:
            depth += 1
            attrs = " ".join(f"{k}={elem.get(k)!r}" for k in sorted(elem.keys()))
            result.append("  " * depth + f"<{elem.tag} {attrs}>\n")
        else:
            result.append("  " * depth + f"</{elem.tag}>\n")
            depth -= 1
    return result
