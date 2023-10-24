import collections
import collections.abc
import dataclasses
import itertools
import os
import re
import textwrap
from pathlib import Path

from elftools.elf.elffile import ELFFile  # type: ignore[import]


@dataclasses.dataclass(frozen=True)
class ManifestCheckFailure:
    """Failure marker for one condition imposed by the Manfifest."""

    subject: Path
    message: str

    def relative_to(self: "ManifestCheckFailure", root: Path) -> "ManifestCheckFailure":
        return self.__class__(subject=self.subject.relative_to(root), message=self.message)

    def __str__(self: "ManifestCheckFailure") -> str:
        return f"Failure on {self.subject}: {self.message}"

    @classmethod
    def multi(
        cls: type["ManifestCheckFailure"],
        failures: collections.abc.Collection["ManifestCheckFailure"],
    ) -> "ManifestCheckFailure":
        if not failures:
            raise ValueError(failures)
        if len(failures) == 1:
            return next(iter(failures))

        subject: Path = next(iter(failures)).subject
        bits: list[str] = ["multiple failures encountered:"]
        for failure in failures:
            if failure.subject != subject:
                raise ValueError(f"Invalid subject for {failure!r}, expected {subject!r}")
            message = textwrap.indent(failure.message, "  ")
            message = "-" + message[1:]
            bits.append(message)
        return cls(subject, "\n".join(bits))


@dataclasses.dataclass(frozen=True)
class ManifestCheckWarning:
    """Warning marker for files which the Manifest has no knowledge about."""

    subject: Path

    def relative_to(self: "ManifestCheckWarning", root: Path) -> "ManifestCheckWarning":
        return self.__class__(subject=self.subject.relative_to(root))

    def __str__(self: "ManifestCheckWarning") -> str:
        return f"Unexpected file: {self.subject}"

    @classmethod
    def multi(
        cls: type["ManifestCheckWarning"],
        warnings: collections.abc.Collection["ManifestCheckWarning"],
    ) -> "ManifestCheckWarning":
        if not warnings:
            raise ValueError(warnings)
        if len(warnings) == 1:
            return next(iter(warnings))

        subject: Path = next(iter(warnings)).subject
        for warning in warnings:
            if warning.subject != subject:
                raise ValueError(f"Invalid subject for {warning!r}, expected {subject!r}")
        return next(iter(warnings))


class ManifestCheckResults:
    """Results from a `Manifest.check` pass."""

    def __init__(self: "ManifestCheckResults") -> None:
        self._warnings: set[ManifestCheckWarning] = set()
        self._failures: set[ManifestCheckFailure] = set()

    def add_warnings(
        self: "ManifestCheckResults", warnings: collections.abc.Iterable[ManifestCheckWarning]
    ) -> None:
        warnings_by_subject: dict[Path, set[ManifestCheckWarning]] = collections.defaultdict(set)
        for warning in itertools.chain(self._warnings, warnings):
            warnings_by_subject[warning.subject].add(warning)
        self._warnings = {
            ManifestCheckWarning.multi(somewarnings)
            for somewarnings in warnings_by_subject.values()
        }

    @property
    def warnings(self: "ManifestCheckResults") -> collections.abc.Collection[ManifestCheckWarning]:
        return self._warnings

    def add_failures(
        self: "ManifestCheckResults", failures: collections.abc.Iterable[ManifestCheckFailure]
    ) -> None:
        failures_by_subject: dict[Path, set[ManifestCheckFailure]] = collections.defaultdict(set)
        for failure in itertools.chain(self._failures, failures):
            failures_by_subject[failure.subject].add(failure)
        self._failures = {
            ManifestCheckFailure.multi(somefailures)
            for somefailures in failures_by_subject.values()
        }

    @property
    def failures(self: "ManifestCheckResults") -> collections.abc.Collection[ManifestCheckFailure]:
        return self._failures


class _SymbolManifest:
    def __init__(self, symbols: frozenset[str], *, allow_extra: bool | re.Pattern = False):
        self.symbols = symbols
        self.allow_extra: re.Pattern | None = None
        if allow_extra:
            self.allow_extra = re.compile(r".*") if allow_extra is True else allow_extra

    def check(self, path: Path) -> ManifestCheckFailure | None:
        with open(path, "rb") as f, ELFFile(f) as file:
            got = {
                s.name
                for seg in file.iter_segments("PT_DYNAMIC")
                for s in seg.iter_symbols()
                if s["st_info"].type == "STT_FUNC"
                and s["st_info"].bind in ("STB_GLOBAL", "STB_WEAK")
                and s["st_value"] != 0
            }
            missing, unexpected = self.symbols - got, got - self.symbols
            if self.allow_extra:
                unexpected = {s for s in unexpected if not self.allow_extra.fullmatch(s)}

            if missing or unexpected:
                lines: list[str] = ["symbol table does not match manifest:"]
                if missing:
                    lines.append("The following symbols were expected but not found:")
                    for sym in missing:
                        lines.append(f"  {sym!r},")
                if unexpected:
                    lines.append("The following symbols were unexpected:")
                    for sym in unexpected:
                        lines.append(f"  {sym!r},")
                return ManifestCheckFailure(path, "\n".join(lines))

            return None


class _ManifestFile:
    def __init__(self, path: str | Path, *, symbols: _SymbolManifest | None = None):
        self.path = Path(path)
        self.symbols = symbols

    def claims(self, installdir: Path) -> set[Path]:
        return {installdir / self.path}

    def check(self, installdir: Path) -> set[ManifestCheckFailure]:
        target = installdir / self.path
        if not target.is_file():
            return {ManifestCheckFailure(target, "file was not found or is not a file")}
        if self.symbols is not None and (symerr := self.symbols.check(target)) is not None:
            return {symerr}
        return set()


class _ManifestLib(_ManifestFile):
    def __init__(self, path: str | Path, target, *aliases, symbols: _SymbolManifest | None = None):
        super().__init__(path)
        self.target = str(target)
        self.aliases = [str(a) for a in aliases]
        self.symbols = symbols

    def claims(self, installdir: Path) -> set[Path]:
        common = installdir / self.path
        result = {common.with_name(common.name + self.target)}
        for a in self.aliases:
            result.add(common.with_name(common.name + a))
        return result

    def check(self, installdir: Path) -> set[ManifestCheckFailure]:
        errors = set()

        target = installdir / self.path
        target = target.with_name(target.name + self.target)
        if not target.is_file():
            errors.add(ManifestCheckFailure(target, "file was not found"))
        elif target.is_symlink():
            errors.add(
                ManifestCheckFailure(target, f"should not be a symlink to {os.readlink(target)}")
            )
        elif self.symbols is not None and (symerr := self.symbols.check(target)) is not None:
            errors.add(symerr)

        for a in self.aliases:
            alias = installdir / self.path
            alias = alias.with_name(alias.name + a)
            if not alias.is_file():
                errors.add(ManifestCheckFailure(alias, "alias symlink was not found"))
                continue
            if not alias.is_symlink():
                errors.add(ManifestCheckFailure(alias, "alias was not a symlink"))
                continue

            targ = Path(os.readlink(alias))
            if len(targ.parts) > 1 or targ.name != target.name:
                errors.add(
                    ManifestCheckFailure(
                        alias, f"invalid alias symlink, must point to sibling {target.name}"
                    )
                )
                continue

        return errors


class Manifest:
    """Representation of an install manifest."""

    SYMBOLS_LIBHPCRUN = frozenset(
        {
            "__sysv_signal",
            "debug_flag_dump",
            "debug_flag_get",
            "debug_flag_init",
            "debug_flag_set",
            "hpctoolkit_sampling_is_active",
            "hpctoolkit_sampling_start",
            "hpctoolkit_sampling_start_",
            "hpctoolkit_sampling_start__",
            "hpctoolkit_sampling_stop",
            "hpctoolkit_sampling_stop_",
            "hpctoolkit_sampling_stop__",
            "messages_donothing",
            "messages_fini",
            "messages_init",
            "messages_logfile_create",
            "messages_logfile_fd",
            "monitor_at_main",
            "monitor_begin_process_exit",
            "monitor_fini_process",
            "monitor_fini_thread",
            "monitor_init_mpi",
            "monitor_init_process",
            "monitor_init_thread",
            "monitor_mpi_pre_init",
            "monitor_post_fork",
            "monitor_pre_fork",
            "monitor_reset_stacksize",
            "monitor_start_main_init",
            "monitor_thread_post_create",
            "monitor_thread_pre_create",
            "ompt_start_tool",
            "poll",
            "ppoll",
            "pselect",
            "select",
        }
    )
    SYMBOLS_LIBHPCRUN_ROCM = frozenset(
        {
            "OnUnloadTool",
            "OnLoadToolProp",
        }
    )
    SYMBOLS_LIBHPCRUN_DLMOPEN = frozenset(
        {
            "dlmopen",
        }
    )
    SYMBOLS_LIBHPCRUN_FAKE_AUDIT = frozenset(
        {
            "dlclose",
            "dlmopen",
            "dlopen",
            "hpcrun_init_fake_auditor",
        }
    )
    SYMBOLS_LIBHPCRUN_GA = frozenset(
        {
            "pnga_acc",
            "pnga_brdcst",
            "pnga_create",
            "pnga_create_handle",
            "pnga_get",
            "pnga_gop",
            "pnga_nbacc",
            "pnga_nbget",
            "pnga_nbput",
            "pnga_nbwait",
            "pnga_put",
            "pnga_sync",
        }
    )
    SYMBOLS_LIBHPCRUN_GPROF = frozenset(
        {
            "__monstartup",
            "_mcleanup",
            "_mcount",
            "mcount",
        }
    )
    SYMBOLS_LIBHPCRUN_IO = frozenset(
        {
            "fread",
            "fwrite",
            "read",
            "write",
        }
    )
    SYMBOLS_LIBHPCRUN_MEMLEAK = frozenset(
        {
            "calloc",
            "free",
            "malloc",
            "memalign",
            "posix_memalign",
            "realloc",
            "valloc",
        }
    )
    SYMBOLS_LIBHPCRUN_OPENCL = frozenset(
        {
            "clBuildProgram",
            "clCreateBuffer",
            "clCreateCommandQueue",
            "clCreateCommandQueueWithProperties",
            "clCreateContext",
            "clEnqueueMapBuffer",
            "clEnqueueNDRangeKernel",
            "clEnqueueReadBuffer",
            "clEnqueueTask",
            "clEnqueueWriteBuffer",
            "clFinish",
            "clReleaseCommandQueue",
            "clReleaseKernel",
            "clReleaseMemObject",
            "clSetKernelArg",
            "clWaitForEvents",
        }
    )
    SYMBOLS_LIBHPCRUN_PTHREAD = frozenset(
        {
            "override_lookup",
            "override_lookupv",
            "pthread_cond_broadcast",
            "pthread_cond_signal",
            "pthread_cond_timedwait",
            "pthread_cond_wait",
            "pthread_mutex_lock",
            "pthread_mutex_timedlock",
            "pthread_mutex_unlock",
            "pthread_spin_lock",
            "pthread_spin_unlock",
            "sched_yield",
            "sem_post",
            "sem_timedwait",
            "sem_wait",
            "tbb_stats",
        }
    )

    def __init__(self, *, mpi: bool, rocm: bool):
        """Given a set of variant-keywords, determine the install manifest as a list of ManifestFiles."""
        hpcrun_symbols = self.SYMBOLS_LIBHPCRUN
        if rocm:
            hpcrun_symbols |= self.SYMBOLS_LIBHPCRUN_ROCM
        hpcrun_extra = re.compile(r"^hpcrun_.+$")

        self.files = [
            _ManifestFile("bin/hpclink"),
            _ManifestFile("bin/hpcprof"),
            _ManifestFile("bin/hpcrun"),
            _ManifestFile("bin/hpcstruct"),
            _ManifestFile("include/hpctoolkit.h"),
            _ManifestFile("lib/hpctoolkit/hash-file"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_audit.a"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_audit.la"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_dlmopen.a"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_dlmopen.la"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_fake_audit.a"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_fake_audit.la"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_ga.a"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_ga_wrap.a"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_ga.la"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_gprof.a"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_gprof.la"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_gprof_wrap.a"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_io.a"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_io.la"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_io_wrap.a"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_memleak.a"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_memleak.la"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_memleak_wrap.a"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_opencl.a"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_opencl.la"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_pthread.a"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_pthread.la"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_pthread_wrap.a"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_wrap.a"),
            _ManifestFile("lib/hpctoolkit/libhpcrun.o"),
            _ManifestFile(
                "lib/hpctoolkit/libhpcrun.so",
                symbols=_SymbolManifest(hpcrun_symbols, allow_extra=hpcrun_extra),
            ),
            # XXX: Why is there no libhpcrun.so.0.0.0 and libhpcrun.la?
            _ManifestFile("lib/hpctoolkit/libhpctoolkit.a"),
            _ManifestFile("lib/hpctoolkit/libhpctoolkit.la"),
            _ManifestFile("lib/hpctoolkit/plugins/ga"),
            _ManifestFile("lib/hpctoolkit/plugins/io"),
            _ManifestFile("lib/hpctoolkit/plugins/memleak"),
            _ManifestFile("lib/hpctoolkit/plugins/pthread"),
            # XXX: Why is there no gprof?
            _ManifestFile("libexec/hpctoolkit/config.guess"),
            _ManifestFile("libexec/hpctoolkit/dotgraph-bin"),
            _ManifestFile("libexec/hpctoolkit/dotgraph"),
            _ManifestFile("libexec/hpctoolkit/hpcfnbounds"),
            _ManifestFile("libexec/hpctoolkit/hpcguess"),
            _ManifestFile("libexec/hpctoolkit/hpclog"),
            _ManifestFile("libexec/hpctoolkit/hpcplatform"),
            _ManifestFile("libexec/hpctoolkit/hpcproftt-bin"),
            _ManifestFile("libexec/hpctoolkit/hpcproftt"),
            _ManifestFile("libexec/hpctoolkit/hpcproflm"),
            _ManifestFile("libexec/hpctoolkit/hpcstruct-bin"),
            _ManifestFile("libexec/hpctoolkit/hpcsummary"),
            _ManifestFile("libexec/hpctoolkit/hpctracedump"),
            _ManifestFile("share/doc/hpctoolkit/FORMATS.md"),
            _ManifestFile("share/doc/hpctoolkit/METRICS.yaml"),
            _ManifestFile("share/doc/hpctoolkit/LICENSE"),
            _ManifestFile("share/doc/hpctoolkit/man/hpclink.html"),
            _ManifestFile("share/doc/hpctoolkit/man/hpcprof.html"),
            _ManifestFile("share/doc/hpctoolkit/man/hpcproftt.html"),
            _ManifestFile("share/doc/hpctoolkit/man/hpcrun.html"),
            _ManifestFile("share/doc/hpctoolkit/man/hpcstruct.html"),
            _ManifestFile("share/doc/hpctoolkit/man/hpctoolkit.html"),
            _ManifestFile("share/doc/hpctoolkit/man/hpcviewer.html"),
            _ManifestFile("share/doc/hpctoolkit/manual/HPCToolkit-users-manual.pdf"),
            _ManifestFile("share/doc/hpctoolkit/README.Acknowledgments"),
            _ManifestFile("share/doc/hpctoolkit/README.Install"),
            _ManifestFile("share/doc/hpctoolkit/README.md"),
            _ManifestFile("share/doc/hpctoolkit/README.ReleaseNotes"),
            _ManifestFile("share/hpctoolkit/dtd/hpc-experiment.dtd"),
            _ManifestFile("share/hpctoolkit/dtd/hpc-structure.dtd"),
            _ManifestFile("share/hpctoolkit/dtd/hpcprof-config.dtd"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isoamsa.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isoamsb.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isoamsc.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isoamsn.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isoamso.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isoamsr.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isobox.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isocyr1.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isocyr2.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isodia.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isogrk3.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isolat1.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isolat2.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isomfrk.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isomopf.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isomscr.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isonum.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isopub.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/isotech.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/mathml.dtd"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/mmlalias.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/mmlextra.ent"),
            _ManifestFile("share/hpctoolkit/dtd/mathml/xhtml1-transitional-mathml.dtd"),
            _ManifestFile("share/man/man1/hpclink.1hpctoolkit"),
            _ManifestFile("share/man/man1/hpcprof.1hpctoolkit"),
            _ManifestFile("share/man/man1/hpcproftt.1hpctoolkit"),
            _ManifestFile("share/man/man1/hpcrun.1hpctoolkit"),
            _ManifestFile("share/man/man1/hpcstruct.1hpctoolkit"),
            _ManifestFile("share/man/man1/hpctoolkit.1hpctoolkit"),
            _ManifestFile("share/man/man1/hpcviewer.1hpctoolkit"),
            _ManifestLib("lib/hpctoolkit/libhpcrun_audit.so", ".0.0.0", ".0", ""),
            _ManifestLib(
                "lib/hpctoolkit/libhpcrun_dlmopen.so",
                ".0.0.0",
                ".0",
                "",
                symbols=_SymbolManifest(self.SYMBOLS_LIBHPCRUN_DLMOPEN),
            ),
            _ManifestLib(
                "lib/hpctoolkit/libhpcrun_fake_audit.so",
                ".0.0.0",
                ".0",
                "",
                symbols=_SymbolManifest(self.SYMBOLS_LIBHPCRUN_FAKE_AUDIT),
            ),
            _ManifestLib(
                "lib/hpctoolkit/libhpcrun_ga.so",
                ".0.0.0",
                ".0",
                "",
                symbols=_SymbolManifest(self.SYMBOLS_LIBHPCRUN_GA),
            ),
            _ManifestLib(
                "lib/hpctoolkit/libhpcrun_gprof.so",
                ".0.0.0",
                ".0",
                "",
                symbols=_SymbolManifest(self.SYMBOLS_LIBHPCRUN_GPROF),
            ),
            _ManifestLib(
                "lib/hpctoolkit/libhpcrun_io.so",
                ".0.0.0",
                ".0",
                "",
                symbols=_SymbolManifest(self.SYMBOLS_LIBHPCRUN_IO),
            ),
            _ManifestLib(
                "lib/hpctoolkit/libhpcrun_memleak.so",
                ".0.0.0",
                ".0",
                "",
                symbols=_SymbolManifest(self.SYMBOLS_LIBHPCRUN_MEMLEAK),
            ),
            _ManifestLib(
                "lib/hpctoolkit/libhpcrun_opencl.so",
                ".0.0.0",
                ".0",
                "",
                symbols=_SymbolManifest(self.SYMBOLS_LIBHPCRUN_OPENCL),
            ),
            _ManifestLib(
                "lib/hpctoolkit/libhpcrun_pthread.so",
                ".0.0.0",
                ".0",
                "",
                symbols=_SymbolManifest(self.SYMBOLS_LIBHPCRUN_PTHREAD),
            ),
            _ManifestLib("lib/hpctoolkit/libhpctoolkit.so", ".0.0.0", ".0", ""),
        ]

        if mpi:
            self.files += [
                _ManifestFile("bin/hpcprof-mpi"),
            ]

    def check(self, installdir: Path) -> ManifestCheckResults:
        """Scan an install directory and compare against the expected manifest.

        Returns a ManifestCheckResults object.
        """
        results = ManifestCheckResults()

        # Perform all the sub-checks and collect together the list of claimed files
        claimed = set()
        for f in self.files:
            claimed |= f.claims(installdir)
            results.add_failures(f.check(installdir))

        # Scan for any files that were not claimed and make them warnings
        for root, _, files in os.walk(installdir):
            for filename in files:
                path = Path(root) / filename
                assert path.is_relative_to(installdir)
                if path not in claimed:
                    results.add_warnings([ManifestCheckWarning(path)])

        return results
