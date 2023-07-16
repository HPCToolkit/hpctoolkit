import base64
import collections
import contextlib
import dataclasses
import itertools
import json
import os
import re
import shlex
import shutil
import stat
import subprocess
import tempfile
import traceback
import typing
import venv
from pathlib import Path

import click
import jsonschema
import ruamel.yaml

from . import schema
from .buildfe import logs
from .buildfe._main import main as buildfe_main
from .command import Command
from .envs import AutogenEnv, DevEnv, InvalidSpecificationError
from .spack.system import OSClass, SystemCompiler
from .spack.system import translate as os_translate
from .spec import DependencyMode, SpackEnv

__all__ = ("main",)


class SystemCompilerType(click.ParamType):
    name = "Compiler"

    def __init__(self, *, missing_ok: bool = False) -> None:
        self._missing_ok = missing_ok

    def convert(
        self, value: str | SystemCompiler, param: click.Parameter | None, ctx: click.Context | None
    ) -> SystemCompiler:
        if isinstance(value, SystemCompiler):
            return value

        try:
            result = SystemCompiler(value)
        except ValueError as e:
            self.fail(str(e), param, ctx)
        if not result and not self._missing_ok:
            self.fail(f"Unable to find compiler: {result}", param, ctx)
        return result


@dataclasses.dataclass(kw_only=True)
class DevState:
    # Root of the HPCToolkit checkout, if it exists.
    project_root: Path | None = None

    # Directory containing named environments, if named environments are enabled.
    # May not exist, create this directory lazily when needed.
    named_environment_root: Path | None = None

    # Whether to print the traceback of caught exceptions before converting to ClickException.
    traceback: bool = False

    _MISSING_PROJECT_ROOT_SOLUTIONS = """\
Potential solutions:
    1. cd to the HPCToolkit project root directory.
    2. Use the ./dev wrapper script in the project root directory.
    3. Ensure './configure --version' works properly."""

    def missing_named_why(self) -> str:
        """Give a suitable (short) reason why named_environment_root is None."""
        if self.project_root is None:
            return f"no HPCToolkit checkout was found\n{self._MISSING_PROJECT_ROOT_SOLUTIONS}"
        return "named devenvs disabled by --isolated"

    @contextlib.contextmanager
    def internal_named_env(
        self, name: str, noun: str, option: Path | None = None
    ) -> typing.Iterator[Path]:
        """Generate a (possibly temporary) directory in the named_environment_root."""
        if option is not None:
            yield option
        elif self.named_environment_root is not None:
            yield (self.named_environment_root / f"_{name}").resolve()
        else:
            click.echo(f"NOTE: Using emphermal {noun} for {name}")
            with tempfile.TemporaryDirectory(prefix="dev-", suffix=f"-{name}") as result:
                yield Path(result).resolve()

    @property
    def meson(self) -> Command:
        with self.internal_named_env("meson", "venv") as ve_dir:
            venv.EnvBuilder(with_pip=True, upgrade_deps=True, prompt="dev/meson").create(ve_dir)
            vpython = Command(ve_dir / "bin" / "python3")
            vpython("-m", "pip", "install", "--require-virtualenv", "meson[ninja]>=1.1.0,<2")

            os.environ["NINJA"] = str(ve_dir / "bin" / "ninja")
            return Command(ve_dir / "bin" / "meson")


dev_pass_obj = click.make_pass_decorator(DevState, ensure=True)

GITIGNORE = """\
# File created by the HPCToolkit ./dev script
/**
"""

P = typing.ParamSpec("P")
T = typing.TypeVar("T")


class Env(typing.NamedTuple):
    root: Path
    args: tuple[str, ...]


class DevEnvType(click.ParamType):
    """ParamType for a devenv referenced by Path."""

    name = "devenv"

    def __init__(
        self, /, *, exists: bool = False, notexists: bool = False, flag: str | None
    ) -> None:
        self._check_exists = exists
        self._check_notexists = notexists
        self._flag = flag

    def convert(
        self, value: Env | str, param: click.Parameter | None, ctx: click.Context | None
    ) -> Env:
        if isinstance(value, Env):
            return value
        envpath = Path(value)
        del value

        env = Env(envpath, (self._flag, envpath.name) if self._flag else (envpath.name,))
        if self._check_exists and not env.root.exists():
            self.fail(f"Selected devenv does not exist: {env.root}", param, ctx)
        if env.root.exists() and not env.root.is_dir():
            self.fail(
                f"Selected devenv already exists as a file, remove and try again: {env.root}",
                param,
                ctx,
            )
        if self._check_notexists and env.root.exists():
            click.confirm(f"Selected devenv already exists, remove {env.root}?", abort=True)
            shutil.rmtree(env.root)

        if ctx is not None:
            obj = ctx.ensure_object(DevState)
            if obj.named_environment_root and env.root.parent == obj.named_environment_root:
                with (obj.named_environment_root / ".gitignore").open("w", encoding="utf-8") as gi:
                    gi.write(GITIGNORE)

        return env


class DevEnvByName(click.ParamType):
    """ParamType for a named devenv, stored under `.devenv/`.

    This concept only exists in the CLI, the backend classes use full Paths instead.
    """

    name = "devenv name"

    def __init__(
        self, /, *, exists: bool = False, notexists: bool = False, flag: str | None
    ) -> None:
        self._check_exists = exists
        self._check_notexists = notexists
        self._flag = flag

    def convert(
        self, value: Env | str, param: click.Parameter | None, ctx: click.Context | None
    ) -> Env:
        if isinstance(value, Env):
            return value
        envpath = Path(value)
        del value

        if ctx is None:
            raise AssertionError("Named devenvs cannot be used in Click prompts and such")
        if not (obj := ctx.ensure_object(DevState)).named_environment_root:
            self.fail(f"Named devenv are not available: {obj.missing_named_why()}", param, ctx)

        env = Env(
            obj.named_environment_root / envpath,
            (self._flag, envpath.name) if self._flag else (envpath.name,),
        )
        if self._check_exists and not env.root.exists():
            self.fail(f"No devenv named {envpath.name}", param, ctx)
        if env.root.exists() and not env.root.is_dir():
            self.fail(
                f"Selected devenv already exists as a file, remove and try again: {env.root}",
                param,
                ctx,
            )
        if self._check_notexists and env.root.exists():
            click.confirm(f"Selected devenv already exists, remove {env.root}?", abort=True)
            shutil.rmtree(env.root)

        if env.root.parent == obj.named_environment_root:
            with (obj.named_environment_root / ".gitignore").open("w", encoding="utf-8") as gi:
                gi.write(GITIGNORE)

        return env


def resolve_devenv(by_dir: Env | None, by_name: Env | None, default_ty: DevEnvByName) -> Env:
    ctx = click.get_current_context()
    if by_dir is not None and by_name is not None:
        raise click.UsageError("Cannot specify a devenv both by -n/--name and -d/--directory")
    return (
        by_dir
        if by_dir is not None
        else by_name
        if by_name is not None
        else default_ty.convert("default", None, ctx)
    )


@click.group
@click.option(
    "--isolated",
    is_flag=True,
    envvar="DEV_ISOLATED",
    help="Isolate internal data from other invocations",
)
@click.option(
    "--traceback",
    is_flag=True,
    envvar="DEV_TRACEBACK",
    help="Print tracebacks for handled exceptions",
)
@dev_pass_obj
def main(obj: DevState, /, *, isolated: bool, traceback: bool) -> None:
    """Create and manage development environments (devenvs) for building HPCToolkit.

    \b
    Example:
        $ ./dev create && ./dev env   # Creates and enters a devenv shell
        $ make -j install             # Builds and installs HPCToolkit
        $ make installcheck           # Runs the test suite
        $ hpcrun -h
    """  # noqa: D301
    # Try to figure out where the HPCToolkit checkout is. This is not a hard requirement but
    # is needed to use named environments, which are stored in .devenv/.
    for trial in itertools.chain(Path(__file__).resolve(strict=True).parents, [Path()]):
        # We guess by looking for a ./configure --version that looks right
        configure = (trial / "configure").resolve()
        if configure.is_file() and configure.stat().st_mode & stat.S_IXOTH == stat.S_IXOTH:
            proc = subprocess.run(
                [configure, "--version"], capture_output=True, text=True, check=False
            )
            if proc.returncode == 0 and re.search(r"(HPCT|hpct)oolkit", proc.stdout):
                obj.project_root = trial
                break

    if not isolated and obj.project_root is not None:
        obj.named_environment_root = obj.project_root / ".devenv"

    obj.traceback = traceback


feature = click.Choice(["enabled", "disabled", "auto"], case_sensitive=False)
feature_ty = typing.Literal["enabled", "disabled", "auto"]


@main.command
@click.option(
    "-o",
    "-d",
    "--directory",
    "devenv_by_dir",
    type=DevEnvType(notexists=True, flag="-d"),
    help="Create a devenv directory",
)
@click.option(
    "-n",
    "--name",
    "devenv_by_name",
    type=DevEnvByName(notexists=True, flag="-n"),
    help="Create a named devenv",
)
# Generation mode
@click.option(
    "--latest",
    "mode",
    flag_value=DependencyMode.LATEST,
    type=DependencyMode,
    default=True,
    help="Use the latest available version of dependencies",
)
@click.option(
    "--minimum",
    "mode",
    flag_value=DependencyMode.MINIMUM,
    type=DependencyMode,
    help="Use the minimum supported version of dependencies",
)
@click.option(
    "--any",
    "mode",
    flag_value=DependencyMode.ANY,
    type=DependencyMode,
    help="Use any supported version of dependencies",
)
# Feature flags
@click.option(
    "--auto", type=feature, default="auto", help="Interpretation of features set to 'auto'"
)
@click.option(
    "--cuda", type=feature, default="auto", help="Enable NVIDA CUDA (-e gpu=nvidia) support"
)
@click.option("--rocm", type=feature, default="auto", help="Enable AMD ROCm (-e gpu=amd) support")
@click.option(
    "--level0", type=feature, default="auto", help="Enable Intel Level 0 (-e gpu=intel) support"
)
@click.option(
    "--gtpin",
    type=feature,
    default="auto",
    help="Enable Intel instrumentation (-e gpu=intel,inst) support (implies --level0)",
)
@click.option(
    "--opencl", type=feature, default="auto", help="Enable OpenCL (-e gpu=opencl) support"
)
@click.option("--python", type=feature, default="auto", help="Enable Python unwinding (-a python)")
@click.option("--papi", type=feature, default="auto", help="Enable PAPI (-e PAPI_*) support")
@click.option("--mpi", type=feature, default="auto", help="Enable hpcprof-mpi")
# Population options
@click.option(
    "-c",
    "--compiler",
    type=SystemCompilerType(),
    help="Compiler to populate the devenv with",
)
@click.option("--build/--no-build", help="Also build HPCToolkit")
@dev_pass_obj
def create(
    obj: DevState,
    /,
    *,
    devenv_by_dir: Env | None,
    devenv_by_name: Env | None,
    mode: DependencyMode,
    auto: feature_ty,
    cuda: feature_ty,
    rocm: feature_ty,
    level0: feature_ty,
    gtpin: feature_ty,
    opencl: feature_ty,
    python: feature_ty,
    papi: feature_ty,
    mpi: feature_ty,
    compiler: SystemCompiler | None,
    build: bool,
) -> None:
    """Create a fresh devenv ready for building HPCToolkit."""
    # pylint: disable=too-many-arguments
    devenv = resolve_devenv(devenv_by_dir, devenv_by_name, DevEnvByName(notexists=True, flag="-n"))
    del devenv_by_dir
    del devenv_by_name
    if not obj.project_root:
        raise click.ClickException("create only operates within an HPCToolkit project checkout")

    def feature2bool(name: str, value: feature_ty) -> bool:
        if value != "auto":
            return value == "enabled"
        if auto != "auto":
            return auto == "enabled"
        return DevEnv.default(name)

    try:
        env = DevEnv(
            devenv.root,
            cuda=feature2bool("cuda", cuda),
            rocm=feature2bool("rocm", rocm),
            level0=feature2bool("level0", level0),
            gtpin=feature2bool("gtpin", gtpin),
            opencl=feature2bool("opencl", opencl),
            python=feature2bool("python", python),
            papi=feature2bool("papi", papi),
            mpi=feature2bool("mpi", mpi),
        )
        click.echo(env.describe())
        env.generate(mode)
        env.install()
        env.populate(compiler=compiler)
        try:
            if obj.project_root:
                env.setup(obj.project_root, obj.meson)
        except subprocess.CalledProcessError as e:
            if obj.traceback:
                traceback.print_exception(e)
            raise click.ClickException(
                f"""\
meson setup failed! Fix any errors above and continue with:
    $ ./dev populate {shlex.quote(str(devenv.root))}
"""
            ) from e
        if build:
            env.build(obj.meson)
    except KeyboardInterrupt:
        if devenv.root.exists():
            shutil.rmtree(devenv.root)
        raise

    click.echo(
        f"""\
Devenv successfully created! You may now use Meson commands, Eg.:
    $ ./dev meson {shlex.join(devenv.args)} compile
    $ ./dev meson {shlex.join(devenv.args)} install
    $ ./dev meson {shlex.join(devenv.args)} test"""
    )


@main.command
@click.option(
    "-d",
    "--directory",
    "devenv_by_dir",
    type=DevEnvType(exists=True, flag="-d"),
    help="Update a devenv directory",
)
@click.option(
    "-n",
    "--name",
    "devenv_by_name",
    type=DevEnvByName(exists=True, flag="-n"),
    help="Update a named devenv",
)
# Generation mode
@click.option(
    "--latest",
    "mode",
    flag_value=DependencyMode.LATEST,
    type=DependencyMode,
    default=True,
    help="Use the latest available version of dependencies",
)
@click.option(
    "--minimum",
    "mode",
    flag_value=DependencyMode.MINIMUM,
    type=DependencyMode,
    help="Use the minimum supported version of dependencies",
)
@click.option(
    "--any",
    "mode",
    flag_value=DependencyMode.ANY,
    type=DependencyMode,
    help="Use any supported version of dependencies",
)
# Population options
@click.option(
    "-c",
    "--compiler",
    type=SystemCompilerType(),
    help="Compiler to populate the devenv with",
)
@click.option("--build/--no-build", help="Also build HPCToolkit")
@dev_pass_obj
def update(
    obj: DevState,
    /,
    *,
    devenv_by_dir: Env | None,
    devenv_by_name: Env | None,
    mode: DependencyMode,
    compiler: SystemCompiler | None,
    build: bool,
) -> None:
    """Update (re-generate) a devenv."""
    devenv = resolve_devenv(devenv_by_dir, devenv_by_name, DevEnvByName(exists=True, flag="-n"))
    del devenv_by_dir
    del devenv_by_name
    if not obj.project_root:
        raise click.ClickException("create only operates within an HPCToolkit project checkout")

    env = DevEnv.restore(devenv.root)
    yaml = ruamel.yaml.YAML(typ="safe")
    with open(devenv.root / "spack.yaml", encoding="utf-8") as t:
        prior = yaml.load(t)
        if prior and not isinstance(prior, dict):
            raise click.ClickException(f"Invalid template, expected !map but got {prior!r}")
        prior = prior["spack"]

    click.echo(env.describe())
    env.generate(mode, template=prior)
    env.install()
    env.populate(compiler=compiler)
    try:
        if obj.project_root:
            env.setup(obj.project_root, obj.meson)
    except subprocess.CalledProcessError as e:
        if obj.traceback:
            traceback.print_exception(e)
        raise click.ClickException(
            f"""\
meson setup failed! Fix any errors above and continue with:
    $ ./dev populate {shlex.quote(str(devenv.root))}
"""
        ) from e
    if build:
        env.build(obj.meson)

    click.echo(
        f"""\
Devenv successfully created! You may now use Meson commands, Eg.:
    $ ./dev meson {shlex.join(devenv.args)} compile
    $ ./dev meson {shlex.join(devenv.args)} install
    $ ./dev meson {shlex.join(devenv.args)} test"""
    )


@main.command(add_help_option=False, context_settings={"ignore_unknown_options": True})
@click.argument("args", nargs=-1, type=click.UNPROCESSED)
@dev_pass_obj
def pre_commit(obj: DevState, /, *, args: collections.abc.Collection[str]) -> None:
    """Run pre-commit."""
    with obj.internal_named_env("pre-commit", "venv") as ve_dir:
        if not (ve_dir / "bin" / "pre-commit").is_file():
            venv.EnvBuilder(
                clear=True, with_pip=True, upgrade_deps=True, prompt="dev/pre-commit"
            ).create(ve_dir)
            vpython = Command(ve_dir / "bin" / "python3")
            vpython(
                "-m",
                "pip",
                "install",
                "--require-virtualenv",
                "pre-commit>=3.2",
                "identify>=2.5",
            )

        vprecommit = Command(ve_dir / "bin" / "pre-commit")
        vprecommit.execl(*args)


@main.command(context_settings={"ignore_unknown_options": True})
@click.argument("args", nargs=-1, type=click.UNPROCESSED)
@click.option(
    "-d",
    "--directory",
    "devenv_by_dir",
    type=DevEnvType(notexists=True, flag="-d"),
    help="Operate on a devenv directory",
)
@click.option(
    "-n",
    "--name",
    "devenv_by_name",
    type=DevEnvByName(notexists=True, flag="-n"),
    help="Operate on a named devenv",
)
@dev_pass_obj
def meson(
    obj: DevState,
    /,
    *,
    devenv_by_dir: Env | None,
    devenv_by_name: Env | None,
    args: collections.abc.Collection[str],
) -> None:
    """Run meson commands in a devenv."""
    devenv = resolve_devenv(devenv_by_dir, devenv_by_name, DevEnvByName(notexists=True, flag="-n"))
    env = DevEnv.restore(devenv.root)
    obj.meson.execl(*args, cwd=env.builddir)


@main.command
@dev_pass_obj
def poetry(obj: DevState, /) -> None:
    """Set up a Python venv with Poetry."""
    with obj.internal_named_env("poetry", "venv") as ve_dir:
        if not (ve_dir / "bin" / "poetry").is_file():
            venv.EnvBuilder(
                clear=True, with_pip=True, upgrade_deps=True, prompt="dev/poetry"
            ).create(ve_dir)
            vpython = Command(ve_dir / "bin" / "python3")
            vpython("-m", "pip", "install", "--require-virtualenv", "poetry>=1.5.1")

        click.echo("Virtual environment ready! Now run in your working shell:")
        click.echo(f"    . {ve_dir}/bin/activate")


@main.command
@click.option(
    "--custom-env",
    type=click.Path(exists=True, file_okay=False, path_type=Path),
    help="Path to an environment generated by ./dev generate --autogen. Only needed in special circumstances.",
)
@click.option(
    "--install/--skip-install",
    default=True,
    help="Install the Spack environment before attempting to use it",
)
@dev_pass_obj
def autogen(obj: DevState, /, *, custom_env: Path, install: bool) -> None:
    """Regenerate the autogoo (./autogen)."""
    if not obj.project_root:
        raise click.ClickException("autogen only operates within an HPCToolkit project checkout")

    with obj.internal_named_env("autogen", "Spack environment", option=custom_env) as se_dir:
        env = AutogenEnv(se_dir)
        if install:
            env.generate()
            env.install()

        try:
            env.autoreconf(
                "--force",
                "--install",
                "--verbose",
                env=env.environ().to_environ(),
                cwd=obj.project_root,
            )
        except subprocess.CalledProcessError as e:
            if obj.traceback:
                traceback.print_exception(e)
            raise click.ClickException(f"autoreconf exited with code {e.returncode}") from e


@main.command
@click.option(
    "-d",
    "--directory",
    "devenv_by_dir",
    type=DevEnvType(exists=True, flag="-d"),
    help="Describe a devenv directory",
)
@click.option(
    "-n",
    "--name",
    "devenv_by_name",
    type=DevEnvByName(exists=True, flag="-n"),
    help="Describe a named devenv",
)
def describe(*, devenv_by_dir: Env | None, devenv_by_name: Env | None) -> None:
    """Describe a devenv."""
    devenv = resolve_devenv(devenv_by_dir, devenv_by_name, DevEnvByName(exists=True, flag="-n"))
    del devenv_by_dir
    del devenv_by_name
    env = DevEnv.restore(devenv.root)
    click.echo(env.describe(), nl=True)


@main.command
@click.option(
    "-d",
    "--directory",
    "devenv_by_dir",
    type=DevEnvType(exists=True, flag="-d"),
    help="Edir a devenv directory",
)
@click.option(
    "-n",
    "--name",
    "devenv_by_name",
    type=DevEnvByName(exists=True, flag="-n"),
    help="Edit a named devenv",
)
def edit(*, devenv_by_dir: Env | None, devenv_by_name: Env | None) -> None:
    """Edit a devenv's 'spack.yaml'."""
    devenv = resolve_devenv(devenv_by_dir, devenv_by_name, DevEnvByName(exists=True, flag="-n"))
    del devenv_by_dir
    del devenv_by_name
    click.edit(filename=str(devenv.root / "spack.yaml"))
    click.echo(
        f"""\
If you altered the environment, you should now run the
following commands to restore consistency:
    $ spack -e {shlex.quote(str(devenv.root))} concretize -f
    $ spack -e {shlex.quote(str(devenv.root))} install
    $ ./dev populate {shlex.join(devenv.args)}"""
    )


def template_merge(base: dict, overlay: dict) -> dict:
    for k, v in overlay.items():
        if k in base and isinstance(base[k], dict) and isinstance(v, dict):
            base[k] = template_merge(base[k], v)
        else:
            base[k] = v
    return base


@main.command
@click.argument("devenv", type=DevEnvType(notexists=True, flag=None))
# Generation type
@click.option("--autogen", is_flag=True, help="Generate an environment for ./dev autogen instead")
# Generation mode
@click.option(
    "--latest",
    "mode",
    flag_value=DependencyMode.LATEST,
    type=DependencyMode,
    default=True,
    help="Use the latest available version of dependencies",
)
@click.option(
    "--minimum",
    "mode",
    flag_value=DependencyMode.MINIMUM,
    type=DependencyMode,
    help="Use the minimum supported version of dependencies",
)
@click.option(
    "--any",
    "mode",
    flag_value=DependencyMode.ANY,
    type=DependencyMode,
    help="Use any supported version of dependencies",
)
# Feature flags
@click.option(
    "--auto", type=feature, default="auto", help="Interpretation of features set to 'auto'"
)
@click.option(
    "--cuda", type=feature, default="auto", help="Enable NVIDA CUDA (-e gpu=nvidia) support"
)
@click.option("--rocm", type=feature, default="auto", help="Enable AMD ROCm (-e gpu=amd) support")
@click.option(
    "--level0", type=feature, default="auto", help="Enable Intel Level 0 (-e gpu=intel) support"
)
@click.option(
    "--gtpin",
    type=feature,
    default="auto",
    help="Enable Intel instrumentation (-e gpu=intel,inst) support (implies --level0)",
)
@click.option(
    "--opencl", type=feature, default="auto", help="Enable OpenCL (-e gpu=opencl) support"
)
@click.option("--python", type=feature, default="auto", help="Enable Python unwinding (-a python)")
@click.option("--papi", type=feature, default="auto", help="Enable PAPI (-e PAPI_*) support")
@click.option("--mpi", type=feature, default="auto", help="Enable hpcprof-mpi")
@click.option(
    "--template",
    type=click.File(),
    help="Use the given Spack environment template. Additional templates will be merged recursively.",
    multiple=True,
)
def generate(
    *,
    devenv: Env,
    autogen: bool,
    mode: DependencyMode,
    auto: feature_ty,
    cuda: feature_ty,
    rocm: feature_ty,
    level0: feature_ty,
    gtpin: feature_ty,
    opencl: feature_ty,
    python: feature_ty,
    papi: feature_ty,
    mpi: feature_ty,
    template: tuple[typing.TextIO, ...],
) -> None:
    # pylint: disable=too-many-locals
    """Generate a DEVENV for manual installation.

    Consider using 'dev create' instead. This command is only needed if the development environment
    needs to be manually installed, e.g. for a container image.
    """
    # pylint: disable=too-many-arguments

    presteps: list[str] = []
    poststeps: list[str] = []
    nice_devenv = shlex.quote(str(devenv.root))

    env_template: dict | None = None
    template_settings: dict = {}
    if template:
        yaml = ruamel.yaml.YAML(typ="safe")
        env_template = {}
        for t in template:
            data = yaml.load(t)
            if data and not isinstance(data, dict):
                raise click.ClickException(f"Invalid template, expected !map but got {data!r}")
            env_template = template_merge(env_template, data)
        template_settings = env_template.pop("dev", {})

    def feature2bool(name: str, value: feature_ty) -> bool:
        if value != "auto":
            return value == "enabled"
        if auto != "auto":
            return auto == "enabled"
        t = template_settings.get("features", {}).get(name)
        if t is not None:
            if not isinstance(t, bool):
                raise ValueError(t)
            return t
        raise click.UsageError(f"Missing a non-auto setting for --{name} and --auto not set!")

    try:
        if autogen:
            aenv = AutogenEnv(devenv.root)
            aenv.generate(template=env_template)
        else:
            denv = DevEnv(
                devenv.root,
                cuda=feature2bool("cuda", cuda),
                rocm=feature2bool("rocm", rocm),
                level0=feature2bool("level0", level0),
                gtpin=feature2bool("gtpin", gtpin),
                opencl=feature2bool("opencl", opencl),
                python=feature2bool("python", python),
                papi=feature2bool("papi", papi),
                mpi=feature2bool("mpi", mpi),
            )
            click.echo(denv.describe())
            denv.generate(mode, template=env_template)
            presteps.append(f"./dev edit -d {nice_devenv}   # (optional)")
            poststeps.append(f"./dev populate {nice_devenv}")
    except KeyboardInterrupt:
        if devenv.root.exists():
            shutil.rmtree(devenv.root)
        raise

    click.echo("Devenv generated for manual installation. Your next steps should be:")
    for line in presteps:
        click.echo(f"    $ {line}")
    click.echo(f"    $ spack -e {nice_devenv} install")
    for line in poststeps:
        click.echo(f"    $ {line}")


def pack_for_request(env: SpackEnv, features: collections.abc.Collection[str] = ()) -> dict:
    """Take a generated Env and produce the contents for a request JSON."""
    root = env.root
    with (root / "spack.yaml").open("r", encoding="utf-8") as spackyamlf:
        spack = ruamel.yaml.YAML(typ="safe").load(spackyamlf)["spack"]

    files: dict[str, dict[str, str]] = {}
    for path in root.rglob("*"):
        if path.parent == root and path.name == "spack.yaml":
            # Handled in a later pass
            continue

        try:
            data = {"text": path.read_text(encoding="utf-8", errors="strict")}
        except ValueError:
            data = {"base64": base64.b64encode(path.read_bytes()).decode("ascii")}
        files[f"/{path.relative_to(root).as_posix()}"] = data

    return {
        "ifAvailable": sorted(features),
        "spack": spack,
        "files": files,
    }


# Translation table between DevEnv keyword arguments and their required request features
FEATURE_FLAGS: dict[str, collections.abc.Collection[str]] = {
    "cuda": ("cuda",),
    "rocm": ("rocm",),
    "level0": ("level0",),
    "gtpin": ("gtpin", "igc"),  # Implies level0
    "opencl": (),
    "python": (),
    "papi": (),
    "mpi": (),
}

# Translation table between request features and external Spack packages
FEATURE_PACKAGES: dict[str, collections.abc.Collection[str]] = {
    "cuda": ("cuda",),
    "rocm": ("hip", "hsa-rocr-dev", "rocprofiler-dev", "roctracer-dev"),
    "level0": ("oneapi-level-zero",),
    "gtpin": ("intel-gtpin",),
    "igc": ("oneapi-igc"),
}


@main.command
@click.argument("request", type=click.File("w", encoding="utf-8"))
# Generation type
@click.option("--autogen", is_flag=True, help="Generate an environment for ./dev autogen instead")
# Generation mode
@click.option(
    "--latest",
    "mode",
    flag_value=DependencyMode.LATEST,
    type=DependencyMode,
    default=True,
    help="Use the latest available version of dependencies",
)
@click.option(
    "--minimum",
    "mode",
    flag_value=DependencyMode.MINIMUM,
    type=DependencyMode,
    help="Use the minimum supported version of dependencies",
)
@click.option(
    "--any",
    "mode",
    flag_value=DependencyMode.ANY,
    type=DependencyMode,
    help="Use any supported version of dependencies",
)
# Feature flags
@click.option(
    "--template",
    type=click.File(),
    help="Use the given Spack environment template. Additional templates will be merged recursively.",
    multiple=True,
)
def generate_request(
    *,
    request: typing.TextIO,
    autogen: bool,
    mode: DependencyMode,
    template: tuple[typing.TextIO, ...],
) -> None:
    """Generate a generic REQUEST JSON file that describes all possible devenvs.

    This command is primarily used internally by CI. It probably isn't what you are looking for,
    consider `./dev create`.
    """
    env_template: dict | None = None
    if template:
        yaml = ruamel.yaml.YAML(typ="safe")
        env_template = {}
        for t in template:
            data = yaml.load(t)
            if data and not isinstance(data, dict):
                raise click.ClickException(f"Invalid template, expected !map but got {data!r}")
            env_template = template_merge(env_template, data)
        if "dev" in env_template:
            del env_template["dev"]

    with tempfile.TemporaryDirectory() as tmpd_str:
        tmpd = Path(tmpd_str).resolve(strict=True)

        contents: list[dict] = []
        if autogen:
            aenv = AutogenEnv(tmpd / "root")
            aenv.generate()
            contents.append(pack_for_request(aenv))
        else:
            # Iterate over the actionable configuration space
            flag_space = sorted(flag for flag, feats in FEATURE_FLAGS.items() if feats)
            for r in range(len(flag_space), -1, -1):
                for flags in itertools.combinations(flag_space, r):
                    try:
                        denv = DevEnv(
                            tmpd / ("root " + " ".join(flags)),
                            **{
                                flag: flag in flags or not feats
                                for flag, feats in FEATURE_FLAGS.items()
                            },
                        )
                    except InvalidSpecificationError:
                        continue

                    unresolve: set[str] = {
                        pkg
                        for flag in flags
                        for feat in FEATURE_FLAGS[flag]
                        for pkg in FEATURE_PACKAGES[feat]
                    }
                    denv.generate(mode, unresolve=unresolve, template=env_template)

                    feats: set[str] = set()
                    for flag in flags:
                        feats |= set(FEATURE_FLAGS[flag])
                    contents.append(pack_for_request(denv, feats))

    result = {
        "version": 2,
        "contents": contents,
    }
    jsonschema.validate(result, schema.request)
    json.dump(result, request, sort_keys=True, separators=(",", ":"))


@main.command
@click.argument("devenv", type=DevEnvType(exists=True, flag=None))
@click.option(
    "-c",
    "--compiler",
    type=SystemCompilerType(),
    help="Compiler to populate the devenv with",
)
@dev_pass_obj
def populate(obj: DevState, /, *, devenv: Env, compiler: SystemCompiler | None) -> None:
    """Populate a manually installed DEVENV.

    Consider using 'dev create' instead. This command is only needed if the development environment
    needs to be manually installed, e.g. for a container image.
    """
    env = DevEnv.restore(devenv.root)
    env.populate(compiler=compiler)
    try:
        if obj.project_root:
            env.setup(obj.project_root, obj.meson)
    except subprocess.CalledProcessError as e:
        if obj.traceback:
            traceback.print_exception(e)
        raise click.ClickException("meson setup failed! Fix any errors above and try again.") from e

    if obj.project_root:
        click.echo(
            f"""\
Devenv successfully created! You may now use Meson commands, Eg.:
    $ ./dev meson -d {shlex.quote(str(devenv.root))} compile
    $ ./dev meson -d {shlex.quote(str(devenv.root))} install
    $ ./dev meson -d {shlex.quote(str(devenv.root))} test"""
        )
    else:
        click.echo(
            f"""
Devenv has been populated but not configured. Use via the buildfe:
    $ ./dev buildfe -d {shlex.quote(str(devenv.root))} -- ..."""
        )


@main.command(add_help_option=False, context_settings={"ignore_unknown_options": True})
@click.option(
    "-d",
    "--directory",
    "devenv_by_dir",
    type=DevEnvType(exists=True, flag="-d"),
    help="Run within a devenv directory",
)
@click.option(
    "-n",
    "--name",
    "devenv_by_name",
    type=DevEnvByName(exists=True, flag="-n"),
    help="Run within a named devenv",
)
@click.argument("args", nargs=-1, type=click.UNPROCESSED)
@dev_pass_obj
def buildfe(
    obj: DevState,
    devenv_by_dir: Env | None,
    devenv_by_name: Env | None,
    args: collections.abc.Collection[str],
) -> None:
    """Run the build frontend with the given ARGS."""
    devenv = resolve_devenv(devenv_by_dir, devenv_by_name, DevEnvByName(exists=True, flag="-n"))
    del devenv_by_dir
    del devenv_by_name
    if not obj.project_root:
        raise click.ClickException("buildfe only operates within an HPCToolkit project checkout")

    env = DevEnv.restore(devenv.root)
    os.chdir(obj.project_root)
    buildfe_main(obj.meson.path, [*args, str(env.root)])


@main.command
@click.argument("packages", nargs=-1)
@click.option(
    "-c",
    "--compiler",
    type=SystemCompilerType(missing_ok=True),
    multiple=True,
    help="Also install the given compiler(s)",
)
@click.option(
    "-y", "--yes-to-all", is_flag=True, help="Avoid any prompts during the install process"
)
def os_install(
    *,
    packages: collections.abc.Iterable[str],
    compiler: collections.abc.Iterable[SystemCompiler],
    yes_to_all: bool,
) -> None:
    """Install a number of PACKAGES by autodetecting the OS installation. Packages are generally
    named by their names in Ubuntu/Debian and will be transformed as needed.

    Primarily intended for setting up emphemeral build containers.
    """
    # First figure out what kind of system we are on
    precmd: Command | None = None
    if apt := shutil.which("apt-get"):
        precmd = Command(apt, "update", "-qq", *(["-y"] if yes_to_all else []))
        cmd = Command(apt, "install", "-qq", *(["-y"] if yes_to_all else []))
        osclass = OSClass.DebianLike
    elif zypper := shutil.which("zypper"):
        cmd = Command(zypper, "install", *(["-y"] if yes_to_all else []))
        osclass = OSClass.SUSELeap
    elif dnf := shutil.which("dnf"):
        cmd = Command(dnf, "install", *(["-y"] if yes_to_all else []))
        # On everything except Fedora, we need to install epel-release
        if not re.search(r"(?m)^ID=fedora$", Path("/etc/os-release").read_text(encoding="utf-8")):
            precmd = cmd.withargs("epel-release")
        osclass = OSClass.RedHatLike
    else:
        raise RuntimeError("Unable to identify the install command for this OS!")

    # Determine the packages to install
    to_install: set[str] = {os_translate(pkg, osclass) for pkg in packages}
    for comp in compiler:
        to_install |= comp.os_packages(osclass)

    # Do the installation
    with logs.section(f"Installing packages {' '.join(to_install)}", collapsed=True):
        try:
            if precmd is not None:
                precmd()
            cmd(*to_install)
        except subprocess.CalledProcessError as e:
            raise click.ClickException("Install commands failed, see above errors.") from e


if __name__ == "__main__":
    main()  # pylint: disable=no-value-for-parameter,missing-kwoa
