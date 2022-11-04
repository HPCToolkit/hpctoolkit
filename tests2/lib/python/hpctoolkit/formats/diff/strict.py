## * BeginRiceCopyright *****************************************************
##
## $HeadURL$
## $Id$
##
## --------------------------------------------------------------------------
## Part of HPCToolkit (hpctoolkit.org)
##
## Information about sources of support for research and development of
## HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
## --------------------------------------------------------------------------
##
## Copyright ((c)) 2022-2022, Rice University
## All rights reserved.
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are
## met:
##
## * Redistributions of source code must retain the above copyright
##   notice, this list of conditions and the following disclaimer.
##
## * Redistributions in binary form must reproduce the above copyright
##   notice, this list of conditions and the following disclaimer in the
##   documentation and/or other materials provided with the distribution.
##
## * Neither the name of Rice University (RICE) nor the names of its
##   contributors may be used to endorse or promote products derived from
##   this software without specific prior written permission.
##
## This software is provided by RICE and contributors "as is" and any
## express or implied warranties, including, but not limited to, the
## implied warranties of merchantability and fitness for a particular
## purpose are disclaimed. In no event shall RICE or contributors be
## liable for any direct, indirect, incidental, special, exemplary, or
## consequential damages (including, but not limited to, procurement of
## substitute goods or services; loss of use, data, or profits; or
## business interruption) however caused and on any theory of liability,
## whether in contract, strict liability, or tort (including negligence
## or otherwise) arising in any way out of the use of this software, even
## if advised of the possibility of such damage.
##
## ******************************************************* EndRiceCopyright *

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
import types
import typing as T

import ruamel.yaml

from .. import v4
from ..base import StructureBase, canonical_paths
from ._util import check_fields
from .base import AccuracyStrategy, DiffHunk, DiffStrategy

__all__ = ["MonotonicHunk", "FixedAlteredHunk", "StrictDiff", "StrictAccuracy"]


def _pretty_path(path: list[str | int], obj) -> list[str]:
    """Given a canonical path to a field of obj, generate a list of lines suitable for printing,
    except for indentation."""
    result = [""]
    for p in path:
        assert isinstance(p, int | str)
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

    def render(self, depth: int, out: io.TextIOBase):
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
                textwrap.indent(_yaml2str(obj, "rt").strip(), prefix_rm, lambda l: True) + "\n"
            )
        for obj in self.added:
            out.write(
                textwrap.indent(_yaml2str(obj, "rt").strip(), prefix_add, lambda l: True) + "\n"
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
    strict structural positions and so children in the object-ownership-tree may be matched."""

    def __init__(self, src, dst):
        super().__init__()
        self.src = src
        self.dst = dst

    def __repr__(self):
        return f"{self.__class__.__name__}({self.src!r}, {self.dst!r})"


ValT = T.TypeVar("ValT")


class Future(T.NamedTuple):
    context: dict[StructureBase, StructureBase]
    updates: set[tuple[StructureBase, StructureBase]]

    def copy(self):
        return Future(self.context.copy(), self.updates.copy())

    def defer_update(self, a: ValT, b: ValT):
        self.updates.add((a, b))

    def __str__(self):
        ctx = ", ".join(f"{a}: {b}" for a, b in self.context.items())
        ups = ", ".join(f"{a} = {b}" for a, b in self.updates)
        return f"{self.__class__.__name__}({{{ctx}}}, [{ups}])"


class StrictDiff(DiffStrategy):
    """Strict DiffStrategy that only accepts exact matches.

    The results of this DiffStrategy are MonotonicHunks, i.e. massive additions
    and removals where matches failed.
    """

    _context: dict[StructureBase, StructureBase]
    removed: set[StructureBase]
    added: set[StructureBase]
    altered: dict[StructureBase, StructureBase]
    _futures: list[Future]

    def __init__(self, a, b):
        super().__init__(a, b)
        self._context, self.removed, self.added, self.altered = {}, set(), set(), {}
        self._futures = []
        self._update(a, b)

    def contexts(self):
        if self._futures:
            yield from (self._context | f.context for f in self._futures)
        else:
            yield self._context

    @functools.cached_property
    def hunks(self):
        return (
            [MonotonicHunk(MonotonicHunk.Mode.REMOVAL, s) for s in self.removed]
            + [MonotonicHunk(MonotonicHunk.Mode.ADDITION, s) for s in self.added]
            + [FixedAlteredHunk(a, b) for a, b in self.altered.items()]
        )

    def render(self, out: io.TextIOBase):
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
        old_state = self._context, self.removed, self.added, self.altered, self._futures
        self._context = self._context.copy()
        self.removed = self.removed.copy()
        self.added = self.added.copy()
        self.altered = self.altered.copy()
        self._futures = [e.copy() for e in self._futures]
        try:
            yield
        finally:
            self._context, self.removed, self.added, self.altered, self._futures = old_state

    def _possible_future(self, a: ValT, b: ValT) -> bool:
        """Determine if a == b can spawn a potential future in the Diff-state. Meaning that it
        can be resolved without any new errors."""
        with self._preserve_state():
            lens = len(self.removed), len(self.added), len(self.altered)
            self._update(a, b)
            return lens == (len(self.removed), len(self.added), len(self.altered))

    def _spawn_futures(self, ab: collections.abc.Iterable[tuple[ValT, ValT]]):
        """Record the futures that spawn from this point when a == b for every given a,b pair.
        These must be "new" futures that are independent of previously seen futures."""
        newfutures: list[Future] = []
        for a, b in ab:
            # Record what would happen if a == b
            assert self._possible_future(a, b)
            with self._preserve_state():
                self._update(a, b)
                newctx, newfut = self._context, self._futures

            # Prune out prior state from the new state. This also ensures the prior state was not
            # altered along the way.
            for f in self._futures:
                del newfut[newfut.index(f)]
            for ca in self._context:
                del newctx[ca]

            # Generate the final set of new futures we add to the table
            for overlay in newfut if newfut else [{}]:
                assert not any(k in newctx for k in overlay)
                newfutures.append(Future(newctx | overlay, set()))
        self._futures.extend(newfutures)

    def _accept_future(self, future: Future):
        """Accept the given future and consider it truth."""
        # Delete this future any fuse it into the _context
        del self._futures[self._futures.index(future)]
        self._context.update(future.context)
        self._remove_bad_futures()
        self._simplify_futures()

        # Run any updates that were deferred until the Future was decided on
        for a, b in future.updates:
            self._update(a, b)

    def _deny_futures(self, where: T.Callable[[dict[StructureBase, StructureBase]], bool]):
        """Deny any futures where where(self._context) returns true."""
        newfutures, lostfutures = [], []
        for f in self._futures:
            (lostfutures if where(self._context | f.context) else newfutures).append(f)
        self._futures = newfutures

        # Some keys may no longer be provided in the futures. If so, mark them as removed.
        for f in lostfutures:
            for a in f.context:
                if not any(a in ff.context for ff in self._futures):
                    self.removed.add(a)

        self._simplify_futures()

    def _remove_bad_futures(self):
        """Remove any "bad" futures which are attempting to overwrite the truth."""
        newfutures = []
        for f in self._futures:
            bad_k = (k in self._context for k in f.context)
            bad_v = (v in self._context.values() for v in f.context.values())
            if any(bad_k) or any(bad_v):
                assert not any(bad_k) or all(bad_k)
                assert not any(bad_v) or all(bad_v)
            else:
                newfutures.append(f)
        self._futures = newfutures

    def _simplify_futures(self):
        """Find any futures where only one future is possible, and accept them."""
        # Build a map of all the futures by the keys they provide.
        providers: dict[StructureBase, Future | None] = {}
        for f in self._futures:
            for k in f.context:
                if k not in providers:
                    providers[k] = f
                else:
                    providers[k] = None

        # Apply every future that is the only one providing a key. To keep consistent, it must be
        # the only provider for all its keys.
        lostfutures = []
        hitlist_ids = {id(f) for f in providers.values() if f is not None}
        for f in self._futures:
            if id(f) not in hitlist_ids:
                continue
            assert all(providers[k] is f for k in f.context)
            self._context.update(f.context)
            lostfutures.append(f)
        self._futures = [f for f in self._futures if id(f) not in hitlist_ids]

        self._remove_bad_futures()

        # Run any updates that were deferred until now
        for f in lostfutures:
            for a, b in f.updates:
                self._update(a, b)

    def _maybe_disambiguate(self, a: ValT, b: ValT) -> bool:
        """Promote a ?= b to a == b, if there was any doubt. In other words, deny any potential
        Future in which a != b. Returns true if a == b when all is said and done."""
        if a is None or b is None:
            return a is None and b is None
        if a not in self._context:
            self._deny_futures(lambda fc: a in fc and fc[a] is not b)
        return a in self._context and self._context[a] is b

    def _disambiguate(self, a: ValT, b: ValT):
        """Promote a ?= b to a == b and raise an AssertionError if a != b."""
        ok = self._maybe_disambiguate(a, b)
        assert ok

    def _maybe_disambiguate_keyed(
        self, a: ValT, b: ValT, *, key: tuple[dict, dict] = None, raw_base_eq: bool = False
    ) -> bool:
        """Same as _maybe_disambiguate, but understands the key keyword argument of _key_m."""
        if key is not None:
            return self._maybe_disambiguate(key[0][a], key[1][b])
        if raw_base_eq:
            return a == b
        return self._maybe_disambiguate(a, b)

    def _disambiguate_keyed(
        self, a: ValT, b: ValT, *, key: tuple[dict, dict] = None, raw_base_eq: bool = False
    ):
        """Same as _disambiguate, but understands the key keyword argument of _key_m."""
        ok = self._maybe_disambiguate_keyed(a, b, key=key, raw_base_eq=raw_base_eq)
        assert ok

    @functools.singledispatchmethod
    def _update(self, a, b):
        """Update the Diff-state presuming a == b. If it does not, update the Diff-state with a new
        set of Hunks indicating the difference. Then, recurse into the objects and attempt to match
        those as well."""
        raise NotImplementedError(f"Unable to diff type {type(a)}")

    @_update.register
    def _(self, a: types.NoneType, b: types.NoneType):
        pass

    def _key_m(self, o, *, side_a: bool, key: tuple[dict, dict] = None) -> collections.abc.Hashable:
        return self._key(key[0 if side_a else 1][o] if key is not None else o, side_a=side_a)

    @functools.singledispatchmethod
    def _key(self, o, *, side_a: bool) -> collections.abc.Hashable:
        """Derive a hashable key from the given object, that uniquely identifies it compared to its
        siblings."""
        raise NotImplementedError(f"No unique key for type {type(o)}")

    @_key.register
    def _(self, o: types.NoneType, *, side_a: bool):
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
        self, a: collections.abc.Iterable[ValT], b: collections.abc.Iterable[ValT]
    ):
        """Update the Diff-state presuming a[i] == b[i] for every possible i."""
        for ea, eb in itertools.zip_longest(a, b):
            if eb is None:
                self.removed.add(ea)
            elif ea is None:
                self.added.add(eb)
            else:
                self._update(ea, eb)

    def _isomorphic_update(  # noqa: MC0001
        self, a: collections.abc.Iterable[ValT], b: collections.abc.Iterable[ValT]
    ):
        """Update the Diff-state presuming a == b, ignoring order between the two sequences."""
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
                self._update(list(all_va)[0], list(all_vb)[0])
                continue

            # Locate any cases where a va matches with exactly one vb in practice.
            # These cases are not ambiguous and we can remove them from the comparison.
            all_matched = True
            new_all_va = set()
            for va in all_va:
                matched = None
                for vb in all_vb:
                    if not self._possible_future(va, vb):
                        all_matched = False
                        continue  # Not a match
                    if matched is None:
                        matched = vb  # First match!
                    else:
                        # Ambiguity is still present, no need to look further.
                        matched = None
                        break
                # If we have a match, _update and remove. Otherwise keep the pair.
                if matched is not None:
                    self._update(va, matched)
                    all_vb.remove(matched)
                else:
                    new_all_va.add(va)
            all_va = new_all_va
            del new_all_va

            # At this point, all_va and all_vb do not match perfectly and are ambiguous.
            # If there could still be a 1:1 mapping, record all possible futures
            if len(all_va) == len(all_vb) and all_matched:
                list_va = list(all_va)
                for list_vb in itertools.permutations(all_vb):
                    self._spawn_futures(zip(list_va, list_vb))
            else:
                # Otherwise, there's no hope for a perfect match. Consider it a full add/rm sequence.
                self.removed.update(all_va)
                self.added.update(all_vb)
        for kb, all_vb in b_dict.items():
            if kb not in a_dict:
                for vb in all_vb:
                    self.added.add(vb)

    def _delegated_update(
        self,
        a: collections.abc.Iterable[tuple[ValT, StructureBase | None]],
        b: collections.abc.Iterable[tuple[ValT, StructureBase | None]],
    ):
        """Update the Diff-state presuming that a[0] == b[0] if a[1] == b[1] in the current Diff-state."""
        b_dict = {}
        for vb, db in b:
            if db is None:
                self.added.add(vb)
                continue
            if db in b_dict:
                raise ValueError(f"Key conflict between {vb} and {b_dict[db]}")
            b_dict[db] = vb
        b_set = set(b_dict.values())

        for va, da in a:
            if da is not None:
                # Deny any futures where the delegation could potentially fail
                self._deny_futures(
                    functools.partial((lambda da, fc: da in fc and fc[da] not in b_dict), da)
                )

                # If we already have a mapping, consider it truth and delegate the comparison
                if da in self._context and self._context[da] in b_dict:
                    vb = b_dict[self._context[da]]
                    self._update(va, vb)
                    b_set.discard(vb)
                    continue

                # If we don't have a mapping yet, see if any future defines the mapping and defer
                found = False
                for f in self._futures:
                    if da in f.context:
                        vb = b_dict[f.context[da]]
                        f.defer_update(va, vb)
                        b_set.discard(vb)
                        found = True
                if found:
                    continue
                del found

            # Whatever it is, we will never get a perfect match here. Consider it a failure.
            self.removed.add(va)
        for vb in b_set:
            self.added.add(vb)

    @_update.register
    @check_fields("meta", "profile", "context", "trace")
    def _(self, a: v4.Database, b: v4.Database):
        self._context[a] = b
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
    @check_fields("General", "IdNames", "Metrics", "Files", "Modules", "Functions", "Context")
    def _(self, a: v4.metadb.MetaDB, b: v4.metadb.MetaDB):
        self._context[a] = b
        self._update(a.General, b.General)
        self._update(a.IdNames, b.IdNames)
        self._update(a.Metrics, b.Metrics)
        self._update(a.Files, b.Files)
        self._update(a.Modules, b.Modules)
        self._update(a.Functions, b.Functions)
        self._update(a.Context, b.Context)

    @_update.register
    @check_fields("title", "description")
    def _(self, a: v4.metadb.GeneralProperties, b: v4.metadb.GeneralProperties):
        if a.title == b.title and a.description == b.description:
            self._context[a] = b
        else:
            self.altered[a] = b

    @_update.register
    @check_fields("names")
    def _(self, a: v4.metadb.IdentifierNames, b: v4.metadb.IdentifierNames):
        # The order the of the names is not semantically important
        if set(a.names) == set(b.names):
            self._context[a] = b
        else:
            self.altered[a] = b

    @_update.register
    @check_fields("scopes", "metrics")
    def _(self, a: v4.metadb.PerformanceMetrics, b: v4.metadb.PerformanceMetrics):
        self._context[a] = b
        self._isomorphic_update(a.scopes, b.scopes)
        self._isomorphic_update(a.metrics, b.metrics)

    @_key.register
    @check_fields("scopeName", "type", "propagationIndex")
    def _(self, o: v4.metadb.PropagationScope, *, side_a: bool):
        _ = side_a
        # propagationIndex is an ID, not semantically important
        return (o.scopeName, o.type)

    @_update.register
    @check_fields("scopeName", "type", "propagationIndex")
    def _(self, a: v4.metadb.PropagationScope, b: v4.metadb.PropagationScope):
        assert self._key(a, side_a=True) == self._key(b, side_a=False)
        self._context[a] = b

    @_key.register
    @check_fields("name", "scopeInsts", "summaries")
    def _(self, o: v4.metadb.Metric, *, side_a: bool):
        _ = side_a
        return (o.name,)

    @_update.register
    @check_fields("name", "scopeInsts", "summaries")
    def _(self, a: v4.metadb.Metric, b: v4.metadb.Metric):
        assert self._key(a, side_a=True) == self._key(b, side_a=False)
        self._context[a] = b
        self._isomorphic_update(a.scopeInsts, b.scopeInsts)
        self._isomorphic_update(a.summaries, b.summaries)

    @_key.register
    @check_fields("scope", "propMetricId")
    def _(self, o: v4.metadb.PropagationScopeInstance, *, side_a: bool):
        # propMetricId is an ID, not semantically important
        return (self._key(o.scope, side_a=side_a),)

    @_update.register
    @check_fields("scope", "propMetricId")
    def _(self, a: v4.metadb.PropagationScopeInstance, b: v4.metadb.PropagationScopeInstance):
        assert self._key(a, side_a=True) == self._key(b, side_a=False)
        self._disambiguate(a.scope, b.scope)
        self._context[a] = b

    @_key.register
    @check_fields("scope", "formula", "combine", "statMetricId")
    def _(self, o: v4.metadb.SummaryStatistic, *, side_a: bool):
        # statMetricId is an ID, not semantically important
        return (self._key(o.scope, side_a=side_a), o.formula, o.combine)

    @_update.register
    @check_fields("scope", "formula", "combine", "statMetricId")
    def _(self, a: v4.metadb.SummaryStatistic, b: v4.metadb.SummaryStatistic):
        assert self._key(a, side_a=True) == self._key(b, side_a=False)
        self._disambiguate(a.scope, b.scope)
        self._context[a] = b

    @_update.register
    @check_fields("files")
    def _(self, a: v4.metadb.SourceFiles, b: v4.metadb.SourceFiles):
        self._context[a] = b
        self._isomorphic_update(a.files, b.files)

    @_key.register
    @check_fields("flags", "path")
    def _(self, o: v4.metadb.File, *, side_a: bool):
        _ = side_a
        return (o.flags, o.path)

    @_update.register
    @check_fields("flags", "path")
    def _(self, a: v4.metadb.File, b: v4.metadb.File):
        assert self._key(a, side_a=True) == self._key(b, side_a=False)
        self._context[a] = b

    @_update.register
    @check_fields("modules")
    def _(self, a: v4.metadb.LoadModules, b: v4.metadb.LoadModules):
        self._context[a] = b
        self._isomorphic_update(a.modules, b.modules)

    @_key.register
    @check_fields("flags", "path")
    def _(self, o: v4.metadb.Module, *, side_a: bool):
        _ = side_a
        return (o.flags, o.path)

    @_update.register
    @check_fields("flags", "path")
    def _(self, a: v4.metadb.Module, b: v4.metadb.Module):
        assert self._key(a, side_a=True) == self._key(b, side_a=False)
        self._context[a] = b

    @_update.register
    @check_fields("functions")
    def _(self, a: v4.metadb.Functions, b: v4.metadb.Functions):
        self._context[a] = b
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
        assert self._key(a, side_a=True) == self._key(b, side_a=False)
        self._disambiguate(a.module, b.module)
        self._disambiguate(a.file, b.file)
        self._context[a] = b

    @_update.register
    @check_fields("entryPoints")
    def _(self, a: v4.metadb.ContextTree, b: v4.metadb.ContextTree):
        self._context[a] = b
        self._isomorphic_update(a.entryPoints, b.entryPoints)

    @_key.register
    @check_fields("ctxId", "entryPoint", "prettyName", "children")
    def _(self, o: v4.metadb.EntryPoint, *, side_a: bool):
        _ = side_a
        # ctxId is an ID, not semantically important
        return (o.entryPoint,)

    @_update.register
    @check_fields("ctxId", "entryPoint", "prettyName", "children")
    def _(self, a: v4.metadb.EntryPoint, b: v4.metadb.EntryPoint):
        assert self._key(a, side_a=True) == self._key(b, side_a=False)
        if a.prettyName == b.prettyName:
            self._context[a] = b
        else:
            self.altered[a] = b
        self._isomorphic_update(a.children, b.children)

    @_key.register
    @check_fields(
        "ctxId",
        "flags",
        "relation",
        "lexicalType",
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
            o.lexicalType,
            self._key(o.function, side_a=side_a),
            self._key(o.file, side_a=side_a),
            o.line,
            self._key(o.module, side_a=side_a),
            o.offset,
        )

    @_update.register
    @check_fields(
        "ctxId",
        "flags",
        "relation",
        "lexicalType",
        "propagation",
        "function",
        "file",
        "line",
        "module",
        "offset",
        "children",
    )
    def _(self, a: v4.metadb.Context, b: v4.metadb.Context):
        assert self._key(a, side_a=True) == self._key(b, side_a=False)
        self._disambiguate(a.function, b.function)
        self._disambiguate(a.file, b.file)
        self._disambiguate(a.module, b.module)
        # TODO: propagation
        self._context[a] = b
        self._isomorphic_update(a.children, b.children)

    # ========================
    #   formats.v4.profiledb
    # ========================

    @_update.register
    @check_fields("ProfileInfos")
    def _(self, a: v4.profiledb.ProfileDB, b: v4.profiledb.ProfileDB):
        self._context[a] = b
        self._update(a.ProfileInfos, b.ProfileInfos)

    @_update.register
    @check_fields("profiles")
    def _(self, a: v4.profiledb.ProfileInfos, b: v4.profiledb.ProfileInfos):
        self._context[a] = b
        self._isomorphic_update(a.profiles, b.profiles)

    @_key.register
    @check_fields("idTuple", "flags", "values")
    def _(self, o: v4.profiledb.Profile, *, side_a: bool):
        return (self._key(o.idTuple, side_a=side_a), o.flags)

    @_update.register
    @check_fields("idTuple", "flags", "values")
    def _(self, a: v4.profiledb.Profile, b: v4.profiledb.Profile):
        assert self._key(a, side_a=True) == self._key(b, side_a=False)
        self._context[a] = b
        self._update(a.idTuple, b.idTuple)

    @_key.register
    @check_fields("ids")
    def _(self, o: v4.profiledb.IdentifierTuple, *, side_a: bool):
        return tuple(self._key(i, side_a=side_a) for i in o.ids)

    @_update.register
    @check_fields("ids")
    def _(self, a: v4.profiledb.IdentifierTuple, b: v4.profiledb.IdentifierTuple):
        self._context[a] = b
        self._sequential_update(a.ids, b.ids)

    @_key.register
    @check_fields("kind", "flags", "logicalId", "physicalId")
    def _(self, o: v4.profiledb.Identifier, *, side_a: bool):
        key = (self.a.kind_map, self.b.kind_map) if isinstance(self.a, v4.Database) else None
        return (
            self._key_m(o.kind, side_a=side_a, key=key),
            o.flags,
            o.logicalId,
            o.physicalId,
        )

    @_update.register
    @check_fields("kind", "flags", "logicalId", "physicalId")
    def _(self, a: v4.profiledb.Identifier, b: v4.profiledb.Identifier):
        if self._key(a, side_a=True) == self._key(b, side_a=False):
            self._context[a] = b
        else:
            self.altered[a] = b

    # ====================
    #   formats.v4.cctdb
    # ====================

    @_update.register
    @check_fields("CtxInfos")
    def _(self, a: v4.cctdb.ContextDB, b: v4.cctdb.ContextDB):
        self._context[a] = b
        self._update(a.CtxInfos, b.CtxInfos)

    @_update.register
    @check_fields("contexts")
    def _(self, a: v4.cctdb.ContextInfos, b: v4.cctdb.ContextInfos):
        self._context[a] = b
        if isinstance(self.a, v4.Database):
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
        self._context[a] = b

    # ======================
    #   formats.v4.tracedb
    # ======================

    @_update.register
    @check_fields("CtxTraces")
    def _(self, a: v4.tracedb.TraceDB, b: v4.tracedb.TraceDB):
        self._context[a] = b
        self._update(a.CtxTraces, b.CtxTraces)

    @_update.register
    @check_fields("timestampRange", "traces")
    def _(self, a: v4.tracedb.ContextTraceHeadersSection, b: v4.tracedb.ContextTraceHeadersSection):
        if a.timestampRange == b.timestampRange:
            self._context[a] = b
        else:
            self.altered[a] = b
        self._isomorphic_update(a.traces, b.traces)

    @_key.register
    @check_fields("profIndex", "line")
    def _(self, o: v4.tracedb.ContextTrace, *, side_a: bool):
        key = (self.a.profile_map, self.b.profile_map) if isinstance(self.a, v4.Database) else None
        return (self._key_m(o.profIndex, side_a=side_a, key=key),)

    @_update.register
    @check_fields("profIndex", "line")
    def _(self, a: v4.tracedb.ContextTrace, b: v4.tracedb.ContextTrace):
        assert self._key(a, side_a=True) == self._key(b, side_a=False)
        key = (self.a.profile_map, self.b.profile_map) if isinstance(self.a, v4.Database) else None
        self._disambiguate_keyed(a.profIndex, b.profIndex, key=key, raw_base_eq=True)
        self._context[a] = b
        self._sequential_update(a.line, b.line)

    @_update.register
    @check_fields("timestamp", "ctxId")
    def _(self, a: v4.tracedb.ContextTraceElement, b: v4.tracedb.ContextTraceElement):
        key = (self.a.context_map, self.b.context_map) if isinstance(self.a, v4.Database) else None
        if a.timestamp == b.timestamp and self._maybe_disambiguate_keyed(
            a.ctxId, b.ctxId, key=key, raw_base_eq=True
        ):
            self._context[a] = b
        else:
            self.altered[a] = b


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
        best_failed_cnt, best_total_cnt, best_failures = float("inf"), 0, {}
        for a2b in diff.contexts():
            self._diffcontext = a2b
            self.failed_cnt, self.total_cnt, self.failures = 0, 0, {}
            for a, b in a2b.items():
                self._compare(a, b)
            if self.failed_cnt < best_failed_cnt and self.total_cnt >= best_total_cnt:
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

    def render(self, out: io.TextIOBase):
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
                print(f"  Details of failures{suffix}:", file=out)
            else:
                print(f"  Details of 30 worst failures{suffix}:", file=out)
            for key in fails[:30]:
                a, b, diff = key
                paths = self.failures[key]

                if diff == self._cmp_bad_sign:
                    why = "values have differing signs"
                elif diff == self._cmp_exp_diff:
                    why = "values differ by more than a power of 2"
                else:
                    exp_a, exp_b = math.frexp(abs(a))[1], math.frexp(abs(b))[1]
                    exp = (
                        f"{exp_a:d}"
                        if exp_a == exp_b
                        else f"({min(exp_a,exp_b):d}|{max(exp_a,exp_b):d})"
                    )
                    why = f"difference of {diff:f} * 2^{exp} = {diff*self._norm2ulp} ULPs"
                print(f"  - {a:f} != {b:f}: {why} (abs diff {abs(a-b):f})", file=out)
                print(
                    "   At: "
                    + "\n".join(
                        textwrap.wrap(" ".join(paths), width=100, subsequent_indent=" " * 5)
                    ),
                    file=out,
                )

    _cmp_bad_sign: T.ClassVar[float] = 510.0
    _cmp_exp_diff: T.ClassVar[float] = 500.0

    def _float_cmp(self, a: float, b: float) -> float:
        """Compare two floats and return 0.0 if the two are equal. If not, returns the
        normalized difference or a _cmp_* constant indicating the error."""
        # Shortcut for true equality
        if a == b:
            return 0.0
        # Different signs are always different, no matter what
        if math.copysign(1.0, a) != math.copysign(1.0, b):
            return self._cmp_bad_sign

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
            return self._cmp_exp_diff
        assert b_e - a_e == 1

        # The values are in neighboring exponents. Calculate the normalized difference by summing
        # the parts of the difference on each "side" of the exponent boundary.
        diff = (1.0 - a_m) + (b_m - 0.5)
        return 0.0 if diff <= self._grace else diff

    def _base_compare(self, a: float, b: float, *path: str | tuple[int, int]) -> None:
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

    ObjT = T.TypeVar("ObjT")

    def _zip_by_id(
        self,
        a: dict[int, T.Any],
        b: dict[int, T.Any],
        id2a: dict[int, ObjT] | None,
        b2id: T.Callable[[ObjT], int] | None,
    ):
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
        ctx_map, met_map, met_b2id = [None] * 3
        if isinstance(self.diff.a, v4.Database):
            ctx_map = self.diff.a.context_map
            met_map = (
                self.diff.a.summary_metric_map
                if a.Flags.isSummary in a.flags
                else self.diff.a.raw_metric_map
            )
            met_b2id = (
                (lambda m: m.statMetricId)
                if b.Flags.isSummary in b.flags
                else (lambda m: m.propMetricId)
            )
            i = self.diff.a.profile.ProfileInfos.profiles.index(
                a
            ), self.diff.b.profile.ProfileInfos.profiles.index(b)
        else:
            assert isinstance(self.diff.a, v4.profiledb.ProfileDB)
            i = self.diff.a.ProfileInfos.profiles.index(a), self.diff.b.ProfileInfos.profiles.index(
                b
            )
        for j, x, y in self._zip_by_id(a.values, b.values, ctx_map, lambda c: c.ctxId):
            for k, v_a, v_b in self._zip_by_id(x, y, met_map, met_b2id):
                self._base_compare(v_a, v_b, "profiles", i, j, k)

    @_compare.register
    def _(self, a: v4.cctdb.PerContext, b: v4.cctdb.PerContext):
        prof_map, prof_b2id, met_map = [None] * 3
        if isinstance(self.diff.a, v4.Database):
            prof_map = self.diff.a.profile_map
            prof_b2id = self.diff.b.profile.ProfileInfos.profiles.index
            met_map = self.diff.a.raw_metric_map
            i = self.diff.a.context.CtxInfos.contexts.index(
                a
            ), self.diff.b.context.CtxInfos.contexts.index(b)
        else:
            assert isinstance(self.diff.a, v4.cctdb.ContextDB)
            i = self.diff.a.CtxInfos.contexts.index(a), self.diff.b.CtxInfos.contexts.index(b)
        for j, x, y in self._zip_by_id(a.values, b.values, met_map, lambda m: m.propMetricId):
            for k, v_a, v_b in self._zip_by_id(x, y, prof_map, prof_b2id):
                self._base_compare(v_a, v_b, "contexts", i, j, k)
