# SPDX-FileCopyrightText: 2003-2024 Rice University
#
# SPDX-License-Identifier: BSD-3-Clause

import dataclasses
import typing


def check_fields(*names: str, cls: typing.Optional[type] = None):
    expected = frozenset(names)
    if cls is None:

        def apply(func):
            hints = typing.get_type_hints(func)
            cls = hints[next(iter(hints.keys()))]
            got = frozenset(f.name for f in dataclasses.fields(cls))
            assert (
                got == expected
            ), f"Missing fields {got-expected!r}, extra fields {expected-got!r}"
            return func

        return apply

    got = frozenset(f.name for f in dataclasses.fields(cls))
    assert got == expected, f"Missing fields {got-expected!r}, extra fields {expected-got!r}"
    return lambda x: x
