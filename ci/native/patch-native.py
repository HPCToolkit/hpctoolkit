#!/bin/sh
# The following is run by sh and ignored by Python.
""":"
for trial in python3.12 python3.11 python3.10 python3.9 python3.8 python3 python; do
  if command -v "$trial" > /dev/null 2>&1; then
    exec "$trial" "$0" "$@"
  fi
done
exit 127
"""  # noqa: D400, D415

# pylint: disable=invalid-name

import configparser
import os.path
import re
import string
import sys

if __name__ == "__main__":
    a = configparser.ConfigParser(interpolation=None)
    if not os.path.exists(sys.argv[1]):
        raise ValueError(f"Not a file: {sys.argv[1]}")
    a.read(sys.argv[1], encoding="utf-8")
    b = configparser.ConfigParser(interpolation=None)
    if not os.path.exists(sys.argv[2]):
        raise ValueError(f"Not a file: {sys.argv[2]}")
    b.read(sys.argv[2], encoding="utf-8")

    for section in b.sections():
        if not a.has_section(section):
            a.add_section(section)
            for opt, val in b.items(section):
                a.set(section, opt, val)
        else:
            for opt, val in b.items(section):
                if not a.has_option(section, opt):
                    a.set(section, opt, val)
                else:
                    aval = a.get(section, opt)
                    if not re.match(r"\s*\[", val):
                        if re.match(r"\s*\[", aval):
                            raise ValueError(
                                f"Attempt to override list with non-list: {aval!r} + {val!r}"
                            )
                        a.set(section, opt, val)
                    else:
                        if not re.match(r"\s*\[", aval):
                            raise ValueError(
                                f"Unable to merge list into non-list: {aval!r} + {val!r}"
                            )
                        a.set(
                            section,
                            opt,
                            f"{aval.rstrip(string.whitespace + '],')}, {val.lstrip(string.whitespace + '[')}",
                        )

    with open(sys.argv[1], "w", encoding="utf-8") as f:
        a.write(f, space_around_delimiters=True)
