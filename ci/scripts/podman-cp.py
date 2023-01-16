#!/usr/bin/env python3

import argparse
import tarfile
import tempfile
from pathlib import PurePath

from podman import PodmanClient  # type: ignore[import]

parser = argparse.ArgumentParser(
    description="Copy files from a Podman image into the current space"
)
parser.add_argument("url", help="Base URL of the Podman socket")
parser.add_argument("image", help="Image to copy files out of")
parser.add_argument(
    "paths", type=PurePath, nargs="+", help="Paths to files/directories to copy out"
)
args = parser.parse_args()
del parser

with PodmanClient(base_url=args.url) as client:
    image = client.images.pull(args.image)
    print(f"Pulled image {args.image} as {image.attrs['RepoDigests'][0]}")
    container = client.containers.create(image)
    try:
        for path in args.paths:
            path = PurePath("/") / path
            print(f"Copying {path}...")
            with tempfile.TemporaryFile() as tf:
                for block in container.get_archive(path.as_posix())[0]:
                    tf.write(block)

                tf.seek(0)
                with tarfile.open(fileobj=tf) as tarf:
                    tarf.extractall("/")
    finally:
        container.remove()
