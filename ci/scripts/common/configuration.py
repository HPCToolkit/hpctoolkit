import collections
import collections.abc
import itertools
import os
import platform
import re
import shlex
import shutil
import typing as T

from .manifest import Manifest
from .util import flatten


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
        with open(fn, encoding="utf-8") as f:
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
                    envvars = {
                        "PWD": conf.parent.absolute().as_posix(),
                        "CC": os.environ.get("CC", None),
                        "CXX": os.environ.get("CXX", None),
                    }
                    for var, val in envvars.items():
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

    def __init__(self, depcfg: DependencyConfiguration, variant: T.Dict[str, bool]):
        """Derive the full Configuration from the given DependencyConfiguration and variant-keywords."""
        make = shutil.which("make")
        if make is None:
            raise RuntimeError("Unable to find make!")
        self.make: str = make

        self.manifest: Manifest = Manifest(mpi=variant["mpi"])

        fragments: T.List[str] = self.__class__._collect_fragments(depcfg, variant)

        # Parse the now-together fragments to derive the environment and configure args
        self.args: T.List[str] = []
        self.env: T.Any = collections.ChainMap({}, os.environ)
        for arg in flatten(fragments):
            m = re.fullmatch(r"ENV\{(\w+)\}=(.*)", arg)
            if m is None:
                self.args.append(arg)
            else:
                self.env[m.group(1)] = m.group(2)

    @staticmethod
    def _collect_fragments(
        depcfg: DependencyConfiguration, variant: T.Dict[str, bool]
    ) -> T.List[str]:
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

        if variant["papi"]:
            fragments.append(depcfg.get("--with-papi="))
        else:
            fragments.append(depcfg.get("--with-perfmon="))

        if variant["cuda"]:
            fragments.append(depcfg.get("--with-cuda="))

        if variant["level0"]:
            fragments.append(depcfg.get("--with-level0="))

        if variant["opencl"]:
            fragments.append(depcfg.get("--with-opencl="))

        # if False:  # TODO: GTPin
        #     fragments.extend(
        #         [
        #             depcfg.get("--with-gtpin="),
        #             depcfg.get("--with-igc="),
        #         ]
        #     )

        if variant["rocm"]:
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

        # if False:  # TODO: all-static (do we really want to support this?)
        #     fragments.append("--enable-all-static")

        fragments.extend([f"MPI{cc}=" for cc in ("CC", "F77")])
        if variant["mpi"]:
            fragments.append(depcfg.get("MPICXX="))
            fragments.append("--enable-force-hpcprof-mpi")
        else:
            fragments.append("MPICXX=")

        if variant["debug"]:
            fragments.append("--enable-develop")

        return fragments

    @classmethod
    def all_variants(cls):
        """Generate a list of all possible variants as dictionaries of variant-keywords"""

        def vbool(x, first=False):
            return [(x, first), (x, not first)]

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
        variant: T.Dict[str, bool],
        separator: str = " ",
    ) -> str:
        """Generate the string form for a variant set"""

        def vbool(n):
            return f"+{n}" if variant[n] else f"~{n}"

        return separator.join(
            [
                vbool("mpi"),
                vbool("debug"),
                vbool("papi"),
                vbool("opencl"),
                vbool("cuda"),
                vbool("rocm"),
                vbool("level0"),
            ]
        )

    @staticmethod
    def parse(arg: str) -> T.Dict[str, bool]:
        """Parse a variant-spec (vaugely Spack format) into a variant (dict)"""
        result = {}
        for wmatch in re.finditer(r"[+~\w]+", arg):
            word = wmatch.group(0)
            if word[0] not in "+~":
                raise ValueError("Variants must have a value indicator (+~): " + word)
            for match in re.finditer(r"([+~])(\w*)", word):
                value, variant = match.group(1), match.group(2)
                result[variant] = value == "+"
        for k in result:
            if k not in ("mpi", "debug", "papi", "opencl", "cuda", "rocm", "level0"):
                raise ValueError(f"Invalid variant name {k}")
        return result

    @staticmethod
    def satisfies(specific: T.Dict[str, bool], general: T.Dict[str, bool]) -> bool:
        """Test if the `specific` variant is a subset of a `general` variant"""
        for k, v in general.items():
            if k in specific and specific[k] != v:
                return False
        return True
