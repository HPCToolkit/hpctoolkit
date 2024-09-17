# SPDX-FileCopyrightText: 2023-2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

import runpy
import sys
import typing
from pathlib import Path

import click


def gtest_out(
    ctx: click.Context,
    param: click.Parameter,
    value: typing.Optional[typing.Union[str, Path]],
) -> typing.Optional[Path]:
    if value is None or isinstance(value, Path):
        return value

    bits = value.split(":", maxsplit=1)
    if len(bits) < 2 or bits[0] != "xml":
        raise click.BadParameter(f"No xml: prefix on value {value!r}", ctx, param)
    if not bits[1]:
        raise click.BadParameter("Expected path after xml: prefix", ctx, param)
    return Path(bits[1])


@click.command(context_settings={"ignore_unknown_options": True})
@click.option("--gtest_output", type=click.UNPROCESSED, callback=gtest_out)
@click.argument("args", type=click.UNPROCESSED, nargs=-1)
@click.pass_context
def main(
    ctx: click.Context, gtest_output: typing.Optional[Path], args: typing.Tuple[str]
):
    """Invoke PyTest but translate (some) GTest arguments into PyTest first."""
    sys.argv = [sys.argv[0], *ctx.args, *list(args)]
    if gtest_output is not None:
        sys.argv.append(f"--junitxml={gtest_output}")
    runpy.run_module("pytest", run_name="__main__", alter_sys=True)


if __name__ == "__main__":
    main()  # pylint: disable=no-value-for-parameter
