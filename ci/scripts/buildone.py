#!/usr/bin/env python3

from pathlib import Path
import argparse
import shutil
import sys

sys.path.insert(0, Path(__file__).parent)

from common.configuration import DependencyConfiguration, Configuration, Unsatisfiable
from common.logs import colorize, FgColor, section, dump_file, print_header, print_results
from common import buildsys

parser = argparse.ArgumentParser(
    description="Build a single HPCToolkit and make sure it compiles.",
    epilog="""\
The output from this program is summarized to only include warnings or errors, and is wrapped in
collapsible sections to allow for easy viewing from the GitLab UI.
""",
)
parser.add_argument(
    "configs", nargs="+", type=Path, help="Argument configuration files (earlier overrides later)"
)
parser.add_argument("-j", "--jobs", default=16, type=int, help="Number of jobs to use for builds")
parser.add_argument(
    "-o",
    "--log-output",
    type=Path,
    metavar="LOGS",
    default="logs/",
    help="Directory to store output logs in (<LOGS>/<PREFIX><spec>)",
)
parser.add_argument(
    "-p",
    "--log-prefix",
    metavar="PREFIX",
    default="variant_",
    help="Prefix to append before logs sub-directories (<LOGS>/<PREFIX><spec>)",
)
parser.add_argument(
    "-s",
    "--spec",
    metavar="SPECS",
    type=Configuration.parse,
    default=[],
    help="Variant-spec (Spack +~ format) to build and check",
)
args = parser.parse_args()
del parser

depscfg = DependencyConfiguration.from_files(*args.configs)


class BuildStepFailure(Exception):
    "Failure in one of the build steps"

    def __init__(self, step):
        self.step = step


def build(**variant):
    "Attempt to build and run `make check` on the given variant. Return whether we were successful or not."
    # Setup the configuration for this run
    try:
        cfg = Configuration(depscfg, **variant)
    except Unsatisfiable as unsat:
        with colorize(FgColor.error):
            print(
                f"## HPCToolkit {Configuration.to_string(**variant)} is unsatisfiable, missing {unsat.missing}"
            )
        sys.stdout.flush()
        return False

    results = []
    try:
        with section(
            "## Build for HPCToolkit " + Configuration.to_string(**variant),
            color=FgColor.header,
            collapsed=True,
        ):
            sys.stdout.flush()
            # Set up the temporary directories
            srcdir = Path().absolute()
            logdir = args.log_output / (args.log_prefix + Configuration.to_string("", **variant))
            logdir.mkdir(parents=True)
            logdir = logdir.absolute()

            builddir = srcdir / "ci" / "build"
            installdir = Path("/opt/hpctoolkit")
            try:
                # Configure
                print_header("[1/4] ../configure ...")
                ok = buildsys.configure(srcdir, builddir, cfg, prefix=installdir, logdir=logdir)
                if (builddir / "config.log").exists():
                    shutil.copyfile(builddir / "config.log", logdir / "config.log")
                if not ok:
                    dump_file(logdir / "configure.log")
                    raise BuildStepFailure("configure")

                # Make
                print_header("[2/4] make")
                ok = buildsys.compile(builddir, cfg, jobs=args.jobs, logdir=logdir)
                dump_file(logdir / "compile.stderr.log")
                results.append(buildsys.CompileResult(logdir / "compile.stderr.log"))
                if not ok:
                    raise BuildStepFailure("make")

                # Make install
                print_header("[3/4] make install")
                ok = buildsys.install(builddir, cfg, logdir=logdir)
                dump_file(logdir / "install.stderr.log")
                if not ok:
                    raise BuildStepFailure("make install")

                # Validate install against the canonical expected manifest
                ok = cfg.manifest.check(installdir)
                if not ok.flawless:
                    print_header("[3/4] Differences from install manifest:")
                    ok.print_colored()
                results.append(ok)
                if not ok:
                    raise BuildStepFailure("install manifest check")

                # Make check
                print_header("[4/4] make check")
                ok = buildsys.test(builddir, cfg, logdir=logdir)
                dump_file(logdir / "test.stderr.log")
                if not ok:
                    raise BuildStepFailure("make check")

            finally:
                if builddir.exists():
                    shutil.rmtree(builddir)

    except BuildStepFailure as fail:
        # Failed build, give a short summary of what went wrong
        with colorize(FgColor.error):
            print(f" [\u274c] Build failed in {fail.step}")
            print_results(*results, prefix="     - ")
        sys.stdout.flush()
        return False

    # Successful build, give a short summary of how well it did
    if any(not r.flawless for r in results):
        with colorize(FgColor.warning):
            print(f" [\U0001f315] Build successful")
            print_results(*results, prefix="     - ")
    else:
        with colorize(FgColor.flawless):
            print(f" [\u2714] Build successful with no detected flaws")
    sys.stdout.flush()
    return True


sys.exit(0 if build(**args.spec) else 1)
