import abc
import contextlib
import enum
import sys
import time

_section_counter = 0


class FgColor(enum.Enum):
    """Selection of foreground color codes to indicate various statuses"""

    # Selected subset that seem to work well in practical cases
    header = 36  # Cyan
    error = 91  # Bright red
    warning = 33  # Yellow
    flawless = 32  # Green
    info = 90  # Bright black / Grey


@contextlib.contextmanager
def colorize(color):
    """Context manager to colorize any text printed to stdout"""
    sys.stdout.write("\x1b[" + str(FgColor(color).value) + "m")
    try:
        yield
    finally:
        sys.stdout.write("\x1b[0m")


def colorize_str(color, string):
    """Wrap a string in the appropriate color codes for the given color"""
    return "\x1b[" + str(FgColor(color).value) + "m" + str(string) + "\x1b[0m"


@contextlib.contextmanager
def section(header, collapsed=False, color=None):
    """Wrap any text printed to stdout in this context manager as a GitLab collapsible section"""
    global _section_counter
    _section_counter += 1
    codename = "section_" + str(_section_counter)
    opts = "[collapsed=true]" if collapsed else ""
    if color is not None:
        header = colorize_str(color, header)
    print(f"\x1b[0Ksection_start:{int(time.time())}:{codename}{opts}\r\x1b[0K{header}")
    try:
        yield
    finally:
        print(f"\x1b[0Ksection_end:{int(time.time())}:{codename}\r\x1b[0K")


def dump_file(filename, limit_bytes=10 * 1024):
    """Dump the given file to stdout, stopping at the given limit if not None"""
    size = 0
    with open(filename, encoding="utf-8") as f:
        for line in f:
            size += len(line)
            if limit_bytes is not None and size > limit_bytes:
                with colorize(FgColor.header):
                    print(f"=== Truncated for space, see {filename} for complete log ===")
                break
            print(line.rstrip("\n"))


def print_header(line):
    with colorize(FgColor.header):
        print(line)
    sys.stdout.flush()


class AbstractStatusResult(abc.ABC):
    """Base class for all results that have a printable status"""

    @abc.abstractmethod
    def summary(self):
        """Return a short summary string for this result, or the empty string if self.flawless"""

    @property
    @abc.abstractmethod
    def flawless(self):
        """Return True if this result finished without any errors or warnings"""


def print_results(*results, prefix=""):
    for r in results:
        assert isinstance(r, AbstractStatusResult)
        s = r.summary()
        if s:
            print(str(prefix) + s.strip())
