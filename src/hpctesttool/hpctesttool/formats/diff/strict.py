# SPDX-FileCopyrightText: 2022-2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

import collections.abc
import contextlib
import dataclasses
import difflib
import enum
import functools
import io
import itertools
import math
import textwrap
import typing

import ruamel.yaml

from .. import v4
from ..base import StructureBase, canonical_paths
from ._util import check_fields
from .base import AccuracyStrategy, DiffHunk, DiffStrategy

__all__ = ["MonotonicHunk", "FixedAlteredHunk", "StrictDiff", "StrictAccuracy"]


def _pretty_path(path: "collections.abc.Iterable[typing.Union[str, int]]", obj) -> typing.List[str]:
    """Given a canonical path to a field of obj, generate a list of lines suitable for printing,
    except for indentation.
    """
    result = [""]
    for p in path:
        assert isinstance(p, (int, str))
        if isinstance(obj, StructureBase):
            assert isinstance(p, str)
            if obj.shorthand is not None:
                result[-1] += ":   # " + obj.shorthand
            else:
                result[-1] += ":"
        if isinstance(p, int):
            result[-1] += f"[{p:d}]"
            obj = obj[p]
        elif isinstance(obj, dict):
            result[-1] += f"[{p!r}]"
            obj = obj[p]
        else:
            result.append(p)
            obj = getattr(obj, p)

    if result[0].startswith(":") or result[0] == "":
        del result[0]

    if isinstance(obj, StructureBase) and obj.shorthand is not None:
        result[-1] += ":   # " + obj.shorthand
    else:
        result[-1] += ":"

    return result


def _yaml2str(obj, typ: str) -> str:
    with io.StringIO() as sf:
        ruamel.yaml.YAML(typ=typ).dump(obj, sf)
        return sf.getvalue()


def _stripped_obj(obj) -> dict:
    return {
        f.name: getattr(obj, f.name)
        for f in dataclasses.fields(obj)
        if not isinstance(getattr(obj, f.name), StructureBase)
    }


class _TrieNode(collections.defaultdict):
    def __init__(self):
        super().__init__(self.__class__)
        self.removed = set()
        self.added = set()
        self.altered = {}

    def render(self, depth: int, out: typing.TextIO):
        indent = "  " * depth
        prefix_add = "+" + indent
        prefix_rm = "-" + indent
        prefix_same = " " + indent

        for obj_a, obj_b in self.altered.items():
            if obj_a.yaml_tag == obj_b.yaml_tag:
                out.write(prefix_same + obj_a.yaml_tag + "\n")
            else:
                out.write(prefix_rm + obj_a.yaml_tag + "\n")
                out.write(prefix_add + obj_b.yaml_tag + "\n")
            str_a = textwrap.indent(_yaml2str(_stripped_obj(obj_a), "rt"), indent)
            str_b = textwrap.indent(_yaml2str(_stripped_obj(obj_b), "rt"), indent)
            out.write(
                "".join(difflib.ndiff(str_a.splitlines(True), str_b.splitlines(True))).rstrip()
                + "\n"
            )
        for obj in self.removed:
            out.write(
                textwrap.indent(_yaml2str(obj, "rt").strip(), prefix_rm, lambda _: True) + "\n"
            )
        for obj in self.added:
            out.write(
                textwrap.indent(_yaml2str(obj, "rt").strip(), prefix_add, lambda _: True) + "\n"
            )

        for key in sorted(self.keys()):
            out.write(" " + indent + key + "\n")
            self[key].render(depth + 1, out)


class MonotonicHunk(DiffHunk):
    """Hunk representing a single addition or removal an entire object-ownership-tree."""

    class Mode(enum.Enum):
        ADDITION = "+"
        REMOVAL = "-"

    def __init__(self, mode: Mode, subject):
        super().__init__()
        self.mode = self.Mode(mode)
        self.subject = subject

    def __repr__(self):
        return f"{self.__class__.__name__}({self.mode!r}, {self.subject!r})"


class FixedAlteredHunk(DiffHunk):
    """Hunk representing the alteration of a single "fixed-position" object. These objects have
    strict structural positions and so children in the object-ownership-tree may be matched.
    """

    def __init__(self, src, dst):
        super().__init__()
        self.src = src
        self.dst = dst

    def __repr__(self):
        return f"{self.__class__.__name__}({self.src!r}, {self.dst!r})"


ValT = typing.TypeVar("ValT")
SValT = typing.TypeVar("SValT", bound=StructureBase)


class StrictDiff(DiffStrategy):
    """Strict DiffStrategy that only accepts matches with no semantic differences."""

    _contexts: typing.List[typing.Dict[StructureBase, StructureBase]]
    removed: typing.Set[StructureBase]
    added: typing.Set[StructureBase]
    altered: typing.Dict[StructureBase, StructureBase]

    def __init__(self, a, b):
        super().__init__(a, b)
        self._contexts, self.removed, self.added, self.altered = [{}], set(), set(), {}
        self._update(a, b)

    def contexts(self):
        return self._contexts

    @functools.cached_property
    def hunks(self):
        return (
            [MonotonicHunk(MonotonicHunk.Mode.REMOVAL, s) for s in self.removed]
            + [MonotonicHunk(MonotonicHunk.Mode.ADDITION, s) for s in self.added]
            + [FixedAlteredHunk(a, b) for a, b in self.altered.items()]
        )

    def render(self, out: typing.TextIO):
        a_paths, b_paths = canonical_paths(self.a), canonical_paths(self.b)
        hunktrie = _TrieNode()
        for obj_a in self.removed:
            cur = hunktrie
            for e in _pretty_path(a_paths[obj_a], self.a):
                cur = cur[e]
            cur.removed.add(obj_a)
        for obj_b in self.added:
            cur = hunktrie
            for e in _pretty_path(b_paths[obj_b], self.b):
                cur = cur[e]
            cur.added.add(obj_b)
        for obj_a, obj_b in self.altered.items():
            cur = hunktrie
            for e in _pretty_path(a_paths[obj_a], self.a):
                cur = cur[e]
            cur.altered[obj_a] = obj_b

        hunktrie.render(0, out)

    @contextlib.contextmanager
    def _preserve_state(self):
        old_state = self._contexts, self.removed, self.added, self.altered
        self._contexts = [c.copy() for c in self._contexts]
        self.removed = self.removed.copy()
        self.added = self.added.copy()
        self.altered = self.altered.copy()
        try:
            yield
        finally:
            self._contexts, self.removed, self.added, self.altered = old_state

    def _failure_canary(self):
        """Return an object which, when changes, indicates new failures have been encountered."""
        return (len(self.removed), len(self.added), len(self.altered))

    def _set(self, a: SValT, b: SValT):
        """Define without a shadow of a doubt that a == b."""
        for c in self._contexts:
            c[a] = b
            assert len(c) == len(set(c.values()))

    def _presume(self, a: typing.Optional[SValT], b: typing.Optional[SValT]) -> bool:
        """Assert that a == b from a prior _update. In other words, deny any context mapping in
        which a != b. Returns False if that would result in no contexts at all.
        """
        if a is None or b is None:
            return a is None and b is None
        new_ctxs = [c for c in self._contexts if a in c and c[a] is b]
        if new_ctxs:
            with self._alter_contexts():
                self._contexts = new_ctxs
            return True
        return False

    def _presume_keyed(
        self,
        a: ValT,
        b: ValT,
        *,
        key: typing.Optional[
            typing.Tuple[typing.Dict[ValT, SValT], typing.Dict[ValT, SValT]]
        ] = None,
        raw_base_eq: bool = False,
    ) -> bool:
        """Assert that a == b from a prior _update, but understands the key keyword argument of _key_m.
        See _presume.
        """
        if key is not None:
            return self._presume(key[0][a], key[1][b])
        if raw_base_eq:
            return a == b
        assert isinstance(a, StructureBase)
        assert isinstance(b, StructureBase)
        return self._presume(a, b)

    @contextlib.contextmanager
    def _alter_contexts(self):
        all_a = set.union(*[set(c.keys()) for c in self._contexts])
        all_b = set.union(*[set(c.values()) for c in self._contexts])
        try:
            yield
        finally:
            for c in self._contexts:
                assert len(c) == len(set(c.values()))
                all_a -= set(c.keys())
                all_b -= set(c.values())
            self.removed.update(all_a)
            self.added.update(all_b)

    @functools.singledispatchmethod
    def _update(self, a, b):
        """Update the Diff-state presuming a == b. If it does not, update the Diff-state with a new
        set of Hunks indicating the difference. Then, recurse into the objects and attempt to match
        those as well.
        """
        raise NotImplementedError(f"Unable to diff type {type(a)}")

    @_update.register
    def _(self, a: None, b: None):
        pass

    del _

    @functools.singledispatchmethod
    def _key(self, o: typing.Any, *, side_a: bool) -> collections.abc.Hashable:
        """Derive a hashable key from the given object, that uniquely identifies it compared to its
        siblings.
        """
        raise NotImplementedError(f"No unique key for type {type(o)}")

    def _key_a(self, o: typing.Any) -> collections.abc.Hashable:
        return self._key(o, side_a=True)

    def _key_b(self, o: typing.Any) -> collections.abc.Hashable:
        return self._key(o, side_a=False)

    def _key_m(
        self, o, *, side_a: bool, key: typing.Optional[typing.Tuple[dict, dict]] = None
    ) -> collections.abc.Hashable:
        return self._key(key[0 if side_a else 1][o] if key is not None else o, side_a=side_a)

    @_key.register
    def _(self, o: None, *, side_a: bool):
        _ = side_a
        return o

    @_key.register
    def _(self, o: int, *, side_a: bool):
        _ = side_a
        return o

    @_key.register
    def _(self, o: str, *, side_a: bool):
        _ = side_a
        return o

    def _sequential_update(
        self, a: "collections.abc.Iterable[SValT]", b: "collections.abc.Iterable[SValT]"
    ):
        """Update the Diff-state presuming a[i] == b[i] for every possible i."""
        for ea, eb in itertools.zip_longest(a, b):
            if eb is None:
                self.removed.add(ea)
            elif ea is None:
                self.added.add(eb)
            else:
                self._update(ea, eb)

    def _isomorphic_update(  # noqa: C901
        self, a: "collections.abc.Iterable[SValT]", b: "collections.abc.Iterable[SValT]"
    ):
        """Update the Diff-state assuming a == b when _key(a) == _key(b), ignoring order between
        the two sequences.
        """
        a_dict = collections.defaultdict(set)
        for e in a:
            a_dict[self._key(e, side_a=True)].add(e)

        b_dict = collections.defaultdict(set)
        for e in b:
            b_dict[self._key(e, side_a=False)].add(e)

        for ka, all_va in a_dict.items():
            all_vb = b_dict[ka]
            if len(all_va) == 1 and len(all_va) == len(all_vb):
                # The key provides an unambiguous match, so use that
                self._update(next(iter(all_va)), next(iter(all_vb)))
                continue

            # Build up a list of all perfect matches. We only consider ambiguity when it wouldn't
            # result in match failure later on down the line.
            matches: typing.Dict[SValT, typing.Set[SValT]] = {}
            for va in all_va:
                matches[va] = set()
                for vb in all_vb:
                    fcan = self._failure_canary()
                    with self._preserve_state():
                        self._update(va, vb)
                        if self._failure_canary() == fcan:
                            matches[va].add(vb)

            # Locate any cases where a va matches with exactly one vb in practice.
            # These cases are not ambiguous and we can remove them from the comparison.
            done = False
            while not done:
                to_rm = []
                for va, good_vbs in matches.items():
                    if len(good_vbs) == 1:
                        to_rm.append((va, next(iter(good_vbs))))
                for va, vb in to_rm:
                    self._update(va, vb)
                    del matches[va]
                    all_va.remove(va)
                    all_vb.remove(vb)
                    for good_vbs in matches.values():
                        good_vbs.discard(vb)
                done = len(to_rm) == 0

            # Locate any cases where a va matches nothing or a vb has no va.
            # These cases are definitely removals/additions so we can handle them.
            for va in [va for va, good_vbs in matches.items() if len(good_vbs) == 0]:
                del matches[va]
                all_va.remove(va)
                self.removed.add(va)
            for vb in all_vb - set().union(*matches.values()):
                all_vb.remove(vb)
                self.added.add(vb)

            if not matches:
                continue

            # Whatever remains in matches is true ambiguity. Spin out more _contexts to
            # represent all the available options.
            final_contexts = []
            va_list = list(all_va)
            for vb_list in itertools.product(*[matches[va] for va in va_list]):
                assert len(vb_list) == len(va_list)
                if len(set(vb_list)) == len(vb_list):  # No overlap allowed
                    with self._preserve_state():
                        for va, vb in zip(va_list, vb_list):
                            self._update(va, vb)
                        final_contexts.extend(self._contexts)
            if final_contexts:
                with self._alter_contexts():
                    self._contexts = final_contexts
            else:
                # Something went horribly wrong. Consider it a full rm/add sequence.
                self.removed.update(all_va)
                self.added.update(all_vb)

        for kb, all_vb in b_dict.items():
            if kb not in a_dict:
                for vb in all_vb:
                    self.added.add(vb)

    def _delegated_update(
        self,
        a: "collections.abc.Iterable[typing.Tuple[SValT, typing.Optional[StructureBase]]]",
        b: "collections.abc.Iterable[typing.Tuple[SValT, typing.Optional[StructureBase]]]",
    ):
        """Update the Diff-state presuming that a[0] == b[0] if a[1] == b[1] in the current Diff-state."""
        b_dict: typing.Dict[StructureBase, SValT] = {}
        for vb, db in b:
            if db is None:
                self.added.add(vb)
                continue
            if db in b_dict:
                raise ValueError(f"Key conflict between {vb} and {b_dict[db]}")
            b_dict[db] = vb
        b_set = set(b_dict.values())

        def worker(va: ValT, da: StructureBase) -> bool:
            # Deny any context mapping where the delegation would fail. If it fails in all
            # current mappings, consider this a failure.
            new_ctxs = [c for c in self._contexts if da in c and c[da] in b_dict]
            if not new_ctxs:
                return False
            with self._alter_contexts():
                self._contexts = new_ctxs

            # Attempt to _update each context mapping based on the delegation, if possible.
            new_ctxs = []
            for c in self._contexts:
                vb = b_dict[c[da]]
                self._contexts = [c]
                fcan = self._failure_canary()
                with self._preserve_state():
                    self._update(va, vb)
                    if self._failure_canary() == fcan:
                        assert self._contexts
                        new_ctxs.extend(self._contexts)
                        b_set.discard(vb)
            if not new_ctxs:
                return False
            with self._alter_contexts():
                self._contexts = new_ctxs

            return True

        a_dict = {}
        for va, da in a:
            a_dict[da] = va
            if da is None or not worker(va, da):
                self.removed.add(va)
        self.added.update(b_set)

    @_update.register
    @check_fields("meta", "profile", "context", "trace")
    def _(self, a: v4.Database, b: v4.Database):
        self._set(a, b)
        self._update(a.meta, b.meta)
        self._update(a.profile, b.profile)
        self._update(a.context, b.context)
        if a.trace is not None:
            if b.trace is not None:
                self._update(a.trace, b.trace)
            else:
                self.removed.add(a.trace)
        elif b.trace is not None:
            self.added.add(b.trace)

    # =====================
    #   formats.v4.metadb
    # =====================

    @_update.register
    @check_fields("general", "id_names", "metrics", "files", "modules", "functions", "context")
    def _(self, a: v4.metadb.MetaDB, b: v4.metadb.MetaDB):
        self._set(a, b)
        self._update(a.general, b.general)
        self._update(a.id_names, b.id_names)
        self._update(a.metrics, b.metrics)
        self._update(a.files, b.files)
        self._update(a.modules, b.modules)
        self._update(a.functions, b.functions)
        self._update(a.context, b.context)

    @_update.register
    @check_fields("title", "description")
    def _(self, a: v4.metadb.GeneralProperties, b: v4.metadb.GeneralProperties):
        if a.title == b.title and a.description == b.description:
            self._set(a, b)
        else:
            self.altered[a] = b

    @_update.register
    @check_fields("names")
    def _(self, a: v4.metadb.IdentifierNames, b: v4.metadb.IdentifierNames):
        # The order the of the names is not semantically important
        if set(a.names) == set(b.names):
            self._set(a, b)
        else:
            self.altered[a] = b

    @_update.register
    @check_fields("scopes", "metrics")
    def _(self, a: v4.metadb.PerformanceMetrics, b: v4.metadb.PerformanceMetrics):
        self._set(a, b)
        self._isomorphic_update(a.scopes, b.scopes)
        self._isomorphic_update(a.metrics, b.metrics)

    @_key.register
    @check_fields("scope_name", "type", "propagation_index")
    def _(self, o: v4.metadb.PropagationScope, *, side_a: bool):
        _ = side_a
        # propagationIndex is an ID, not semantically important
        return (o.scope_name, o.type)

    @_update.register
    @check_fields("scope_name", "type", "propagation_index")
    def _(self, a: v4.metadb.PropagationScope, b: v4.metadb.PropagationScope):
        assert self._key_a(a) == self._key_b(b)
        self._set(a, b)

    @_key.register
    @check_fields("name", "scope_insts", "summaries")
    def _(self, o: v4.metadb.Metric, *, side_a: bool):
        _ = side_a
        return (o.name,)

    @_update.register
    @check_fields("name", "scope_insts", "summaries")
    def _(self, a: v4.metadb.Metric, b: v4.metadb.Metric):
        assert self._key_a(a) == self._key_b(b)
        self._set(a, b)
        self._isomorphic_update(a.scope_insts, b.scope_insts)
        self._isomorphic_update(a.summaries, b.summaries)

    @_key.register
    @check_fields("scope", "prop_metric_id")
    def _(self, o: v4.metadb.PropagationScopeInstance, *, side_a: bool):
        # prop_metric_id is an ID, not semantically important
        return (self._key(o.scope, side_a=side_a),)

    @_update.register
    @check_fields("scope", "prop_metric_id")
    def _(self, a: v4.metadb.PropagationScopeInstance, b: v4.metadb.PropagationScopeInstance):
        assert self._key_a(a) == self._key_b(b)
        if self._presume(a.scope, b.scope):
            self._set(a, b)
        else:
            self.altered[a] = b

    @_key.register
    @check_fields("scope", "formula", "combine", "stat_metric_id")
    def _(self, o: v4.metadb.SummaryStatistic, *, side_a: bool):
        # stat_metric_id is an ID, not semantically important
        return (self._key(o.scope, side_a=side_a), o.formula, o.combine)

    @_update.register
    @check_fields("scope", "formula", "combine", "stat_metric_id")
    def _(self, a: v4.metadb.SummaryStatistic, b: v4.metadb.SummaryStatistic):
        assert self._key_a(a) == self._key_b(b)
        if self._presume(a.scope, b.scope):
            self._set(a, b)
        else:
            self.altered[a] = b

    @_update.register
    @check_fields("files")
    def _(self, a: v4.metadb.SourceFiles, b: v4.metadb.SourceFiles):
        self._set(a, b)
        self._isomorphic_update(a.files, b.files)

    @_key.register
    @check_fields("flags", "path")
    def _(self, o: v4.metadb.File, *, side_a: bool):
        _ = side_a
        return (o.flags, o.path)

    @_update.register
    @check_fields("flags", "path")
    def _(self, a: v4.metadb.File, b: v4.metadb.File):
        assert self._key_a(a) == self._key_b(b)
        self._set(a, b)

    @_update.register
    @check_fields("modules")
    def _(self, a: v4.metadb.LoadModules, b: v4.metadb.LoadModules):
        self._set(a, b)
        self._isomorphic_update(a.modules, b.modules)

    @_key.register
    @check_fields("flags", "path")
    def _(self, o: v4.metadb.Module, *, side_a: bool):
        _ = side_a
        return (o.flags, o.path)

    @_update.register
    @check_fields("flags", "path")
    def _(self, a: v4.metadb.Module, b: v4.metadb.Module):
        assert self._key_a(a) == self._key_b(b)
        self._set(a, b)

    @_update.register
    @check_fields("functions")
    def _(self, a: v4.metadb.Functions, b: v4.metadb.Functions):
        self._set(a, b)
        self._isomorphic_update(a.functions, b.functions)

    @_key.register
    @check_fields("name", "module", "offset", "file", "line", "flags")
    def _(self, o: v4.metadb.Function, *, side_a: bool):
        return (
            o.name,
            o.flags,
            self._key(o.module, side_a=side_a),
            o.offset,
            self._key(o.file, side_a=side_a),
            o.line,
        )

    @_update.register
    @check_fields("name", "module", "offset", "file", "line", "flags")
    def _(self, a: v4.metadb.Function, b: v4.metadb.Function):
        assert self._key_a(a) == self._key_b(b)
        # NB: Avoid short-circuit evaluation
        x = self._presume(a.module, b.module)
        y = self._presume(a.file, b.file)
        if x and y:
            self._set(a, b)
        else:
            self.altered[a] = b

    @_update.register
    @check_fields("entry_points")
    def _(self, a: v4.metadb.ContextTree, b: v4.metadb.ContextTree):
        self._set(a, b)
        self._isomorphic_update(a.entry_points, b.entry_points)

    @_key.register
    @check_fields("ctx_id", "entry_point", "pretty_name", "children")
    def _(self, o: v4.metadb.EntryPoint, *, side_a: bool):
        _ = side_a
        # ctxId is an ID, not semantically important
        return (o.entry_point,)

    @_update.register
    @check_fields("ctx_id", "entry_point", "pretty_name", "children")
    def _(self, a: v4.metadb.EntryPoint, b: v4.metadb.EntryPoint):
        assert self._key_a(a) == self._key_b(b)
        if a.pretty_name == b.pretty_name:
            self._set(a, b)
        else:
            self.altered[a] = b
        self._isomorphic_update(a.children, b.children)

    @_key.register
    @check_fields(
        "ctx_id",
        "flags",
        "relation",
        "lexical_type",
        "propagation",
        "function",
        "file",
        "line",
        "module",
        "offset",
        "children",
    )
    def _(self, o: v4.metadb.Context, *, side_a: bool):
        return (
            o.flags,
            o.relation,
            o.lexical_type,
            self._key(o.function, side_a=side_a),
            self._key(o.file, side_a=side_a),
            o.line,
            self._key(o.module, side_a=side_a),
            o.offset,
        )

    @_update.register
    @check_fields(
        "ctx_id",
        "flags",
        "relation",
        "lexical_type",
        "propagation",
        "function",
        "file",
        "line",
        "module",
        "offset",
        "children",
    )
    def _(self, a: v4.metadb.Context, b: v4.metadb.Context):
        assert self._key_a(a) == self._key_b(b)
        x = self._presume(a.function, b.function)
        y = self._presume(a.file, b.file)
        z = self._presume(a.module, b.module)
        # TODO: propagation
        if x and y and z:
            self._set(a, b)
        else:
            self.altered[a] = b
        self._isomorphic_update(a.children, b.children)

    # ========================
    #   formats.v4.profiledb
    # ========================

    @_update.register
    @check_fields("profile_infos")
    def _(self, a: v4.profiledb.ProfileDB, b: v4.profiledb.ProfileDB):
        self._set(a, b)
        self._update(a.profile_infos, b.profile_infos)

    @_update.register
    @check_fields("profiles")
    def _(self, a: v4.profiledb.ProfileInfos, b: v4.profiledb.ProfileInfos):
        self._set(a, b)
        self._isomorphic_update(a.profiles, b.profiles)

    @_key.register
    @check_fields("id_tuple", "flags", "values")
    def _(self, o: v4.profiledb.Profile, *, side_a: bool):
        return (self._key(o.id_tuple, side_a=side_a), o.flags)

    @_update.register
    @check_fields("id_tuple", "flags", "values")
    def _(self, a: v4.profiledb.Profile, b: v4.profiledb.Profile):
        assert self._key_a(a) == self._key_b(b)
        self._set(a, b)
        self._update(a.id_tuple, b.id_tuple)

    @_key.register
    @check_fields("ids")
    def _(self, o: v4.profiledb.IdentifierTuple, *, side_a: bool):
        return tuple(self._key(i, side_a=side_a) for i in o.ids)

    @_update.register
    @check_fields("ids")
    def _(self, a: v4.profiledb.IdentifierTuple, b: v4.profiledb.IdentifierTuple):
        self._set(a, b)
        self._sequential_update(a.ids, b.ids)

    @_key.register
    @check_fields("kind", "flags", "logical_id", "physical_id")
    def _(self, o: v4.profiledb.Identifier, *, side_a: bool):
        if isinstance(self.a, v4.Database):
            assert isinstance(self.b, v4.Database)
            key = (self.a.kind_map, self.b.kind_map)
        else:
            key = None
        return (
            self._key_m(o.kind, side_a=side_a, key=key),
            o.flags,
            o.logical_id,
            o.physical_id,
        )

    @_update.register
    @check_fields("kind", "flags", "logical_id", "physical_id")
    def _(self, a: v4.profiledb.Identifier, b: v4.profiledb.Identifier):
        if self._key_a(a) == self._key_b(b):
            self._set(a, b)
        else:
            self.altered[a] = b

    # ====================
    #   formats.v4.cctdb
    # ====================

    @_update.register
    @check_fields("ctx_infos")
    def _(self, a: v4.cctdb.ContextDB, b: v4.cctdb.ContextDB):
        self._set(a, b)
        self._update(a.ctx_infos, b.ctx_infos)

    @_update.register
    @check_fields("contexts")
    def _(self, a: v4.cctdb.ContextInfos, b: v4.cctdb.ContextInfos):
        self._set(a, b)
        if isinstance(self.a, v4.Database):
            assert isinstance(self.b, v4.Database)
            # The PerContexts are associated based on the association of the Contexts in meta.db
            # PerContexts that don't have an associated Context are left unassociated
            self._delegated_update(
                (
                    (a.contexts[cid], ctx)
                    for cid, ctx in self.a.context_map.items()
                    if cid < len(a.contexts) and ctx is not None
                ),
                (
                    (b.contexts[cid], ctx)
                    for cid, ctx in self.b.context_map.items()
                    if cid < len(b.contexts) and ctx is not None
                ),
            )

            # Special case: the roots (ctxId 0) are always associated with each other
            self._sequential_update(a.contexts[:1], b.contexts[:1])
        else:
            # The PerContexts are associated based on their Context's id
            self._sequential_update(a.contexts, b.contexts)

    @_update.register
    @check_fields("values")
    def _(self, a: v4.cctdb.PerContext, b: v4.cctdb.PerContext):
        self._set(a, b)

    # ======================
    #   formats.v4.tracedb
    # ======================

    @_update.register
    @check_fields("ctx_traces")
    def _(self, a: v4.tracedb.TraceDB, b: v4.tracedb.TraceDB):
        self._set(a, b)
        self._update(a.ctx_traces, b.ctx_traces)

    @_update.register
    @check_fields("timestamp_range", "traces")
    def _(self, a: v4.tracedb.ContextTraceHeadersSection, b: v4.tracedb.ContextTraceHeadersSection):
        if a.timestamp_range == b.timestamp_range:
            self._set(a, b)
        else:
            self.altered[a] = b
        self._isomorphic_update(a.traces, b.traces)

    @_key.register
    @check_fields("prof_index", "line")
    def _(self, o: v4.tracedb.ContextTrace, *, side_a: bool):
        if isinstance(self.a, v4.Database):
            assert isinstance(self.b, v4.Database)
            key = (self.a.profile_map, self.b.profile_map)
        else:
            key = None
        return (self._key_m(o.prof_index, side_a=side_a, key=key),)

    @_update.register
    @check_fields("prof_index", "line")
    def _(self, a: v4.tracedb.ContextTrace, b: v4.tracedb.ContextTrace):
        assert self._key_a(a) == self._key_b(b)
        if isinstance(self.a, v4.Database):
            assert isinstance(self.b, v4.Database)
            key = (self.a.profile_map, self.b.profile_map)
        else:
            key = None
        if self._presume_keyed(a.prof_index, b.prof_index, key=key, raw_base_eq=True):
            self._set(a, b)
        else:
            self.altered[a] = b
        self._sequential_update(a.line, b.line)

    @_update.register
    @check_fields("timestamp", "ctx_id")
    def _(self, a: v4.tracedb.ContextTraceElement, b: v4.tracedb.ContextTraceElement):
        if isinstance(self.a, v4.Database):
            assert isinstance(self.b, v4.Database)
            key = (self.a.context_map, self.b.context_map)
        else:
            key = None
        x = self._presume_keyed(a.ctx_id, b.ctx_id, key=key, raw_base_eq=True)
        if a.timestamp == b.timestamp and x:
            self._set(a, b)
        else:
            self.altered[a] = b


class CmpError(enum.Enum):
    bad_sign = enum.auto()
    exp_diff = enum.auto()

    def __lt__(self, other):
        if isinstance(other, float):
            return False
        return self.value < other.value

    def __gt__(self, other):
        if isinstance(other, float):
            return True
        return self.value > other.value


class StrictAccuracy(AccuracyStrategy):
    """Strict AccuracyStrategy that only accepts matches within numerical inaccuracy."""

    def __init__(self, diff: DiffStrategy, *, grace: int = 16, precision: int = 53):
        super().__init__(diff)
        assert grace < math.pow(2, precision)
        assert precision > 1
        self._ulp2norm = math.pow(2, -precision)
        self._norm2ulp = math.pow(2, precision)
        self._grace = grace * self._ulp2norm

        # Try every potential context from the Diff, and optimize for failure count so long as
        # the total count doesn't suffer (and otherwise prefer earlier contexts).
        best_failed_cnt, best_total_cnt, best_failures = -1, 0, {}
        for a2b in diff.contexts():
            self._diffcontext = a2b
            self.failed_cnt, self.total_cnt = 0, 0
            self.failures: typing.Dict[
                typing.Tuple[float, float, typing.Union[float, CmpError]], typing.List[str]
            ] = {}
            for a, b in a2b.items():
                self._compare(a, b)
            if (
                best_failed_cnt == -1 or self.failed_cnt < best_failed_cnt
            ) and self.total_cnt >= best_total_cnt:
                best_failed_cnt, best_total_cnt, best_failures = (
                    self.failed_cnt,
                    self.total_cnt,
                    self.failures,
                )
            if best_failed_cnt == 0:
                break  # Can't get any better
        self.failed_cnt, self.total_cnt, self.failures = (
            best_failed_cnt,
            best_total_cnt,
            best_failures,
        )

    @property
    def inaccuracy(self) -> float:
        if self.total_cnt == 0:
            return 1
        if self.failed_cnt == 0:
            return 0
        return self.failed_cnt / self.total_cnt

    def render(self, out: typing.TextIO):
        print(
            f"Identified {self.inaccuracy*100:.2f}% inaccuracies in {self.total_cnt:d} values",
            file=out,
        )
        if self.total_cnt == 0:
            print("  No values were compared!", file=out)
        elif self.failed_cnt > 0:
            fails = sorted(self.failures, key=lambda x: x[2], reverse=True)
            suffix = " (of first 1000 encountered)" if self.failed_cnt >= 1000 else ""
            if len(fails) <= 30:
                print(f"  Details of failures{suffix} (expected != got):", file=out)
            else:
                print(f"  Details of 30 worst failures{suffix} (expected != got):", file=out)
            for key in fails[:30]:
                a, b, diff = key
                paths = self.failures[key]

                if diff == CmpError.bad_sign:
                    why = "values have differing signs"
                elif diff == CmpError.exp_diff:
                    why = "values differ by more than a power of 2"
                elif isinstance(diff, float):
                    exp_a, exp_b = math.frexp(abs(a))[1], math.frexp(abs(b))[1]
                    exp = (
                        f"{exp_a:d}"
                        if exp_a == exp_b
                        else f"({min(exp_a,exp_b):d}|{max(exp_a,exp_b):d})"
                    )
                    why = f"difference of {diff:f} * 2^{exp} = {diff*self._norm2ulp} ULPs"
                else:
                    raise AssertionError(f"Invalid diff value: {diff!r}")

                print(f"  - {a:f} != {b:f}: {why} (abs diff {abs(a-b):f})", file=out)
                print(
                    "   At: "
                    + "\n".join(
                        textwrap.wrap(" ".join(paths), width=100, subsequent_indent=" " * 5)
                    ),
                    file=out,
                )

    def _float_cmp(self, a: float, b: float) -> typing.Union[float, CmpError]:
        """Compare two floats and return 0.0 if the two are equal. If not, returns the
        normalized difference or a CmpError indicating the error.
        """
        # Shortcut for true equality
        if a == b:
            return 0.0
        # Different signs are always different, no matter what
        if math.copysign(1.0, a) != math.copysign(1.0, b):
            return CmpError.bad_sign

        # Pick apart the floats into their mantissa and exponents
        a_m, a_e = math.frexp(abs(a))
        b_m, b_e = math.frexp(abs(b))
        if a_e == b_e:
            # The difference between the mantissas is the normalized difference
            diff = abs(a_m - b_m)
            return 0.0 if diff <= self._grace else diff

        # The exponents differ. Ensure a is always the smaller of the two.
        if b_e < a_e:
            a_m, a_e, b_m, b_e = b_m, b_e, a_m, a_e
        assert a_e < b_e
        if b_e - a_e > 1:
            # There is at least 2 orders of magnitude difference. They really aren't the same.
            return CmpError.exp_diff
        assert b_e - a_e == 1

        # The values are in neighboring exponents. Calculate the normalized difference by summing
        # the parts of the difference on each "side" of the exponent boundary.
        diff = (1.0 - a_m) + (b_m - 0.5)
        return 0.0 if diff <= self._grace else diff

    def _base_compare(
        self, a: float, b: float, *path: typing.Union[str, typing.Tuple[int, int]]
    ) -> None:
        """Compare two floats and mark the comparison as successful or not."""
        self.total_cnt += 1
        diff = self._float_cmp(a, b)
        if diff != 0.0:
            self.failed_cnt += 1
            if self.failed_cnt <= 1000:
                route = []
                for elem in path:
                    if isinstance(elem, str):
                        route.append("." + elem)
                    else:
                        i_a, i_b = elem
                        if i_a == i_b:
                            route.append(f"[{i_a:d}]")
                        else:
                            route.append(f"[{i_a:d}|{i_b:d}]")
                self.failures.setdefault((a, b, diff), []).append("".join(route))

    ObjT = typing.TypeVar("ObjT")
    ValT = typing.TypeVar("ValT")

    def _zip_by_id(
        self,
        a: typing.Dict[int, ValT],
        b: typing.Dict[int, ValT],
        id2a: typing.Optional[typing.Dict[int, ObjT]],
        b2id: typing.Optional[typing.Callable[[ObjT], int]],
    ) -> "collections.abc.Iterable[typing.Tuple[typing.Tuple[int, int], ValT, ValT]]":
        if id2a is not None:
            assert b2id is not None
            for id_a in a:
                if id_a in id2a and id2a[id_a] in self._diffcontext:
                    id_b = b2id(self._diffcontext[id2a[id_a]])
                    if id_b in b:
                        yield (id_a, id_b), a[id_a], b[id_b]
        else:
            for k in a:
                if k in b:
                    yield (k, k), a[k], b[k]

    del ObjT

    @functools.singledispatchmethod
    def _compare(self, a, b):
        """Compare the values contained in a and b, and log the results of comparison."""

    @_compare.register
    def _(self, a: v4.profiledb.Profile, b: v4.profiledb.Profile):
        ctx_map: typing.Optional[typing.Dict[int, v4.metadb.Context]]
        ctx_map, met_map, met_b2id = [None] * 3
        if isinstance(self.diff.a, v4.Database):
            assert isinstance(self.diff.b, v4.Database)
            ctx_map = self.diff.a.context_map
            met_map = (
                self.diff.a.summary_metric_map
                if a.Flags.is_summary in a.flags
                else self.diff.a.raw_metric_map
            )
            met_b2id = (
                (lambda m: m.stat_metric_id)
                if b.Flags.is_summary in b.flags
                else (lambda m: m.prop_metric_id)
            )
            i = self.diff.a.profile.profile_infos.profiles.index(
                a
            ), self.diff.b.profile.profile_infos.profiles.index(b)
        else:
            assert isinstance(self.diff.a, v4.profiledb.ProfileDB)
            assert isinstance(self.diff.b, v4.profiledb.ProfileDB)
            i = self.diff.a.profile_infos.profiles.index(
                a
            ), self.diff.b.profile_infos.profiles.index(b)
        for j, x, y in self._zip_by_id(a.values, b.values, ctx_map, lambda c: c.ctx_id):
            for k, v_a, v_b in self._zip_by_id(x, y, met_map, met_b2id):
                self._base_compare(v_a, v_b, "profiles", i, j, k)

    @_compare.register
    def _(self, a: v4.cctdb.PerContext, b: v4.cctdb.PerContext):
        prof_map: typing.Optional[typing.Dict[int, v4.profiledb.Profile]]
        met_map: typing.Optional[typing.Dict[int, v4.metadb.PropagationScopeInstance]]
        prof_map, prof_b2id, met_map = [None] * 3
        if isinstance(self.diff.a, v4.Database):
            assert isinstance(self.diff.b, v4.Database)
            prof_map = self.diff.a.profile_map
            prof_b2id = self.diff.b.profile.profile_infos.profiles.index
            met_map = self.diff.a.raw_metric_map
            i = (
                self.diff.a.context.ctx_infos.contexts.index(a),
                self.diff.b.context.ctx_infos.contexts.index(b),
            )
        else:
            assert isinstance(self.diff.a, v4.cctdb.ContextDB)
            assert isinstance(self.diff.b, v4.cctdb.ContextDB)
            i = self.diff.a.ctx_infos.contexts.index(a), self.diff.b.ctx_infos.contexts.index(b)
        for j, x, y in self._zip_by_id(a.values, b.values, met_map, lambda m: m.prop_metric_id):
            for k, v_a, v_b in self._zip_by_id(x, y, prof_map, prof_b2id):
                self._base_compare(v_a, v_b, "contexts", i, j, k)
