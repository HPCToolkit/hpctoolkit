import abc
import collections.abc
import typing

from .. import base

__all__ = ["DiffStrategy", "DiffHunk", "AccuracyStrategy"]


class DiffHunk:
    """One or more related differences identified by a DiffStrategy."""

    # TODO: Come up with a unified API that all Hunks are able to provide


DbT = typing.TypeVar("DbT", bound=base.DatabaseBase | base.DatabaseFile)


class DiffStrategy(abc.ABC):
    """Strategy for identifying differences between 2 performance databases."""

    def __init__(self, a: DbT, b: DbT):
        """Compare two database objects (a and b) or individual database files and identify
        differences between them.
        """
        super().__init__()
        if not isinstance(a, type(b)) and not isinstance(b, type(a)):
            raise TypeError(type(a), type(b))
        self.a, self.b = a, b

    @abc.abstractmethod
    def contexts(self) -> collections.abc.Iterable[collections.abc.Mapping]:
        """Return the similar parts between a and b, i.e. the context for any differences.
        There may be multiple, this method returns all of them, "best" ones first.

        Objects in this mapping may not be identical, but are "similar enough" for the
        DiffStrategy to consider them as context for other differences.
        """

    def best_context(self) -> collections.abc.Mapping:
        """Return the similar parts between a and b, i.e. the context for any differences.
        There may be multiple, this returns only the "best" option.

        Objects in this mapping may not be identical, but are "similar enough" for the
        DiffStrategy to consider them as context for other differences.
        """
        return next(iter(self.contexts()))

    @property
    @abc.abstractmethod
    def hunks(self) -> collections.abc.Collection[DiffHunk]:
        """Return a series of all DiffHunks indicating differences between a and b.

        Note that these DiffHunks may be available through other methods as well, this just
        provides a consistent interface to iterate over all of them.
        """

    @abc.abstractmethod
    def render(self, out: typing.TextIO) -> None:
        """Render this Diff into a human-readable textual representation and write it to out."""


class AccuracyStrategy(abc.ABC):
    """Strategy for assessing numeric accuracy between 2 performance databases.

    Requires an initial comparison with a DiffStrategy to identify similarities by which to compare
    numeric accuracy.
    """

    def __init__(self, diff: DiffStrategy):
        """Assess the accuracy between 2 databases, based on the given Diff. Note that accuracy is
        only calculated between the objects listed in one of `diff.contexts()`.

        If the assessment used is not symmetric, `diff.a` is considered the ground-truth or expected
        result, and `diff.b` the obtained or tested result.
        """
        super().__init__()
        if not isinstance(diff, DiffStrategy):
            raise TypeError(type(diff))
        self.diff = diff

    @property
    @abc.abstractmethod
    def inaccuracy(self) -> float:
        """Return a single value (between 0 and 1) indicating the total degree of inaccuracy.

        An inaccuracy of 0 indicates that the values are, for the intents and purposes specified by
        the AccuracyStrategy, identical. In particular, this allows for logic such as:
            if Accuracy(diff).inaccuracy:
                raise ValueError("Failed the test, values are inaccurate!")

        The maximum inaccuracy is normalized to 1, although exactly what this means is
        defined by the subclasses. Some ideas are:
          - All values differ by at least a constant or relative threshold, or
          - The relative factors between values in diff.a differ from those in diff.b, or
          - Values result in different cost-orderings for sibling contexts, or
          - The statistical probability of diff.b (using diff.a as reference) is over
            some predefined threshold.
        """

    @abc.abstractmethod
    def render(self, out: typing.TextIO) -> None:
        """Render this Accuracy assessment into a human-readable textual representation and write it
        to out.
        """
