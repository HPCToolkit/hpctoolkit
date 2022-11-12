import abc
import graphlib
import typing as T
from collections.abc import Iterable
from pathlib import Path

from .configuration import Configuration
from .logs import FgColor, colorize_str


class ActionResult(abc.ABC):
    """The result of running an Action. Returned by Action.run(...)."""

    @abc.abstractmethod
    def summary(self) -> str:
        """Return a short one-line summary string for this result."""

    @property
    @abc.abstractmethod
    def flawless(self) -> bool:
        """Return True if the Action finished without any errors or warnings."""

    @property
    @abc.abstractmethod
    def passed(self) -> bool:
        """Return False if the Action finished with errors that fail the run as a whole."""

    @property
    @abc.abstractmethod
    def completed(self) -> bool:
        """Return False if the Action was unable to complete its task, and dependent Actions should not run."""


def summarize_results(results: Iterable[ActionResult], prefix: str = "") -> None:
    """Print the summaries of a list of ActionResults to the output. Colorized to indicate status."""
    for r in results:
        assert isinstance(r, ActionResult)

        color = None
        if r.flawless:
            color = FgColor.flawless
        elif r.passed:
            color = FgColor.warning
        else:
            color = FgColor.error

        if color is not None:
            s = r.summary().strip()
            if s:
                print(colorize_str(color, prefix + s))


class ReturnCodeResult(ActionResult):
    """Simple ActionResult that is successful or not based on an integer process returncode"""

    def __init__(self, cmdname: str, returncode: int):
        self._cmdname = cmdname
        self._returncode = returncode

    def summary(self):
        return (
            f"{self._cmdname} was successful"
            if self._returncode == 0
            else f"{self._cmdname} failed with code {self._returncode:d}"
        )

    @property
    def flawless(self):
        return self.completed

    @property
    def passed(self):
        return self.completed

    @property
    def completed(self):
        return self._returncode == 0


class Action(abc.ABC):
    """
    ABC for the Actions the build front-end can take, for instance configuring or building the
    project. These share a small set of arguments but are otherwise very generic.

    These are always singletons, calling Action() will return the singleton object.
    """

    __singleton = None

    def __init_subclass__(cls, **kwargs):
        super().__init_subclass__(**kwargs)
        init = cls.__init__

        def newinit(self, *args, **kwargs):
            if self.__singleton is None:  # noqa: protected-access
                init(self, *args, **kwargs)

        cls.__init__ = newinit

    def __new__(cls):
        """Return the singleton Action object."""
        if cls.__singleton is None:
            value = super().__new__(cls)
            value.__init__()
            cls.__singleton = value
        return cls.__singleton

    def header(self, cfg: Configuration) -> str:  # noqa: unused-argument
        """Header for this Action's operations. To be printed to the log as a status message."""
        return self.name()

    @abc.abstractmethod
    def name(self) -> str:
        """Name for this Action, as a noun. To be used in sentences in the log"""

    @abc.abstractmethod
    def dependencies(self) -> tuple["Action"]:
        """List of other Actions that need to run before this one."""

    @abc.abstractmethod
    def run(
        self,
        cfg: Configuration,
        *,
        builddir: Path,
        srcdir: Path,
        installdir: Path,
        logdir: T.Optional[Path] = None,
    ) -> ActionResult:
        """Run this Action with the given arguments."""


def action_sequence(actions: Iterable[Action]) -> list[Action]:
    """Given a list of Actions, expand all dependencies and sort in topological order.
    Returns the final list of Actions in the order they should be executed."""

    # Expand all dependencies, as much as possible
    graph = {}

    def expand(action):
        if len(graph) > 100:
            raise RuntimeError("Action-graph is too large, aborting infinite loop!")
        if action not in graph:
            graph[action] = action.dependencies()
            for d in graph[action]:
                expand(d)

    for a in actions:
        expand(a)

    # Run a topo sort on the graph and return the result
    return list(graphlib.TopologicalSorter(graph).static_order())
