# SPDX-FileCopyrightText: 2023-2024 Rice University
#
# SPDX-License-Identifier: BSD-3-Clause

import collections
import functools
import os
import struct
import types
import typing
from pathlib import Path

from .errors import PredictableFailureError

if typing.TYPE_CHECKING:
    import collections.abc


class Measurements:
    def __init__(self, basedir):
        self.basedir = Path(basedir)

    _log_suffix = ".log"
    _profile_suffix = ".hpcrun"
    _trace_suffix = ".hpctrace"
    _suffixes = (_log_suffix, _profile_suffix, _trace_suffix)

    @functools.cached_property
    def thread_stems(self):
        return {x.stem for x in self.basedir.iterdir() if x.suffix in self._suffixes}

    def _get_file_path(self, suffixattr, stem):
        result = self.basedir / (stem + getattr(self.__class__, suffixattr))
        return result if result.is_file() else None

    logfile = functools.partialmethod(_get_file_path, "_log_suffix")
    profile = functools.partialmethod(_get_file_path, "_profile_suffix")
    tracefile = functools.partialmethod(_get_file_path, "_trace_suffix")

    def __str__(self):
        return f"{self.__class__.__name__}({self.basedir}, {len(self.thread_stems)} threads)"

    _HPCRUN_STRUCT_FOOTER_END = struct.Struct(">QQ")
    _HPCRUN_STRUCT_FOOTER = struct.Struct(">" + "QQ" * 6)
    _HPCRUN_STRUCT_TUPLE_START = struct.Struct(">H")
    _HPCRUN_STRUCT_TUPLE_ELEM = struct.Struct(">HQQ")
    _HPCRUN_KIND_CODES = types.MappingProxyType(
        {
            0: "SUMMARY",
            1: "NODE",
            2: "RANK",
            3: "THREAD",
            4: "GPUDEVICE",
            5: "GPUCONTEXT",
            6: "GPUSTREAM",
            7: "CORE",
        }
    )
    _HPCRUN_INTERPRET_CODES = types.MappingProxyType(
        {
            0: "both",
            1: "l-local",
            2: "l-global",
            3: "logical",
        }
    )

    def parse_idtuple(self, stem) -> str:
        filename = self.profile(stem)
        if not filename:
            raise ValueError(f"No profile with stem: {stem!r}")
        with open(filename, "rb") as pf:

            def read(numbytes: int) -> bytes:
                result = pf.read(numbytes)
                while len(result) < numbytes:
                    seg = pf.read(numbytes - len(result))
                    if not seg:
                        raise RuntimeError("Unexpected EOF")
                    result += seg
                return result

            def read_unpack(strct):
                return strct.unpack(read(strct.size))

            assert self._HPCRUN_STRUCT_FOOTER_END.size == 16
            pf.seek(-16, os.SEEK_END)
            footer_offset, magic = read_unpack(self._HPCRUN_STRUCT_FOOTER_END)
            if magic != 0x48504352554E736D:
                raise ValueError(f"Invalid magic bytes encountered: 0x{magic:016x}")

            pf.seek(footer_offset, os.SEEK_SET)
            sparse_start = read_unpack(self._HPCRUN_STRUCT_FOOTER)[10]

            pf.seek(sparse_start, os.SEEK_SET)
            (num_elems,) = read_unpack(self._HPCRUN_STRUCT_TUPLE_START)
            bits = []
            for _ in range(num_elems):
                kind_interp, pidx, lidx = read_unpack(self._HPCRUN_STRUCT_TUPLE_ELEM)
                kind, interpret = kind_interp & 0x3FFF, (kind_interp >> 14) & 0x3
                kind_name = self._HPCRUN_KIND_CODES[kind]
                interpret_name = self._HPCRUN_INTERPRET_CODES[interpret]
                bits.append(f"{kind_name} {lidx:x}/{pidx:x}:{interpret_name}")
            return " ".join(bits)

    def check_standard(
        self,
        *,
        procs: int = 1,
        threads_per_proc: typing.Union[int, "collections.abc.Collection[int]"] = 1,
        traces: bool = False,
    ):
        threads = [
            procs * tpp
            for tpp in (
                threads_per_proc if not isinstance(threads_per_proc, int) else [threads_per_proc]
            )
        ]
        for trial in threads:
            if len(self.thread_stems) == trial:
                break
        else:
            str_threads = " or ".join([f"{t:d}" for t in threads])
            raise PredictableFailureError(
                f"Expected exactly {str_threads} threads, got {len(self.thread_stems)}"
            )

        logs_found = 0
        for t in self.thread_stems:
            logs_found += 1 if self.logfile(t) else 0
            if not self.profile(t):
                raise PredictableFailureError(f"Expected a profile for thread stem {t}")
            if self.tracefile(t):
                if not traces:
                    raise PredictableFailureError(f"Did not expect a trace for thread stem {t}")
            elif traces:
                raise PredictableFailureError(f"Expected a trace for thread stem {t}")

        if logs_found != procs:
            raise PredictableFailureError(f"Expected exactly {procs} log file, got {logs_found}")


class Database:
    def __init__(self, basedir):
        self.basedir = Path(basedir)

    def __str__(self):
        return f"{self.__class__.__name__}({self.basedir})"

    def check_standard(self, *, tracedb: bool = False):
        meta_db = self.basedir / "meta.db"
        profile_db = self.basedir / "profile.db"
        cct_db = self.basedir / "cct.db"
        trace_db = self.basedir / "trace.db"
        formats_md = self.basedir / "FORMATS.md"
        metrics_yaml = self.basedir / "metrics" / "METRICS.yaml.ex"
        metrics_default = self.basedir / "metrics" / "default.yaml"

        # Check for standard files that should always be there
        for fn in (meta_db, profile_db, cct_db, formats_md, metrics_yaml, metrics_default):
            if not fn.is_file():
                raise PredictableFailureError(f"Expected {fn.name} in database!")

        # trace.db should only be present if traces were enabled
        if trace_db.is_file():
            if not tracedb:
                raise PredictableFailureError("trace.db present in database but not expected!")
        elif tracedb:
            raise PredictableFailureError("Expected trace.db in database!")
