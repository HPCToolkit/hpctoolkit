import argparse
import os
import re
import shlex
import shutil
import statistics
import string
import subprocess
import sys
import tempfile
import time
import typing as T
from pathlib import Path

from .action import Action, ActionResult, action_sequence, summarize_results
from .buildsys import (
    Build,
    CheckInstallManifest,
    Configure,
    GenTestData,
    Install,
    InstallTestData,
    Test,
)
from .configuration import Configuration, DependencyConfiguration, Unsatisfiable
from .logs import FgColor, colorize, print_header, section

actions = {
    "configure": (Configure,),
    "build": (Build,),
    "install": (Install,),
    "check-install": (CheckInstallManifest,),
    "test": (Test,),
    "gen-testdata": (GenTestData,),
    "install-testdata": (InstallTestData,),
}


def build_parser() -> argparse.ArgumentParser:
    def source(src: str) -> tuple[Path, Path] | tuple[Path]:
        return tuple(Path(p) for p in src.split(";", 1))

    def action(act: str) -> Action:
        if act not in actions:
            print(f"Invalid action {act}, must be one of {', '.join(actions)}")
            raise ValueError()
        return act

    parser = argparse.ArgumentParser(
        description="Build front-end for HPCToolkit('s CI)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""\
Each SRC above can be one of the following:
 - A path to a configure-configuration file listing arguments to `./configure`, one per line.
   ${CC} and ${CXX} will be replaced with the compiler.
 - A path to a directory containing such a file named `config`. If this is also a Spack environment,
   it will be automatically installed and activated during the build. ${CTX} in the `config` will be
   replaced by the environment directory.
 - Two paths separated by a semicolon (<bdir>\\;<cdir>). The `config` in the first is used and the
   second is Spack installed and activated. ${CTX} in the `config` is replaced by the second.

The SPEC above is very similar to the Spack boolean variant syntax, a SPEC lists keys separated by
whitespace and prefixed by `+` or `~`. The allowed keys are:
    +mpi:  Enable Prof-MPI support
    +debug:  Enable extra debug flags
    +papi:  Enable PAPI support
    +opencl:  Enable OpenCL support
    +cuda:  Enable Nvidia CUDA support
    +rocm: Enable AMD ROCm support
    +level0:  Enable Intel Level Zero support
These flags more-or-less reflect the variants of the same name in the official Spack recipe, but
there is no enforcement for this and there may be differences.

The output from this program is summarized to only include warnings or errors, and is wrapped in
collapsible sections to allow for easy viewing from the GitLab UI.

Examples:
 - Build all non-MPI builds, using the latest version of dependencies
        python -m ci.buildfe -a build -s '~mpi' ci/dependencies/latest/

 - Build and install a single specific spec, keeping the install around afterwards
        python -m ci.buildfe -a install -ks '+mpi ~debug ...' ci/dependencies/latest/
""",
    )
    parser.add_argument(
        "sources",
        metavar="SRC",
        nargs="+",
        type=source,
        help="Source of dependency packages, see below for format",
    )
    parser.add_argument(
        "-l", "--logs-dir", type=Path, help="Top-level directory to save log outputs to"
    )
    parser.add_argument(
        "-f",
        "--log-format",
        default="{0}",
        help="Format-string to use to generate the log output subdirectory",
    )
    parser.add_argument(
        "-a",
        "--action",
        required=True,
        type=action,
        action="append",
        help="Add an action to the of actions done by this sequence",
    )
    parser.add_argument(
        "-s",
        "--spec",
        metavar="SPEC",
        type=Configuration.parse,
        action="append",
        help="Only build variants matching this spec. Can be repeated to union specs",
    )
    parser.add_argument(
        "-u",
        "--unsatisfiable",
        metavar="SPEC",
        type=Configuration.parse,
        action="append",
        help="Mark variants matching this spec as known to be unsatisfiable",
    )
    parser.add_argument(
        "-1",
        "--single-spec",
        action="store_true",
        default=False,
        help="Only build one spec, error if more than one matches",
    )
    parser.add_argument(
        "-k",
        "--keep",
        action="store_true",
        default=False,
        help="Keep the build and install directories around after the command completes. Implies --single-spec",
    )
    parser.add_argument(
        "--spack-args",
        type=shlex.split,
        default=[],
        help="Extra arguments to pass to spack install",
    )
    parser.add_argument(
        "--ccache-stats",
        default=False,
        action="store_true",
        help="Report per-build ccache statistics. Note that this clears the ccache statistics",
    )
    return parser


def post_parse(args):
    if args.keep:
        args.single_spec = True
    args.action = action_sequence([act() for name in args.action for act in actions[name]])
    return args


def load_src(
    depcfg: DependencyConfiguration, src: tuple[Path, Path] | tuple[Path], spack_args: list[str]
) -> None:
    if len(src) == 1:
        if src[0].is_file():
            # Single configuration file
            depcfg.load(src[0])
            return
        if not src[0].is_dir():
            print(f"Invalid source, not a file or directory: {src[0]}", file=sys.stderr)
            sys.exit(2)

        bdir, cdir = src[0], src[0]
    else:
        bdir, cdir = src
        if not bdir.is_dir():
            print(f"Invalid source, not a directory: {bdir}", file=sys.stderr)
            sys.exit(2)
        if not cdir.is_dir():
            print(f"Invalid source, not a directory: {cdir}", file=sys.stderr)
            sys.exit(2)

    if not (bdir / "config").is_file():
        print(f"Invalid source directory, no config file: {bdir}", file=sys.stderr)
        sys.exit(2)
    depcfg.load(bdir / "config", cdir)

    # If the second directory is a Spack environment, install and activate it
    if (cdir / "spack.yaml").exists():
        load_spackenv(cdir, spack_args)


def shlex_iter(lexer: shlex.shlex):
    while True:
        token = lexer.get_token()
        if token == lexer.eof:
            return
        yield token


def load_spackenv(cdir: Path, spack_args) -> None:
    spack = shutil.which("spack")
    if not spack:
        print("Spack environment given as a source but `spack` not available!", file=sys.stderr)
        sys.exit(2)
    assert isinstance(spack, str)

    with section(f"spack install {cdir.as_posix()}...", collapsed=True):
        # Save and restore the spack.yaml to keep things sane between runs
        # See https://github.com/spack/spack/issues/31091
        with tempfile.SpooledTemporaryFile(mode="w+") as storef:
            with open(cdir / "spack.yaml", encoding="utf-8") as f:
                shutil.copyfileobj(f, storef)
            storef.seek(0)
            subprocess.run([spack, "-e", cdir, "install", *spack_args], check=True)
            with open(cdir / "spack.yaml", "w", encoding="utf-8") as f:
                shutil.copyfileobj(storef, f)

    envload = subprocess.run(
        [spack, "env", "activate", "--sh", cdir], stdout=subprocess.PIPE, text=True, check=True
    )
    lexer = shlex.shlex(envload.stdout, posix=True, punctuation_chars=True)
    lexer.wordchars = string.ascii_letters + string.digits + "%*+,-./:=?@\\^_~"

    in_cmd = None
    for word in shlex_iter(lexer):
        match word:
            case "alias" if not in_cmd:
                in_cmd = "alias"
            case "export" if not in_cmd:
                in_cmd = "export"
            case ";" if in_cmd:
                in_cmd = None
            case _ if not in_cmd:
                print(
                    f"Bad environment modification encountered from Spack, first bad word: {word}"
                )
                print(envload.stdout)
                sys.exit(2)
            case _ if in_cmd == "export":
                parts = word.split("=", 1)
                if len(parts) != 2:
                    print(
                        f"Bad environment modification encountered from Spack, first bad word: {word}"
                    )
                    print(envload.stdout)
                    sys.exit(2)
                var, val = parts
                os.environ[var] = val


def gen_variants(args) -> list[tuple[dict[str, bool], bool]]:
    result = [
        (v, any(Configuration.satisfies(v, u) for u in (args.unsatisfiable or [])))
        for v in Configuration.all_variants()
        if not args.spec or any(Configuration.satisfies(v, s) for s in args.spec)
    ]
    assert result
    if args.single_spec and len(result) > 1:
        print("Multiple spec match but -1/--single-spec given, refine your parameters!")
        print("Specs that matched:")
        for v, _ in result:
            print("  " + Configuration.to_string(v))
        sys.exit(2)
    return result


def configure(
    depcfg: DependencyConfiguration, v: dict[str, bool], unsat: bool
) -> T.Optional[Configuration]:
    try:
        cfg = Configuration(depcfg, v)
    except Unsatisfiable as e:
        if not unsat:
            with colorize(FgColor.error):
                print(
                    f"## HPCToolkit {Configuration.to_string(v)} is unsatisfiable, missing {e.missing}",
                    flush=True,
                )
        return None
    else:
        if unsat:
            with colorize(FgColor.error):
                print(
                    f"## HPCToolkit {Configuration.to_string(v)} was incorrectly marked as unsatisfiable",
                    flush=True,
                )
    return cfg


def run_actions(
    variant: dict[str, bool], cfg: Configuration, args
) -> list[tuple[Action, ActionResult, float]]:
    srcdir = Path().absolute()
    logdir = None
    if args.logs_dir:
        logdir = args.logs_dir / args.log_format.format(Configuration.to_string(variant, ""))
        logdir.mkdir(parents=True)
        logdir = logdir.absolute()
    builddir = srcdir / "ci" / "build"
    installdir = srcdir / "ci" / "install"

    results = []
    try:
        for i, a in enumerate(args.action):
            print_header(f"[{i+1}/{len(args.action)}] {a.header(cfg)}")
            starttim = time.monotonic()
            r = a.run(cfg, builddir=builddir, srcdir=srcdir, installdir=installdir, logdir=logdir)
            endtim = time.monotonic()
            tim = endtim - starttim
            print(f"[{i+1}/{len(args.action)}] {a.name()} took {tim:.3f} seconds")
            results.append((a, r, tim))
            if not r.completed:
                break
    finally:
        if not args.keep:
            if builddir.exists():
                shutil.rmtree(builddir)
            if installdir.exists():
                shutil.rmtree(installdir)
    return results


def parse_stats(stdout):
    stats = {}
    for line in stdout.splitlines():
        parts = re.split(" {3,}|\t+", line)
        if len(parts) == 2:
            key, val = parts
            try:
                stats[key] = int(val)
            except ValueError:
                pass
    return stats


def print_ccache_stats():
    try:
        proc = subprocess.run(
            [shutil.which("ccache"), "--print-stats"],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            check=True,
        )
    except subprocess.CalledProcessError:
        # Older ccache doesn't support the machine-readable --print-stats, so just skip
        return

    stats = parse_stats(proc.stdout)
    d_hit, p_hit, miss = (
        stats["direct_cache_hit"],
        stats["preprocessed_cache_hit"],
        stats["cache_miss"],
    )
    total_hit = d_hit + p_hit
    total = total_hit + miss

    with section(f"Ccache efficacy: {total_hit/total*100:.3f}%", collapsed=True):
        print(f"Hit: {total_hit/total*100:.3f}% ({total_hit:d} of {total:d})")
        print(f"  Direct: {d_hit/total_hit*100:.3f}% ({d_hit:d} of {total_hit:d})")
        print(f"  Preprocessed: {p_hit/total_hit*100:.3f}% ({p_hit:d} of {total_hit:d})")
        print(f"Missed: {miss/total*100:.3f}% ({miss:d} of {total:d})")


def build(v, cfg, args) -> tuple[bool, dict]:
    if args.ccache_stats:
        subprocess.run(
            [shutil.which("ccache"), "--zero-stats"], stdout=subprocess.DEVNULL, check=True
        )

    ok = True
    with section(
        "## Build for HPCToolkit " + Configuration.to_string(v),
        color=FgColor.header,
        collapsed=True,
    ):
        results = run_actions(v, cfg, args)

    if any(not r.completed for a, r, t in results):
        with colorize(FgColor.error):
            failures = [a.name() for a, r, t in results if not r.completed]
            print(f" [\U0001f4a5] Build failed in {' '.join(failures)}", flush=True)
        ok = False
    elif any(not r.passed for a, r, t in results):
        with colorize(FgColor.error):
            print(" [\u274c] Build completed with errors", flush=True)
        ok = False
    elif any(not r.flawless for a, r, t in results):
        with colorize(FgColor.warning):
            print(" [\U0001f315] Build successful", flush=True)
    else:
        with colorize(FgColor.flawless):
            print(" [\u2714] Build successful with no detected flaws", flush=True)
    summarize_results((r for a, r, t in results), prefix="    - ")

    if args.ccache_stats:
        print_ccache_stats()

    return ok, {a: t for a, r, t in results}


def print_stats(times: list[float], prefix: str = ""):
    if len(times) == 1:
        print(f"{prefix}Time: {times[0]:.3f} seconds")
    else:
        print(
            f"{prefix}Time: {statistics.mean(times):.3f} +- {statistics.stdev(times):.1f} seconds"
        )
        qtls = statistics.quantiles(times, n=5)
        print(f"{prefix}      20% < {qtls[0]:.3f} s | {qtls[3]:.3f} s < 20%")


def main():
    args = post_parse(build_parser().parse_args())

    variants = gen_variants(args)

    depcfg = DependencyConfiguration()
    for src in args.sources:
        load_src(depcfg, src, args.spack_args)

    times: dict[Action, list[float]] = {}

    all_ok = True
    for v, unsat in variants:
        cfg = configure(depcfg, v, unsat)
        if cfg is None:
            if not unsat:
                all_ok = False
            continue
        if unsat:
            all_ok = False

        ok, this_times = build(v, cfg, args)
        all_ok = all_ok and ok

        for a, t in this_times.items():
            ts = times.get(a, [])
            ts.append(t)
            times[a] = ts

    if len(variants) > 1:
        with section("Performance statistics", collapsed=True):
            for i, a in enumerate(args.action):
                if a in times:
                    print(f"{i+1}/{len(args.action)}: {a.name()}")
                    print_stats(times[a], "  ")

    sys.exit(0 if all_ok else 1)
