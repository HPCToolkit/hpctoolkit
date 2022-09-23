#!/usr/bin/env python3

import subprocess
import sys
import shutil
from urllib.request import urlopen

git = shutil.which("git")

target = sys.argv[1]
new_patches = sys.argv[2:]
ok_patches = []


def apply(patch, check=True):
    with urlopen(patch) as pf:
        patch_data = pf.read()
    return (
        subprocess.run(
            [git, "-C", target, "apply", "-3", "-"], input=patch_data, check=check
        ).returncode
        == 0
    )


done = False
while not done:
    # Reset the target's Git index
    subprocess.run([git, "-C", target, "reset", "--hard"], check=True)

    # Re-apply the sequence we already know worked, so far
    for patch in ok_patches:
        apply(patch)

    # Attempt to apply new patches, in order. If one fails we need to reset and try this all over again.
    done = True
    while len(new_patches) > 0:
        patch = new_patches.pop(0)
        if not apply(patch, check=False):
            print(f"Patch failed to apply, it will be skipped: {patch}")
            done = False
            break
        print(f"Successfully applied patch: {patch}")
