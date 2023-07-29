import contextlib
import enum
import random
import sys
import time

_section_counter = 0


class FgColor(enum.Enum):
    """Selection of foreground color codes to indicate various statuses."""

    # Selected subset that seem to work well in practical cases
    header = 36  # Cyan
    error = 91  # Bright red
    warning = 33  # Yellow
    flawless = 32  # Green
    info = 37  # White


@contextlib.contextmanager
def colorize(color):
    """Context manager to colorize any text printed to stdout."""
    sys.stdout.write(f"\x1b[{FgColor(color).value}m")
    try:
        yield
    finally:
        sys.stdout.write("\x1b[0m")


def colorize_str(color, string):
    """Wrap a string in the appropriate color codes for the given color."""
    return "\x1b[" + str(FgColor(color).value) + "m" + str(string) + "\x1b[0m"


@contextlib.contextmanager
def section(header, collapsed=False, color=None):
    """Wrap any text printed to stdout in this context manager as a GitLab collapsible section."""
    global _section_counter  # noqa: PLW0603
    _section_counter += 1
    codename = "section_" + str(_section_counter) + "_" + random.randbytes(8).hex()
    opts = "[collapsed=true]" if collapsed else ""
    if color is not None:
        header = colorize_str(color, header)
    print(
        f"\x1b[0Ksection_start:{int(time.monotonic())}:{codename}{opts}\r\x1b[0K{header}",
        flush=True,
    )
    try:
        yield
    finally:
        print(f"\x1b[0Ksection_end:{int(time.monotonic())}:{codename}\r\x1b[0K", end="", flush=True)


def dump_file(filename, limit_bytes=10 * 1024, tail: bool = True):
    """Dump the given file to stdout, stopping at the given limit if not None.
    If tail is True, dump the end of the file (instead of the start).
    """
    buffer, size, truncated = [], 0, False
    with open(filename, encoding="utf-8") as f:
        for line in f:
            buffer.append(line)
            size += len(line)
            if limit_bytes is not None and size > limit_bytes:
                truncated = True
                while size > limit_bytes:
                    size -= len(buffer.pop(0))

    if truncated and tail:
        with colorize(FgColor.header):
            print("=== Truncated for space, printing tail end of log ===")
    for line in buffer:
        print(line.rstrip("\n"))
    if truncated:
        with colorize(FgColor.header):
            if not tail:
                print(f"=== Truncated for space, see {filename} for complete log ===")
            else:
                print(f"=== See {filename} for complete log ===")


def print_header(line):
    with colorize(FgColor.header):
        print(line)
    sys.stdout.flush()
