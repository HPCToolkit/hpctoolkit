#!/usr/bin/env python3

import argparse
import shutil
import sys
from pathlib import Path

from common import buildsys
from common.configuration import Configuration, DependencyConfiguration, Unsatisfiable
from common.logs import FgColor, colorize, dump_file, print_header, print_results, section

parser = argparse.ArgumentParser(
    description="Build combinatorically many HPCToolkits and make sure they compile.",
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
    action="append",
    type=Configuration.parse,
    help="Variant-specs (Spack +~ format) attempted builds need to satisfy. If repeated, builds need only satisfy one",
)
parser.add_argument(
    "-i",
    "--ignore",
    metavar="VARIANTS",
    action="append",
    type=Configuration.parse,
    default=[],
    help="Variant-spec (Spack +~ format) for which unsatisfiability errors should become warnings",
)
args = parser.parse_args()
del parser

depscfg = DependencyConfiguration.from_files(*args.configs)


class BuildStepFailure(Exception):
    "Failure in one of the build steps"

    def __init__(self, step):
        self.step = step


def configure(variant):
    try:
        return Configuration(depscfg, variant)
    except Unsatisfiable as unsat:
        if any(Configuration.satisfies(variant, ig) for ig in args.ignore):
            # This one can be unsatisfiable, it's marked as ignored.
            with colorize(FgColor.info):
                print(f"## HPCToolkit {Configuration.to_string(variant)} skipped")
            sys.stdout.flush()
            return True

        # Otherwise, it's an error
        with colorize(FgColor.error):
            print(
                f"## HPCToolkit {Configuration.to_string(variant)} is unsatisfiable, missing {unsat.missing}"
            )
        sys.stdout.flush()
        return False


def build(variant):
    "Attempt to build the given variant. Return whether we were successful or not."
    cfg = configure(variant)
    if not isinstance(cfg, Configuration):
        return cfg

    results = []
    try:
        with section(
            "## Build for HPCToolkit " + Configuration.to_string(variant),
            color=FgColor.header,
            collapsed=True,
        ):
            sys.stdout.flush()
            # Set up the temporary directories
            srcdir = Path().absolute()
            logdir = args.log_output / (args.log_prefix + Configuration.to_string(variant, ""))
            logdir.mkdir(parents=True)
            logdir = logdir.absolute()

            builddir = srcdir / "ci" / "build"
            installdir = srcdir / "ci" / "install"
            try:
                # Configure
                print_header("[1/3] ../configure ...")
                ok = buildsys.configure(srcdir, builddir, cfg, prefix=installdir, logdir=logdir)
                if not ok:
                    dump_file(logdir / "configure.log")
                    raise BuildStepFailure("configure")

                # Make
                print_header("[2/3] make")
                ok = buildsys.build(builddir, cfg, jobs=args.jobs, logdir=logdir)
                dump_file(logdir / "build.stderr.log")
                results.append(buildsys.BuildResult(logdir / "build.stderr.log"))
                if not ok:
                    raise BuildStepFailure("make")

                # Make install
                print_header("[3/3] make install")
                ok = buildsys.install(builddir, cfg, logdir=logdir)
                dump_file(logdir / "install.stderr.log")
                if not ok:
                    raise BuildStepFailure("make install")

                # Validate install against the canonical expected manifest
                ok = cfg.manifest.check(installdir)
                if not ok.flawless:
                    print_header("[3/3] Differences from install manifest:")
                    ok.print_colored()
                results.append(ok)
                if not ok:
                    raise BuildStepFailure("install manifest check")

            finally:
                if builddir.exists():
                    shutil.rmtree(builddir)
                if installdir.exists():
                    shutil.rmtree(installdir)

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
            print(" [\U0001f315] Build successful")
            print_results(*results, prefix="     - ")
    else:
        with colorize(FgColor.flawless):
            print(" [\u2714] Build successful with no detected flaws")
    sys.stdout.flush()
    return True


exitcode = 0
for variant in Configuration.all_variants():
    if not args.spec or any(Configuration.satisfies(variant, s) for s in args.spec):
        if not build(variant):
            exitcode = 1
sys.exit(exitcode)
