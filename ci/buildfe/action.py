import abc
import graphlib
from collections.abc import Iterable
from pathlib import Path

from .configuration import Configuration
from .logs import FgColor


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

    def color(self) -> FgColor:
        """Return the standard color that represents this result's state."""
        if not self.completed or not self.passed:
            return FgColor.error
        if not self.flawless:
            return FgColor.warning
        return FgColor.flawless


class SummaryResult(ActionResult):
    """Simple ActionResult that combines the worst of all its inputs."""

    def __init__(self):
        self._results = []

    def add(self, action: "Action", result: ActionResult):
        self._results.append((action, result))

    def summary(self) -> str:
        if not self.completed:
            return "Did not complete " + " ".join(
                a.name() for a, r in self._results if not r.completed
            )
        if not self.passed:
            return "Failed in " + " ".join(a.name() for a, r in self._results if not r.passed)
        if not self.flawless:
            return "Passed with warnings in " + " ".join(
                a.name() for a, r in self._results if not r.flawless
            )
        return "Passed with no warnings"

    def icon(self) -> str:
        if not self.completed:
            return "\U0001f4a5"  # 💥
        if not self.passed:
            return "\u274c"  # ❌
        if not self.flawless:
            return "\U0001f315"  # 🌕
        return "\u2714"  # ✔

    @property
    def flawless(self):
        return all(r.flawless for a, r in self._results)

    @property
    def passed(self):
        return all(r.passed for a, r in self._results)

    @property
    def completed(self):
        return all(r.completed for a, r in self._results)


class ReturnCodeResult(ActionResult):
    """Simple ActionResult that is successful or not based on an integer process returncode."""

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
    """ABC for the Actions the build front-end can take, for instance configuring or building the
    project. These share a small set of arguments but are otherwise very generic.

    These are always singletons, calling Action() will return the singleton object.
    """

    __singleton = None

    def __init_subclass__(cls, **kwargs):
        super().__init_subclass__(**kwargs)
        init = cls.__init__

        def newinit(self, *args, **kwargs):
            if self.__singleton is None:  # pylint: disable=protected-access
                init(self, *args, **kwargs)

        cls.__init__ = newinit  # type: ignore[assignment]

    def __new__(cls):
        """Return the singleton Action object."""
        if cls.__singleton is None:
            value = super().__new__(cls)
            value.__init__()  # type: ignore[misc]
            cls.__singleton = value
        return cls.__singleton

    def header(self, cfg: Configuration) -> str:  # pylint: disable=unused-argument
        """Header for this Action's operations. To be printed to the log as a status message."""
        return self.name()

    @abc.abstractmethod
    def name(self) -> str:
        """Name for this Action, as a noun. To be used in sentences in the log."""

    @abc.abstractmethod
    def dependencies(self) -> tuple["Action", ...]:
        """List of other Actions that need to run before this one."""

    @abc.abstractmethod
    def run(
        self,
        cfg: Configuration,
        *,
        builddir: Path,
        srcdir: Path,
        installdir: Path,
        logdir: Path | None = None,
    ) -> ActionResult:
        """Run this Action with the given arguments."""


def action_sequence(actions: Iterable[Action]) -> list[Action]:
    """Given a list of Actions, expand all dependencies and sort in topological order.
    Returns the final list of Actions in the order they should be executed.
    """
    # Expand all dependencies, as much as possible
    graph: dict[Action, tuple[Action, ...]] = {}

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
