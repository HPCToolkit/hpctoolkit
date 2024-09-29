# SPDX-FileCopyrightText: 2023-2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

import abc
import re
import string
import typing
import xml.etree.ElementTree as ET
from pathlib import Path

import pyparsing as pp

if typing.TYPE_CHECKING:
    import collections.abc


class ValueLike(abc.ABC):
    @abc.abstractmethod
    def as_value(
        self,
        *,
        bounds: typing.Tuple[int, int],
        par_bounds: typing.Optional[typing.Tuple[int, int]] = None,
    ) -> "Value":
        pass


class Value(ValueLike):
    def as_value(
        self,
        *,
        bounds: typing.Tuple[int, int],
        par_bounds: typing.Optional[typing.Tuple[int, int]] = None,
    ) -> "Value":
        return self

    @abc.abstractmethod
    def matches(self, value: str) -> bool:
        pass

    @classmethod
    def parser(cls, *, file: str, binary: str, debug: bool) -> pp.ParserElement:
        return (
            LiteralValue.parser(file=file, binary=binary, debug=debug)
            | AnyValue.parser()
            | InboundsPlaceholder.parser(debug=debug)
            | IntRangeValue.parser(debug=debug)
        ).set_name("value")


class InboundsPlaceholder(ValueLike):
    def __init__(self, *, parent: bool):
        self.parent_bounds = parent

    def __str__(self) -> str:
        return "<inparbounds>" if self.parent_bounds else "<inbounds>"

    def as_value(
        self,
        *,
        bounds: typing.Tuple[int, int],
        par_bounds: typing.Optional[typing.Tuple[int, int]] = None,
    ) -> typing.Union["IntRangeValue", "AnyValue"]:
        if self.parent_bounds:
            return (
                IntRangeValue(par_bounds[0], par_bounds[1])
                if par_bounds is not None
                else AnyValue()
            )
        return IntRangeValue(bounds[0], bounds[1])

    @classmethod
    def parser(cls, *, debug: bool) -> pp.ParserElement:
        def lift(
            toks: pp.ParseResults,
        ) -> typing.Union[InboundsPlaceholder, IntRangeValue]:
            if not debug:
                return IntRangeValue(0)
            if toks[0] == "inbounds":
                return cls(parent=False)
            if toks[0] == "inparbounds":
                return cls(parent=True)
            raise AssertionError

        val = (
            pp.Literal("inbounds") | "inparbounds"
        )  # pylint: disable=unsupported-binary-operation
        val.set_parse_action(lift)
        return val


class AnyValue(Value):
    def __str__(self) -> str:
        return "*"

    def matches(self, value: str) -> bool:
        return True

    @classmethod
    def parser(cls, **_kwargs) -> pp.ParserElement:
        # pylint: disable=unnecessary-lambda
        return pp.Literal("*").set_parse_action(lambda: cls())


class LiteralValue(Value):
    def __init__(
        self,
        literal: str,
        *,
        wildcard_prefix: bool = False,
        bracket_suffix: typing.Optional[bool] = False,
    ):
        self.literal = literal
        self.wildcard_prefix = wildcard_prefix
        self.bracket_suffix = bracket_suffix

    def __str__(self) -> str:
        prefix = "**" if self.wildcard_prefix else ""
        suffix = "[*]" if self.bracket_suffix or self.bracket_suffix is None else ""
        if self.bracket_suffix is None:
            suffix += "?"
        return f"{prefix}{self.literal!r}{suffix}"

    def matches(self, value: str) -> bool:
        # Match and remove any bracket suffix, first
        if self.bracket_suffix or self.bracket_suffix is None:
            value, subs = re.subn(r"\s*\[[^]]+\]\s*$", "", value, count=0)
            if subs == 0 and self.bracket_suffix:
                return False  # Expected bracket suffix but not found

        # Match the literal on the rest
        return (
            value.endswith(self.literal)
            if self.wildcard_prefix
            else value == self.literal
        )

    @classmethod
    def parser(cls, *, file: str, binary: str, debug: bool) -> pp.ParserElement:
        wprefix = pp.Opt(pp.Literal("**")("any"))

        @wprefix.set_parse_action
        def parse_wildcard(toks: pp.ParseResults) -> typing.List[bool]:
            return [bool(toks.any)]

        del parse_wildcard

        bsuffix = pp.Opt(
            pp.Combine(
                pp.Literal("[")
                + (
                    pp.Literal("*")("always")
                    ^ pp.Literal("dbg:*")("debugonly")
                    ^ pp.Literal("nodbg:*")("nodebugonly")
                    ^ pp.Literal("*?")("opt")
                )
                + "]"
            )
        )

        @bsuffix.set_parse_action
        def parse_brackets(toks: pp.ParseResults) -> typing.List[typing.Optional[bool]]:
            if toks.always:
                return [True]
            if toks.opt:
                return [None]
            if toks.debugonly:
                return [debug]
            if toks.nodebugonly:
                return [not debug]
            return [False]

        del parse_brackets

        std = pp.Combine(
            wprefix("wpfx") + pp.QuotedString('"')("literal") + bsuffix("bs")
        )

        @std.set_parse_action
        def lift_std(toks: pp.ParseResults) -> LiteralValue:
            return cls(toks.literal, wildcard_prefix=toks.wpfx, bracket_suffix=toks.bs)

        del lift_std

        c_file = pp.Combine(pp.Literal("file") + bsuffix("bs"))

        @c_file.set_parse_action
        def lift_file(toks: pp.ParseResults) -> LiteralValue:
            return cls(file if debug else "", bracket_suffix=toks.bs)

        del lift_file

        c_binary = pp.Literal("binary").set_parse_action(lambda: cls(binary))

        return c_file | c_binary | std


class IntRangeValue(Value):
    def __init__(
        self, min_val: int, max_val: typing.Optional[int] = None, *, plus: int = 0
    ):
        if plus < 0:
            raise ValueError(plus)
        if max_val is not None and min_val > max_val:
            raise ValueError((min_val, max_val))

        self.min_val = min_val
        if max_val is not None:
            self.max_val = max_val
        else:
            self.max_val = min_val + plus

    def __str__(self) -> str:
        return (
            f"<in [{self.min_val:d}, {self.max_val:d}]>"
            if self.min_val != self.max_val
            else f"{self.min_val:d}"
        )

    def matches(self, value: str) -> bool:
        try:
            ivalue = int(value, base=10)
        except ValueError:
            return False

        return self.min_val <= ivalue <= self.max_val

    @classmethod
    def parser(cls, *, debug: bool, **_kwargs) -> pp.ParserElement:
        return (
            pp.Literal("line").set_parse_action(
                lambda s, ln, _t: cls(pp.lineno(ln, s)) if debug else cls(0)
            )
            ^ pp.Literal("nextline").set_parse_action(
                lambda s, ln, _t: cls(pp.lineno(ln, s) + 1) if debug else cls(0)
            )
            ^ pp.Literal("lineornext").set_parse_action(
                lambda s, ln, _t: cls(pp.lineno(ln, s), plus=1) if debug else cls(0)
            )
        )


def dict_to_attrs(
    d: "collections.abc.Mapping[str, typing.Any]", *, r: bool = False
) -> typing.List[str]:
    return [f"{k}={repr(v) if r else v}" for k, v in d.items()]


class Tag:
    def __init__(
        self,
        tag: str,
        attrs: "collections.abc.Mapping[str, ValueLike]",
        children: "collections.abc.Iterable[Tag]",
        *,
        match_multiple: bool = False,
        match_none: bool = False,
        allow_extra_children: bool = False,
        line_bounds: typing.Tuple[int, int],
    ):
        self.tag = tag
        self.attrs = attrs
        self.children = tuple(children)
        self.allow_extra_children = allow_extra_children
        self.line_bounds = line_bounds
        self.match_multiple = match_multiple
        self.match_none = match_none

    def __str__(self) -> str:
        bits = [
            self.tag
            + {
                (False, False): "",
                (False, True): "?",
                (True, False): "+",
                (True, True): "*",
            }[self.match_multiple, self.match_none]
        ]
        bits.extend(dict_to_attrs(self.attrs))
        if self.allow_extra_children:
            bits.append("<*>*")
        return "<" + " ".join(bits) + ">"

    class MatchFailureError(Exception):
        def __init__(self, msg: str):
            super().__init__("Uncaught MatchFailureError exception!")
            self.msg = msg

    def match(self, elem: ET.Element) -> typing.Optional[str]:
        if not self._matches_root(elem):
            return f"Root tag failed to match: <{elem.tag} {elem.attrib}> does not match {self}"

        try:
            self._match_children(elem)
            return None
        except self.MatchFailureError as e:
            return e.msg

    def _matches_root(
        self,
        elem: ET.Element,
        *,
        par_bounds: typing.Optional[typing.Tuple[int, int]] = None,
    ) -> bool:
        return (
            elem.tag == self.tag
            and all(k in self.attrs for k in elem.attrib)
            and all(
                v.as_value(bounds=self.line_bounds, par_bounds=par_bounds).matches(
                    elem.get(k, "")
                )
                for k, v in self.attrs.items()
            )
        )

    def _match_children(
        self, elem: ET.Element, path: typing.Optional[str] = None
    ) -> None:
        path = (
            path + "/" if path is not None else ""
        ) + f"{self.tag}({self.line_bounds[0]}-{self.line_bounds[1]})"

        remaining = list(elem)
        for m_none in (False, True):
            for m_multi in (False, True):
                for child in self.children:
                    if (
                        bool(child.match_none) is not m_none
                        or bool(child.match_multiple) is not m_multi
                    ):
                        continue

                    matches = [
                        e
                        for e in remaining
                        if child._matches_root(e, par_bounds=self.line_bounds)
                    ]
                    if len(matches) == 0 and not child.match_none:
                        msg = "\n".join(
                            [
                                f"{path}: Did not find match for {child} ({child.line_bounds[0]}-{child.line_bounds[1]}) in:"
                            ]
                            + [
                                f"  - <{e.tag} {' '.join(dict_to_attrs(e.attrib, r=True))}>"
                                for e in remaining
                            ]
                        )
                        raise self.MatchFailureError(msg)
                    if len(matches) > 1 and not child.match_multiple:
                        msg = "\n".join(
                            [
                                f"{path}: Found multiple matches for {child} ({child.line_bounds[0]}-{child.line_bounds[1]}):"
                            ]
                            + [
                                f"  - <{e.tag} {' '.join(dict_to_attrs(e.attrib, r=True))}>"
                                for e in matches
                            ]
                        )
                        raise self.MatchFailureError(msg)

                    for e in matches:
                        remaining.remove(e)
                        child._match_children(e, path)

        if remaining and not self.allow_extra_children:
            msg = "\n".join(
                [f"{path}: Some children were not matched:"]
                + [
                    f"  - <{e.tag} {' '.join(dict_to_attrs(e.attrib, r=True))}>"
                    for e in remaining
                ]
            )
            raise self.MatchFailureError(msg)

    @classmethod
    def predefined_parser(cls) -> pp.ParserElement:
        return pp.Combine(
            pp.Literal("!") + pp.CharsNotIn(string.whitespace)("name")
        ).set_name("macro")

    @classmethod
    def parser(
        cls, predefined_parser: pp.ParserElement, *, debug: bool, file: str, binary: str
    ) -> pp.ParserElement:
        mode = (
            pp.Literal("?").set_parse_action(lambda: (False, True))
            | pp.Literal("*").set_parse_action(lambda: (True, True))
            | pp.Literal("+").set_parse_action(lambda: (True, False))
            | pp.Empty().set_parse_action(lambda: (False, False))
        ).set_name("mode")
        attrs = pp.Group(
            pp.Combine(
                pp.Word(pp.alphas + "_-")("key")
                + "="
                + Value.parser(file=file, binary=binary, debug=debug)("value")
            )
        ).set_name("attribute")[...]

        @attrs.set_parse_action
        def lift_attrs(toks: pp.ParseResults) -> typing.Dict[str, Value]:
            return {a.key: a.value for a in toks}

        del lift_attrs

        dbg_prefix = pp.Opt(
            pp.Literal("dbg:")("debugonly") | pp.Literal("nodbg:")("nodebugonly")
        )

        shorthand_tag = (
            pp.Combine(
                dbg_prefix + "<" + pp.Word(pp.alphas)("tag") + "/" + mode("mode")
            )
        ).set_name("short-tag") - attrs("attrs")

        @shorthand_tag.set_parse_action
        def lift_shorthand(s: str, loc: int, toks: pp.ParseResults) -> typing.List[Tag]:
            if (toks.debugonly and not debug) or (toks.nodebugonly and debug):
                return []
            m_multi, m_none = toks.mode
            lineno = pp.lineno(loc, s)
            return [
                cls(
                    toks.tag,
                    toks.attrs,
                    match_multiple=m_multi,
                    match_none=m_none,
                    line_bounds=(lineno, lineno),
                    children=[],
                )
            ]

        del lift_shorthand

        lineno = pp.Empty()

        @lineno.set_parse_action
        def get_lineno(s: str, loc: int, _toks: pp.ParseResults) -> int:
            return pp.lineno(loc, s)

        del get_lineno

        open_tag = pp.Combine(
            dbg_prefix + "<" + pp.Word(pp.alphas)("opentag") + mode("mode")
        ).set_name("tag-open")
        close_tag = pp.Combine(
            "</" + pp.Word(pp.alphas)("closetag") + lineno("clineno")
        ).set_name("tag-close")

        tag = pp.Forward()
        paired_tag = (
            open_tag
            - attrs("attrs")
            - pp.Opt("<*>*")("unmatched")
            - tag[...]("children")
            - close_tag
        )

        @paired_tag.set_parse_action
        def lift(s: str, loc: int, toks: pp.ParseResults) -> typing.List[Tag]:
            if (toks.debugonly and not debug) or (toks.nodebugonly and debug):
                return list(toks.children)
            if toks.opentag != toks.closetag:
                raise ValueError(
                    f"Tag <{toks.opentag} was closed with </{toks.closetag} (line: {toks.clineno:d})"
                )
            m_multi, m_none = toks.mode
            lineno = pp.lineno(loc, s)
            return [
                cls(
                    toks.opentag,
                    toks.attrs,
                    match_multiple=m_multi,
                    match_none=m_none,
                    allow_extra_children=bool(toks.unmatched),
                    line_bounds=(lineno, toks.clineno),
                    children=toks.children,
                )
            ]

        del lift

        tag <<= predefined_parser | shorthand_tag | paired_tag
        return tag


def parse_sources(  # noqa: C901
    sources: "collections.abc.Iterable[Path]", *, debug: bool, binary: str
) -> Tag:
    # pylint: disable=too-many-locals
    definitions: typing.Dict[str, typing.List[Tag]] = {}
    def_parser = Tag.predefined_parser()

    @def_parser.set_parse_action
    def expand(toks: pp.ParseResults) -> typing.List[Tag]:
        return definitions[toks.name]

    del expand

    macros: typing.Dict[str, str] = {}

    def expand_macro(m) -> str:
        return macros[m[1]]

    result: typing.Optional[Tag] = None
    for source in sources:
        file = source.resolve(strict=True).as_posix()
        parser = Tag.parser(def_parser, file=file, binary=binary, debug=debug)

        macros = {}
        lines: typing.List[str] = []
        in_def: typing.Optional[typing.Tuple[str, typing.List[str]]] = None
        with open(source, encoding="utf-8") as f:
            for line in f:
                lines.append("")
                if in_def is not None:
                    in_def[1].append("")

                mat = re.search(r"\b(CHECK|DECLARE|DEFINE|ENDDEFINE):\s+(.+)", line)
                if not mat:
                    continue
                if mat[1] == "DECLARE":
                    bits = mat[2].split(maxsplit=1)
                    name = bits[0]
                    value = bits[1]
                    if not name.startswith("!!"):
                        raise ValueError("DECLARE macros must start with '!!'")
                    macros[name[2:]] = value
                elif mat[1] == "DEFINE":
                    if in_def is not None:
                        raise ValueError("Nested DEFINEs not allowed")
                    if not mat[2].startswith("!"):
                        raise ValueError("DEFINE subcheck names must start with '!'")
                    in_def = (mat[2][1:].strip(), [""] * len(lines))
                    if re.search(r"\s", in_def[0]):
                        raise ValueError(
                            "DEFINE subcheck names must not contain spaces"
                        )
                    if in_def[0] in definitions:
                        raise ValueError(f"Attempt to re-DEFINE macro !{in_def[0]}")
                elif mat[1] == "ENDDEFINE":
                    if in_def is None:
                        raise ValueError("ENDDEFINE without a prior DEFINE")
                    def_name, def_lines = (  # pylint: disable=unpacking-non-sequence
                        in_def
                    )
                    if not mat[2].startswith("!") or def_name != mat[2][1:].strip():
                        raise ValueError(
                            f"Mismatched ENDDEFINE for macro !{def_name}, got {mat[2].strip()}"
                        )
                    definitions[def_name] = list(
                        parser.parse_string("\n".join(def_lines), parse_all=True)
                    )
                    in_def = None
                elif mat[1] == "CHECK":
                    expr = re.sub(
                        r"!!(\S+)",
                        expand_macro,
                        mat[2],
                    )
                    (
                        in_def[1]  # pylint: disable=unsubscriptable-object
                        if in_def is not None
                        else lines
                    )[-1] = (" " * mat.start(2) + expr)
                else:
                    raise AssertionError

        if in_def is not None:
            raise ValueError(f"Missing ENDDEFINE for !{in_def[0]} at EOF")
        if any(not line.isspace() for line in lines):
            if result is not None:
                raise ValueError("Only one file may define the top-level tag")
            result = parser.parse_string("\n".join(lines), parse_all=True)[0]

    if result is None:
        raise ValueError("No nonempty CHECK directives gathered from input")
    return result
