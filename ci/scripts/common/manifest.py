from pathlib import Path
import re
import os
import textwrap

from .logs import AbstractStatusResult, colorize, FgColor


class _ManifestFile:
    def __init__(self, path):
        self.path = Path(path)

    def check(self, installdir):
        if (Path(installdir) / self.path).is_file():
            return {self.path}, set()
        return set(), {self.path}


class _ManifestLib(_ManifestFile):
    def __init__(self, path, target, *aliases, strict=True):
        super().__init__(path)
        self.target = str(target)
        self.aliases = [str(a) for a in aliases]

    def check(self, installdir):
        installdir = Path(installdir)
        found, missing = set(), set()

        target = installdir / self.path
        target = target.with_name(target.name + self.target)
        if not target.is_file():
            missing.add(target.relative_to(installdir))
        if target.is_symlink():
            missing.add(
                (target.relative_to(installdir), f"Unexpected symlink to {os.readlink(target)}")
            )

        for a in self.aliases:
            alias = installdir / self.path
            alias = alias.with_name(alias.name + a)
            if not alias.is_file():
                missing.add(alias.relative_to(installdir))
                continue
            if not alias.is_absolute():
                missing.add((alias.relative_to(installdir), f"Not a symlink"))
                continue

            targ = Path(os.readlink(alias))
            if len(targ.parts) > 1:
                missing.add(
                    (alias.relative_to(installdir), f"Invalid symlink, must point to sibling file")
                )
                continue
            if targ.name != target.name:
                missing.add(
                    (alias.relative_to(installdir), f"Invalid symlink, must point to {target.name}")
                )
                continue

            found.add(alias.relative_to(installdir))

        return found, missing


class _ManifestExtLib(_ManifestFile):
    def __init__(self, path, main_suffix, *suffixes, strict=True):
        super().__init__(path)
        self.main_suffix = str(main_suffix)
        self.suffixes = [str(s) for s in suffixes]

    def check(self, installdir):
        installdir = Path(installdir)
        common = installdir / self.path
        found = set()

        main_path = common.with_name(common.name + self.main_suffix)
        if not main_path.is_file():
            return set(), {main_path.relative_to(installdir)}
        found.add(main_path.relative_to(installdir))

        for path in common.parent.iterdir():
            if path.name.startswith(common.name) and path != main_path:
                name = path.name[len(common.name) :]
                if any(re.match(s, name) for s in self.suffixes):
                    found.add(path.relative_to(installdir))

        return found, set()


class ManifestResult(AbstractStatusResult):
    """Result of Manifest.check(...)"""

    def __init__(self):
        self.warnings = []
        self.n_unexpected = 0
        self.errors = []
        self.n_uninstalled = 0

    def __bool__(self):
        return not self.errors and self.n_uninstalled == 0

    @property
    def flawless(self):
        return self and not self.warnings and self.n_unexpected == 0

    def summary(self):
        if self.n_uninstalled > 0:
            return f"{self.n_uninstalled:d} uninstalled files + {self.n_unexpected:d} unexpected installed files"
        if self.n_unexpected > 0:
            return f"{self.n_unexpected:d} unexpected installed files"
        return ""

    def print_colored(self):
        with colorize(FgColor.warning):
            for hunk in self.warnings:
                print(hunk)
        with colorize(FgColor.error):
            for hunk in self.errors:
                print(hunk)


class Manifest:
    """Representation of an install manifest"""

    def __init__(self, *, mpi: bool):
        """Given a set of variant-keywords, determine the install manifest as a list of ManifestFiles"""
        so = lambda n: r"\.so" + r"\.\d+" * n.__index__()
        self.files = [
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_atomic-mt", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_atomic", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_chrono", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_date_time-mt", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_date_time", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_filesystem-mt", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_filesystem", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_graph-mt", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_graph", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_regex-mt", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_regex", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_system-mt", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_system", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_thread-mt", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_thread", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_timer-mt", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libboost_timer", ".so", so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libbz2", ".so", so(1), so(2), so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libcommon", ".so", so(2), so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libdw", ".so", so(1), r"-\d+\.\d+\.so"),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libdynDwarf", ".so", so(2), so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libdynElf", ".so", so(1), r"-\d+\.\d+\.so"),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libelf", ".so", so(1), r"-\d+\.\d+\.so"),
            _ManifestExtLib(
                "lib/hpctoolkit/ext-libs/libinstructionAPI", ".so", so(1), r"-\d+\.\d+\.so"
            ),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/liblzma", ".so", so(1), so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libmonitor_wrap", ".a"),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libmonitor", ".so", ".so.0", ".so.0.0.0"),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libparseAPI", ".so", so(2), so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libpfm", ".so", so(1), so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libsymtabAPI", ".so", so(2), so(3)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libtbb", ".so", so(1)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libtbbmalloc_proxy", ".so", so(1)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libtbbmalloc", ".so", so(1)),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libxerces-c", ".a"),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libxerces-c", ".so", r"-\d+.\d+\.so"),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libz", ".a"),
            _ManifestExtLib("lib/hpctoolkit/ext-libs/libz", ".so", so(1), so(3)),
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
            _ManifestFile("lib/hpctoolkit/libhpcrun_ga.la"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_gprof.a"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_gprof.la"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_io.a"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_io.la"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_memleak.a"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_memleak.la"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_pthread.a"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_pthread.la"),
            _ManifestFile("lib/hpctoolkit/libhpcrun_wrap.a"),
            _ManifestFile("lib/hpctoolkit/libhpcrun.o"),
            _ManifestFile("lib/hpctoolkit/libhpcrun.so"),
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
            _ManifestFile("libexec/hpctoolkit/hpcsummary"),
            _ManifestFile("libexec/hpctoolkit/hpctracedump"),
            _ManifestFile("libexec/hpctoolkit/renamestruct.sh"),
            _ManifestFile("share/doc/hpctoolkit/documentation.html"),
            _ManifestFile("share/doc/hpctoolkit/download.html"),
            _ManifestFile("share/doc/hpctoolkit/examples.html"),
            _ManifestFile("share/doc/hpctoolkit/fig/hpctoolkit-workflow.png"),
            _ManifestFile("share/doc/hpctoolkit/fig/hpcviewer-annotated-screenshot.jpg"),
            _ManifestFile("share/doc/hpctoolkit/fig/index.html"),
            _ManifestFile("share/doc/hpctoolkit/fig/spacer.gif"),
            _ManifestFile("share/doc/hpctoolkit/FORMATS.md"),
            _ManifestFile("share/doc/hpctoolkit/googleeeb6a75d4102e1ef.html"),
            _ManifestFile("share/doc/hpctoolkit/hpctoolkit.org.sitemap.txt"),
            _ManifestFile("share/doc/hpctoolkit/index.html"),
            _ManifestFile("share/doc/hpctoolkit/info-acks.html"),
            _ManifestFile("share/doc/hpctoolkit/info-people.html"),
            _ManifestFile("share/doc/hpctoolkit/LICENSE"),
            _ManifestFile("share/doc/hpctoolkit/man/hpclink.html"),
            _ManifestFile("share/doc/hpctoolkit/man/hpcprof-mpi.html"),
            _ManifestFile("share/doc/hpctoolkit/man/hpcprof.html"),
            _ManifestFile("share/doc/hpctoolkit/man/hpcproftt.html"),
            _ManifestFile("share/doc/hpctoolkit/man/hpcrun.html"),
            _ManifestFile("share/doc/hpctoolkit/man/hpcstruct.html"),
            _ManifestFile("share/doc/hpctoolkit/man/hpctoolkit.html"),
            _ManifestFile("share/doc/hpctoolkit/man/hpcviewer.html"),
            _ManifestFile("share/doc/hpctoolkit/manual/HPCToolkit-users-manual.pdf"),
            _ManifestFile("share/doc/hpctoolkit/overview.html"),
            _ManifestFile("share/doc/hpctoolkit/publications.html"),
            _ManifestFile("share/doc/hpctoolkit/README.Acknowledgments"),
            _ManifestFile("share/doc/hpctoolkit/README.Install"),
            _ManifestFile("share/doc/hpctoolkit/README.md"),
            _ManifestFile("share/doc/hpctoolkit/README.ReleaseNotes"),
            _ManifestFile("share/doc/hpctoolkit/software-instructions.html"),
            _ManifestFile("share/doc/hpctoolkit/software.html"),
            _ManifestFile("share/doc/hpctoolkit/spack-issues.html"),
            _ManifestFile("share/doc/hpctoolkit/style/footer-hpctoolkit.js"),
            _ManifestFile("share/doc/hpctoolkit/style/header-hpctoolkit.js"),
            _ManifestFile("share/doc/hpctoolkit/style/header.gif"),
            _ManifestFile("share/doc/hpctoolkit/style/index.html"),
            _ManifestFile("share/doc/hpctoolkit/style/style.css"),
            _ManifestFile("share/doc/hpctoolkit/training.html"),
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
            _ManifestFile("share/man/man1/hpcprof-mpi.1hpctoolkit"),
            _ManifestFile("share/man/man1/hpcprof.1hpctoolkit"),
            _ManifestFile("share/man/man1/hpcproftt.1hpctoolkit"),
            _ManifestFile("share/man/man1/hpcrun.1hpctoolkit"),
            _ManifestFile("share/man/man1/hpcstruct.1hpctoolkit"),
            _ManifestFile("share/man/man1/hpctoolkit.1hpctoolkit"),
            _ManifestFile("share/man/man1/hpcviewer.1hpctoolkit"),
            _ManifestLib("lib/hpctoolkit/libhpcrun_audit.so", ".0.0.0", ".0", ""),
            _ManifestLib("lib/hpctoolkit/libhpcrun_dlmopen.so", ".0.0.0", ".0", ""),
            _ManifestLib("lib/hpctoolkit/libhpcrun_fake_audit.so", ".0.0.0", ".0", ""),
            _ManifestLib("lib/hpctoolkit/libhpcrun_ga.so", ".0.0.0", ".0", ""),
            _ManifestLib("lib/hpctoolkit/libhpcrun_gprof.so", ".0.0.0", ".0", ""),
            _ManifestLib("lib/hpctoolkit/libhpcrun_io.so", ".0.0.0", ".0", ""),
            _ManifestLib("lib/hpctoolkit/libhpcrun_memleak.so", ".0.0.0", ".0", ""),
            _ManifestLib("lib/hpctoolkit/libhpcrun_pthread.so", ".0.0.0", ".0", ""),
            _ManifestLib("lib/hpctoolkit/libhpctoolkit.so", ".0.0.0", ".0", ""),
        ]

        if mpi:
            self.files += [
                _ManifestFile("bin/hpcprof-mpi"),
            ]

    def check(self, installdir) -> ManifestResult:
        """Check whether installdir contains all the files expected"""
        installdir = Path(installdir)

        # First derive the full listing of actually installed files
        listing = set()
        for root, _, files in os.walk(installdir):
            for fn in files:
                listing.add((Path(root) / fn).relative_to(installdir))

        # Then match these files up with the results we found
        result = ManifestResult()
        for f in self.files:
            found, not_found = f.check(installdir)
            result.warnings.extend(f"+ {fn.as_posix()}" for fn in found - listing)
            result.n_unexpected += len(found - listing)
            listing -= found
            for fn in not_found:
                if isinstance(fn, tuple):
                    fn, msg = fn
                    result.errors.append(f"! {fn.as_posix()}\n  ^ {textwrap.indent(msg, '    ')}")
                else:
                    result.errors.append(f"- {fn.as_posix()}")
            result.n_uninstalled += len(not_found)
        return result
