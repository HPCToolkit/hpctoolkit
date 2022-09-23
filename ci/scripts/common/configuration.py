import re
import platform
import os
import shlex
import shutil
import itertools
from pathlib import Path
import collections

from .util import flatten
from .manifest import Manifest


class Unsatisfiable(Exception):
    "Exception raised when the given variant is unsatisfiable"

    def __init__(self, missing):
        super().__init__(f"missing definition for argument {missing}")
        self.missing = missing


class Impossible(Exception):
    "Exception raised when the given variant is impossible"

    def __init__(self, a, b):
        super().__init__(f'conflict between "{a}" and "{b}"')
        self.a, self.b = a, b


class DependencyConfiguration:
    """State needed to derive configure-time arguments, based on simple configuration files"""

    def __init__(self):
        self.configs = []

    def load(self, fn):
        with open(fn, "r") as f:
            for line in f:
                self.configs.append((fn, line))

    @classmethod
    def from_files(cls, *filenames):
        result = cls()
        for fn in filenames:
            result.load(fn)
        return result

    def get(self, argument):
        "Fetch the full form of the given argument, by searching the configs"
        argument = argument.lstrip()
        for conf, line in self.configs:
            if line.startswith(argument):
                if argument[-1].isspace():
                    line = line[len(argument) :]
                line = line.strip()

                result = []
                for word in shlex.split(line):
                    vars = {
                        "PWD": conf.parent.absolute().as_posix(),
                        "CC": os.environ.get("CC", None),
                        "CXX": os.environ.get("CXX", None),
                    }
                    for var, val in vars.items():
                        var = "${" + var + "}"
                        if var not in word:
                            continue
                        if val is None:
                            raise Unsatisfiable(var)
                        word = word.replace(var, val)
                    result.append(word)
                return result
        raise Unsatisfiable(argument)


class Configuration:
    """Representation of a possible build configuration of HPCToolkit"""

    def __init__(
        self,
        depcfg: DependencyConfiguration,
        *,
        mpi: bool,
        debug: bool,
        papi: bool,
        opencl: bool,
        cuda: bool,
        rocm: bool,
        level0: bool,
    ):
        """Derive the full Configuration from the given DependencyConfiguration and variant-keywords."""
        self.make = shutil.which("make")
        if not self.make:
            raise RuntimeError(f"Unable to find make!")

        self.manifest = Manifest(mpi=mpi)

        fragments = [
            depcfg.get("--with-boost="),
            depcfg.get("--with-bzip="),
            depcfg.get("--with-dyninst="),
            depcfg.get("--with-elfutils="),
            depcfg.get("--with-tbb="),
            depcfg.get("--with-libmonitor="),
            depcfg.get("--with-libunwind="),
            depcfg.get("--with-xerces="),
            depcfg.get("--with-lzma="),
            depcfg.get("--with-zlib="),
            depcfg.get("--with-libiberty="),
            depcfg.get("--with-memkind="),
            depcfg.get("--with-yaml-cpp="),
        ]

        if platform.machine() == "x86_64":
            fragments.append(depcfg.get("--with-xed="))

        if papi:
            fragments.append(depcfg.get("--with-papi="))
        else:
            fragments.append(depcfg.get("--with-perfmon="))

        if cuda:
            fragments.append(depcfg.get("--with-cuda="))

        if level0:
            fragments.append(depcfg.get("--with-level0="))

        if opencl:
            fragments.append(depcfg.get("--with-opencl="))

        if False:  # TODO: GTPin
            fragments.extend(
                [
                    depcfg.get("--with-gtpin="),
                    depcfg.get("--with-igc="),
                ]
            )

        if rocm:
            try:
                fragments.append(depcfg.get("--with-rocm="))
            except Unsatisfiable:
                # Try the split-form arguments instead
                fragments.extend(
                    [
                        depcfg.get("--with-rocm-hip="),
                        depcfg.get("--with-rocm-hsa="),
                        depcfg.get("--with-rocm-tracer="),
                        depcfg.get("--with-rocm-profiler="),
                    ]
                )

        if False:  # TODO: all-static (do we really want to support this?)
            fragments.append("--enable-all-static")

        fragments.extend([f"MPI{cc}=" for cc in ("CC", "F77")])
        if mpi:
            fragments.append(depcfg.get("MPICXX="))
            fragments.append("--enable-force-hpcprof-mpi")
        else:
            fragments.append("MPICXX=")

        if debug:
            fragments.append("--enable-develop")

        # Parse the now-togther fragments to derive the environment and configure args
        self.args = []
        self.env = collections.ChainMap({}, os.environ)
        for arg in flatten(fragments):
            m = re.fullmatch(r"ENV\{(\w+)\}=(.*)", arg)
            if m is None:
                self.args.append(arg)
            else:
                self.env[m.group(1)] = m.group(2)

    @classmethod
    def all_variants(cls):
        """Generate a list of all possible variants as dictionaries of variant-keywords"""
        vbool = lambda x, first=False: [(x, first), (x, not first)]
        return map(
            dict,
            itertools.product(
                *reversed(
                    [
                        vbool("mpi"),
                        vbool("debug", True),
                        vbool("papi", True),
                        vbool("opencl"),
                        vbool("cuda"),
                        vbool("rocm"),
                        vbool("level0"),
                    ]
                )
            ),
        )

    @staticmethod
    def to_string(
        separator: str = " ",
        *,
        mpi: bool,
        debug: bool,
        papi: bool,
        opencl: bool,
        cuda: bool,
        rocm: bool,
        level0: bool,
    ):
        """Generate the string form for a series of variant-keywords"""
        vbool = lambda x, n: f"+{n}" if x else f"~{n}"
        return separator.join(
            [
                vbool(mpi, "mpi"),
                vbool(debug, "debug"),
                vbool(papi, "papi"),
                vbool(opencl, "opencl"),
                vbool(cuda, "cuda"),
                vbool(rocm, "rocm"),
                vbool(level0, "level0"),
            ]
        )

    @staticmethod
    def parse(arg: str):
        """Parse a variant-spec (vaugely Spack format) into a dictionary of variant-keywords"""
        result = {}
        for word in re.finditer(r"[+~\w]+", arg):
            word = word.group(0)
            if word[0] not in "+~":
                raise ValueError("Variants must have a value indicator (+~): " + word)
            for match in re.finditer(r"([+~])(\w*)", word):
                value, variant = match.group(1), match.group(2)
                result[variant] = value == "+"
        for k in result.keys():
            if k not in ("mpi", "debug", "papi", "opencl", "cuda", "rocm", "level0"):
                raise ValueError(f"Invalid variant name {k}")
        return result

    @staticmethod
    def satisfies(specific, general):
        """Test if the `specific` variant is a subset of a `general` variant"""
        for k, v in general.items():
            if k in specific and specific[k] != v:
                return False
        return True
