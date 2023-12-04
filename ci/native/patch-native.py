import configparser
import re
import sys

if __name__ == "__main__":
    a = configparser.ConfigParser(interpolation=None)
    a.read(sys.argv[1], encoding="utf-8")
    b = configparser.ConfigParser(interpolation=None)
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
                        a.set(section, opt, f"{aval}\n+ {val}")

    with open(sys.argv[1], "w", encoding="utf-8") as f:
        a.write(f, space_around_delimiters=True)
