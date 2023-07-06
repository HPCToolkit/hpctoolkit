import collections
import contextlib
import dataclasses
import functools
import itertools
import os
import re
import shlex
import shutil
import stat
import subprocess
import tempfile
import textwrap
import typing
import venv
from pathlib import Path

import click
import ruamel.yaml

from .buildfe._main import main as buildfe_main
from .command import Command
from .envs import AutogenEnv, DevEnv
from .spack import Spack
from .spec import DependencyMode

__all__ = ("main",)


@dataclasses.dataclass(kw_only=True)
class DevState:
    # Root of the HPCToolkit checkout, if it exists.
    project_root: Path | None = None

    # Directory containing named environments, if named environments are enabled.
    # May not exist, create this directory lazily when needed.
    named_environment_root: Path | None = None

    _MISSING_PROJECT_ROOT_SOLUTIONS = textwrap.dedent(
        """\
    Potential solutions:
      1. cd to the HPCToolkit project root directory.
      2. Use the ./dev wrapper script in the project root directory.
      3. Ensure './configure --version' works properly."""
    )

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


class NamedEnv(click.ParamType):
    """ParamType for a named devenv, stored under `.devenv/`.

    This concept only exists in the CLI, the backend classes use full Paths instead.
    """

    name = "devenv name"

    def convert(
        self, value: typing.Any, param: click.Parameter | None, ctx: click.Context | None
    ) -> Path:
        assert ctx is not None
        obj = ctx.find_object(DevState)
        assert obj is not None

        if not obj.named_environment_root:
            self.fail(obj.missing_named_why(), param, ctx)

        if isinstance(value, Path):
            if not obj.named_environment_root.samefile(value.parent):
                self.fail("Path does not refer to a named devenv", param, ctx)

            if not value.name or value.name[0] == "_":
                self.fail(f"Path to devenv has an invalid name: {value.name}", param, ctx)

            return value

        if not isinstance(value, str):
            raise TypeError(value)

        if value[0] == "_" or "/" in value or "\\" in value:
            self.fail(f"Name is invalid for a devenv: {value}", param, ctx)

        return obj.named_environment_root / value

    @classmethod
    def pass_env(
        cls,
        /,
        *,
        nameflags: collections.abc.Collection[str] = ("-n", "--name"),
        dirflags: collections.abc.Collection[str] = ("-d", "--directory"),
        exists: bool = False,
        notexists: bool = False,
        help_verb: str,
    ):
        assert not exists or not notexists
        nameflags = [*list(nameflags), "devenv_by_name"]
        dirflags = [*list(dirflags), "devenv_by_dir"]

        def result(f):
            @dev_pass_obj
            @click.option(*nameflags, type=cls(), help=f"{help_verb} a named devenv")
            @click.option(
                *dirflags,
                type=click.Path(file_okay=False, exists=exists, writable=True, path_type=Path),
                help=f"{help_verb} a devenv directory",
            )
            @functools.wraps(f)
            def wrapper(
                obj: DevState,
                *args,
                devenv_by_name: Path | None,
                devenv_by_dir: Path | None,
                **kwargs,
            ):
                if devenv_by_name is not None:
                    if devenv_by_dir is not None:
                        raise click.UsageError(
                            f"{'/'.join(nameflags)} and {'/'.join(dirflags)} cannot both be given"
                        )
                    if not obj.named_environment_root:
                        raise click.UsageError(
                            f"Named devenvs are not available: {obj.missing_named_why()}"
                        )
                    env = Env(
                        obj.named_environment_root / devenv_by_name, ("-n", devenv_by_name.name)
                    )
                    if exists and not env.root.is_dir():
                        raise click.BadParameter(f"No devenv named {devenv_by_name.name}")
                elif devenv_by_dir is not None:
                    env = Env(devenv_by_dir, ("-d", str(devenv_by_dir)))
                else:
                    if not obj.named_environment_root:
                        raise click.UsageError(
                            f"Default devenv is not available: {obj.missing_named_why()}"
                        )
                    env = Env(obj.named_environment_root / "default", ())

                if env.root.exists() and not env.root.is_dir():
                    raise click.UsageError(
                        f"Selected devenv already exists as a file, remove and try again: {env.root}"
                    )
                if notexists and env.root.exists():
                    click.confirm(f"Selected devenv already exists, remove {env.root}?", abort=True)
                    shutil.rmtree(env.root)
                if exists and not env.root.exists():
                    raise click.UsageError(
                        f"Selected devenv does not exist, try: ./dev create {shlex.join(env.args)}"
                    )

                if env.root.parent == obj.named_environment_root:
                    with open(
                        obj.named_environment_root / ".gitignore", "w", encoding="utf-8"
                    ) as gi:
                        gi.write(GITIGNORE)

                return f(env, *args, **kwargs)

            return wrapper

        return result

    @classmethod
    def pass_env_arg(cls, /, *, exists: bool = False, notexists: bool = False):
        def result(f):
            @click.argument(
                "devenv_by_dir",
                metavar="DEVENV",
                type=click.Path(file_okay=False, exists=exists, writable=True, path_type=Path),
            )
            @functools.wraps(f)
            def wrapper(*args, devenv_by_dir: Path, **kwargs):
                if notexists and devenv_by_dir.exists():
                    raise click.UsageError(
                        f"Selected devenv already exists, remove and try again: {devenv_by_dir}"
                    )
                return f(devenv_by_dir, *args, **kwargs)

            return wrapper

        return result


@click.group()
@click.option(
    "--isolated",
    is_flag=True,
    envvar="DEV_ISOLATED",
    help="Isolate internal data from other invocations",
)
@dev_pass_obj
def main(obj: DevState, isolated: bool) -> None:
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


feature = click.Choice(["enabled", "disabled", "auto"], case_sensitive=False)
feature_ty = typing.Literal["enabled", "disabled", "auto"]


@main.command()
@NamedEnv.pass_env(notexists=True, dirflags=("-o", "-d", "--directory"), help_verb="Create")
@dev_pass_obj
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
@click.option("--build/--no-build", help="Also build HPCToolkit")
def create(
    obj: DevState,
    devenv: Env,
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
    build: bool,
) -> None:
    """Create a fresh devenv ready for building HPCToolkit."""
    # pylint: disable=too-many-arguments
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
            optional_papi=False,
            mpi=feature2bool("mpi", mpi),
        )
        click.echo(env.describe())
        env.generate(mode)
        env.install()
        try:
            env.populate(obj.project_root)
        except subprocess.CalledProcessError as e:
            raise click.ClickException(
                f"""\
./configure failed! Fix any errors above and continue with:
    $ ./dev populate {shlex.quote(str(devenv.root))}
"""
            ) from e
        if build:
            with env.activate():
                env.build()
    except KeyboardInterrupt:
        if devenv.root.exists():
            shutil.rmtree(devenv.root)
        raise

    click.echo(
        f"""\
Devenv successfully created! You may now enter the devenv with:
    $ ./dev env {shlex.join(devenv.args)}
Further commands (make/meson/etc.) MUST be run under this shell. Eg.:
    $ make -j
    $ make -j install
    $ hpcrun -h"""
    )


@main.command
@NamedEnv.pass_env(exists=True, help_verb="Update")
@dev_pass_obj
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
@click.option("--build/--no-build", help="Also build HPCToolkit")
def update(obj: DevState, devenv: Env, mode: DependencyMode, build: bool) -> None:
    """Update (re-generate) a devenv."""
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
    try:
        env.populate(obj.project_root)
    except subprocess.CalledProcessError as e:
        raise click.ClickException(
            f"""\
./configure failed! Fix any errors above and continue with:
$ ./dev populate {shlex.quote(str(devenv.root))}
"""
        ) from e
    if build:
        with env.activate():
            env.build()

    click.echo(
        f"""\
Devenv successfully created! You may now enter the devenv with:
    $ ./dev env {shlex.join(devenv.args)}
Further commands (make/meson/etc.) MUST be run under this shell. Eg.:
    $ make -j
    $ make -j install
    $ hpcrun -h"""
    )


@main.command
@click.argument("command", nargs=-1, type=click.UNPROCESSED)
@NamedEnv.pass_env(exists=True, help_verb="Run within")
def env(devenv: Env, command: collections.abc.Sequence[str]) -> None:
    """Run COMMAND (interactive shell by default) within a devenv.

    The working directory of the COMMAND (or shell) will the build directory for the environment, if
    it exists.
    """
    env = DevEnv.restore(devenv.root)

    if not command:
        spack_setup = (
            Path(Spack.get().capture("location", "--spack-root").strip())
            / "share"
            / "spack"
            / "setup-env.sh"
        )
        click.echo(
            f"""\
You are now entering a devenv shell. Some quick notes:
    1. Do NOT load/unload modules or Spack packages (spack load).
        These can cause the build to become inconsistent.
    2. The Spack environment for the devenv is NOT activated.
        If you have altered the spack.yaml and need this, run in the shell:
            $ . {shlex.quote(str(spack_setup))}
            $ spack env activate {shlex.quote(str(env.root))}
    3. You will be automatically placed in the build directory of
        this devenv, if it exists."""
        )

    with env.activate():
        if env.builddir.exists():
            os.chdir(env.builddir)
        if not command:
            shell = Path(os.environ.get("SHELL", "/bin/sh")).resolve()
            os.execl(shell, shell)
        else:
            os.execvp(command[0], list(command))


@main.command(add_help_option=False, context_settings={"ignore_unknown_options": True})
@click.argument("args", nargs=-1, type=click.UNPROCESSED)
@dev_pass_obj
def pre_commit(obj: DevState, args: collections.abc.Collection[str]) -> None:
    """Run pre-commit."""
    with obj.internal_named_env("pre-commit", "venv") as ve_dir:
        if not (ve_dir / "bin" / "pre-commit").is_file():
            venv.EnvBuilder(
                clear=True, with_pip=True, upgrade_deps=True, prompt="dev/pre-commit"
            ).create(ve_dir)
            vpython = Command(ve_dir / "bin" / "python3")
            vpython(
                "-m", "pip", "install", "--require-virtualenv", "pre-commit>=3.2", "identify>=2.5"
            )

        vprecommit = Command(ve_dir / "bin" / "pre-commit")
        vprecommit(*args)


@main.command
@dev_pass_obj
def poetry(obj: DevState) -> None:
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
@dev_pass_obj
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
def autogen(obj: DevState, custom_env: Path, install: bool) -> None:
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
            raise click.ClickException(f"autoreconf exited with code {e.returncode}") from e


@main.command
@NamedEnv.pass_env(exists=True, help_verb="Describe")
def describe(devenv: Env) -> None:
    """Describe a devenv."""
    env = DevEnv.restore(devenv.root)
    click.echo(env.describe(), nl=True)


@main.command
@NamedEnv.pass_env(exists=True, help_verb="Edit")
def edit(devenv: Env) -> None:
    """Edit a devenv's 'spack.yaml'."""
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
@NamedEnv.pass_env_arg(notexists=True)
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
@click.option(
    "--papi",
    type=click.Choice(["enabled", "disabled", "auto", "both"], case_sensitive=False),
    default="auto",
    help="Enable PAPI (-e PAPI_*) support",
)
@click.option("--mpi", type=feature, default="auto", help="Enable hpcprof-mpi")
@click.option(
    "--template",
    type=click.File(),
    help="Use the given Spack environment template. Additional templates will be merged recursively.",
    multiple=True,
)
def generate(  # noqa: C901
    devenv: Path,
    autogen: bool,
    mode: DependencyMode,
    auto: feature_ty,
    cuda: feature_ty,
    rocm: feature_ty,
    level0: feature_ty,
    gtpin: feature_ty,
    opencl: feature_ty,
    python: feature_ty,
    papi: feature_ty | typing.Literal["both"],
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
    nice_devenv = shlex.quote(str(devenv))

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

    def feature2bool(name: str, value: feature_ty | typing.Literal["both"]) -> bool:
        if value != "auto":
            return value == "enabled"
        if auto != "auto":
            return auto == "enabled"
        t = template_settings.get("features", {}).get(name)
        if t is not None:
            if t == "both" and name == "papi":
                return False
            if not isinstance(t, bool):
                raise ValueError(t)
            return t
        raise click.UsageError(f"Missing a non-auto setting for --{name} and --auto not set!")

    try:
        if autogen:
            aenv = AutogenEnv(devenv)
            aenv.generate(template=env_template)
        else:
            denv = DevEnv(
                devenv,
                cuda=feature2bool("cuda", cuda),
                rocm=feature2bool("rocm", rocm),
                level0=feature2bool("level0", level0),
                gtpin=feature2bool("gtpin", gtpin),
                opencl=feature2bool("opencl", opencl),
                python=feature2bool("python", python),
                papi=feature2bool("papi", papi),
                optional_papi=(
                    papi == "both"
                    or papi == "auto"
                    and template_settings.get("features", {}).get("papi") == "both"
                ),
                mpi=feature2bool("mpi", mpi),
            )
            click.echo(denv.describe())
            denv.generate(mode, template=env_template)
            presteps.append(f"./dev edit -d {nice_devenv}   # (optional)")
            poststeps.append(f"./dev populate {nice_devenv}")
    except KeyboardInterrupt:
        if devenv.exists():
            shutil.rmtree(devenv)
        raise

    click.echo("Devenv generated for manual installation. Your next steps should be:")
    for line in presteps:
        click.echo(f"    $ {line}")
    click.echo(f"    $ spack -e {nice_devenv} install")
    for line in poststeps:
        click.echo(f"    $ {line}")


@main.command
@NamedEnv.pass_env_arg(exists=True)
@dev_pass_obj
def populate(obj: DevState, devenv: Path) -> None:
    """Populate a manually installed DEVENV.

    Consider using 'dev create' instead. This command is only needed if the development environment
    needs to be manually installed, e.g. for a container image.
    """
    env = DevEnv.restore(devenv)
    try:
        env.populate(obj.project_root)
    except subprocess.CalledProcessError as e:
        raise click.ClickException("./configure failed! Fix any errors above and try again.") from e

    if obj.project_root:
        click.echo(
            f"""\
Devenv has been populated and is ready for use. To enter run:
    $ ./dev env -d {shlex.quote(str(devenv))}
Further commands (make/meson/etc.) MUST be run under this shell. Eg.:
    $ make -j
    $ make -j install
    $ hpcrun -h"""
        )
    else:
        click.echo(
            f"""
Devenv has been populated but not configured. Use via the buildfe:
    $ ./dev buildfe -d {shlex.quote(str(devenv))} -- ..."""
        )


@main.command(add_help_option=False, context_settings={"ignore_unknown_options": True})
@NamedEnv.pass_env(exists=True, help_verb="Run within")
@dev_pass_obj
@click.argument("args", nargs=-1, type=click.UNPROCESSED)
def buildfe(obj: DevState, devenv: Env, args: collections.abc.Collection[str]) -> None:
    """Run the build frontend with the given ARGS."""
    if not obj.project_root:
        raise click.ClickException("buildfe only operates within an HPCToolkit project checkout")

    env = DevEnv.restore(devenv.root)
    with env.activate():
        os.chdir(obj.project_root)
        buildfe_main([*args] + [str(env.root)])


if __name__ == "__main__":
    main()  # pylint: disable=no-value-for-parameter
