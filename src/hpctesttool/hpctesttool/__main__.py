# SPDX-FileCopyrightText: 2023-2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

import difflib
import re
import sys
import typing
import xml.etree.ElementTree as XmlET
from pathlib import Path

import click
import pyparsing as pp
import ruamel.yaml

from . import struct_canonicalize, struct_match
from .formats import from_path, vcurrent
from .formats.diff.strict import StrictAccuracy, StrictDiff
from .match.context import MatchCtx, MatchEntryPoint, MatchFunction, chainmatch
from .test.execution import Database, Measurements

if typing.TYPE_CHECKING:
    import collections.abc


@click.group()
def main() -> None:
    """Front-end to all sorts of testing utilities."""


@main.group()
def test() -> None:
    """Specific tests."""


@test.command
@click.option("-T", "--trace/--no-trace", help="Expect to have traces")
@click.argument(
    "measurements", type=click.Path(exists=True, readable=True, file_okay=False, path_type=Path)
)
@click.argument("pattern_all", type=re.compile)
@click.argument("patterns_any", type=re.compile, nargs=-1)
@click.pass_context
def produces_profiles(
    ctx: click.Context,
    *,
    measurements: Path,
    trace: bool,
    pattern_all: re.Pattern,
    patterns_any: typing.Tuple[re.Pattern],
) -> None:
    """Test that the given MEASUREMENTS contain a sufficient number of profiles."""
    ok = True

    meas = Measurements(measurements)
    for stem in meas.thread_stems:
        ids = meas.parse_idtuple(stem)
        print(f"Found profile: {ids}")

        if trace and not meas.tracefile(stem):
            print(
                f"  Error: Expected associated trace file but none found: {stem}.hpctrace not found"
            )
            ok = False
        elif not trace and meas.tracefile(stem):
            print(f"  Error: Did not expect trace but found: {stem}.hpctrace is present")
            ok = False

        if not pattern_all.search(ids):
            print(f"  Error: Profile did not match all pattern: {pattern_all.pattern!r}")
            ok = False

    for pat in patterns_any:
        if not any(pat.search(meas.parse_idtuple(stem)) for stem in meas.thread_stems):
            print(f"Error: Did not find match for regex {pat.pattern!r}")
            ok = False

    if not ok:
        ctx.exit(1)


del produces_profiles


@test.command
@click.option("-T", "--trace/--no-trace", help="Expect a trace.db")
@click.argument(
    "database", type=click.Path(exists=True, readable=True, file_okay=False, path_type=Path)
)
def check_db(*, trace: bool, database: Path) -> None:
    """Test that the given database has the required data."""
    db = Database(database)
    db.check_standard(tracedb=trace)


del check_db


@test.command
@click.argument(
    "script",
    type=click.Path(exists=True, readable=True, dir_okay=False, path_type=Path, resolve_path=True),
)
@click.argument(
    "database", type=click.Path(exists=True, readable=True, file_okay=False, path_type=Path)
)
def unwind_py_simple(*, script: Path, database: Path) -> None:
    """Test that the given DATABASE contains a func_hi -> func_mid -> func_lo callstack.

    Specifically, searches for a callpath of the form:

    \b
    (main_thread)
    -call> [line] SCRIPT
    -call> [function] func_hi
    -lexical> [line] SCRIPT
    -call> [function] func_mid
    -lexical> [line] SCRIPT
    -call> [function] func_lo
    """  # noqa: D301
    db = from_path(database)
    assert isinstance(db, vcurrent.Database)

    for gapframes in range(4):
        for entry_point in ["main_thread", "application_thread"]:
            gap = [MatchCtx()] * gapframes
            matches = list(
                chainmatch(
                    db.meta.context,
                    MatchEntryPoint(entry_point=entry_point),
                    *gap,
                    MatchCtx(relation="call", lexical_type="line", file=str(script)),
                    MatchCtx(
                        relation="call",
                        lexical_type="function",
                        function=MatchFunction(name="func_hi", module="<logical python>"),
                    ),
                    MatchCtx(relation="lexical", lexical_type="line", file=str(script)),
                    MatchCtx(
                        relation="call",
                        lexical_type="function",
                        function=MatchFunction(name="func_mid", module="<logical python>"),
                    ),
                    MatchCtx(relation="lexical", lexical_type="line", file=str(script)),
                    MatchCtx(
                        relation="call",
                        lexical_type="function",
                        function=MatchFunction(name="func_lo", module="<logical python>"),
                    ),
                )
            )
            if matches:
                break
        if matches:
            break

    if not matches:
        raise ValueError("Unable to find match!")
    if len(matches) > 1:
        raise ValueError(f"Found {len(matches)} matches, expected 1!")


del unwind_py_simple


@test.command
@click.argument(
    "database", type=click.Path(exists=True, readable=True, file_okay=False, path_type=Path)
)
@click.argument(
    "canonical", type=click.Path(exists=True, readable=True, file_okay=False, path_type=Path)
)
def db_compare(*, database: Path, canonical: Path) -> None:
    """Compare a DATABASE against a CANONICAL database and report any differences."""
    diff = StrictDiff(from_path(database), from_path(canonical))
    acc = StrictAccuracy(diff)
    if len(diff.hunks) > 0 or acc.inaccuracy:
        diff.render(sys.stdout)
        acc.render(sys.stdout)
        raise click.ClickException("Comparison failed!")


del db_compare


@test.command
@click.argument(
    "structfile", type=click.Path(exists=True, readable=True, dir_okay=False, path_type=Path)
)
@click.argument(
    "binary", type=click.Path(exists=True, dir_okay=False, path_type=Path, resolve_path=True)
)
@click.argument(
    "sources", type=click.Path(exists=True, readable=True, dir_okay=False, path_type=Path), nargs=-1
)
@click.option(
    "--debug/--no-debug",
    default=True,
    help="Disable any matching that would rely on the presence of debug info",
)
def match_struct(
    *, structfile: Path, binary: Path, sources: "collections.abc.Iterable[Path]", debug: bool
) -> None:
    """Compare a STRUCTFILE against the lexical structure expressed in SOURCES."""
    try:
        tag = struct_match.parse_sources(sources, debug=debug, binary=str(binary))
    except pp.ParseException as e:
        raise click.ClickException("Pattern syntax error:\n" + e.explain()) from e
    except pp.ParseFatalException as e:
        raise click.ClickException("Pattern syntax error:\n" + e.explain()) from e

    data = XmlET.parse(structfile)
    found = False
    for lm in data.findall("LM"):
        found = True
        msg = tag.match(lm)
        if msg is not None:
            raise click.ClickException(msg)
    if not found:
        raise click.ClickException("Missing <LM> tag?")


del match_struct


@test.command
@click.argument("structfile", type=click.File("rb"))
@click.argument("canonical", type=click.File("rb"))
def struct_compare(*, structfile: typing.BinaryIO, canonical: typing.BinaryIO) -> None:
    """Compare a STRUCTFILE against a CANONICAL Structfile and report any differences."""
    expected = struct_canonicalize.canonical_form(canonical)
    got = struct_canonicalize.canonical_form(structfile)
    if got != expected:
        for line in difflib.unified_diff(expected, got):
            click.echo(line, nl=False)
        raise click.ClickException("Differences found between obtained and expected structure!")


del struct_compare


@test.command
@click.argument(
    "database", type=click.Path(exists=True, file_okay=False, readable=True, path_type=Path)
)
@click.argument("output", type=click.File("w", encoding="utf-8"))
def yaml(*, database: Path, output: typing.BinaryIO) -> None:
    """Transcode the given database into a YAML file."""
    ruamel.yaml.YAML(typ="rt").dump(from_path(database), output)


del yaml

if __name__ == "__main__":
    main()
