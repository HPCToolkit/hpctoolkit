#!/bin/sh -e

# SPDX-FileCopyrightText: 2023-2024 Rice University
# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: BSD-3-Clause

cd "$(dirname "$(realpath "$0")")"

rm -rf bin/
mkdir bin/

podman run --rm -v ./:/src --pull always docker.io/debian:buster-slim sh -exc \
  'apt-get update -yq && apt-get install -y gcc-x86-64-linux-gnu clang-7
   x86_64-linux-gnu-gcc-8 --version > /src/gcc8-x86_64.txt
   x86_64-linux-gnu-gcc-8 /src/inlines+loops.c -shared -fPIC -o /src/bin/inlines+loops-x86_64-gcc8-0 -g -O0
   x86_64-linux-gnu-gcc-8 /src/inlines+loops.c -shared -fPIC -o /src/bin/inlines+loops-x86_64-gcc8-d -g -Og
   x86_64-linux-gnu-gcc-8 /src/inlines+loops.c -shared -fPIC -o /src/bin/inlines+loops-x86_64-gcc8-dr -g -O3
   x86_64-linux-gnu-gcc-8 /src/inlines+loops.c -shared -fPIC -o /src/bin/inlines+loops-x86_64-gcc8-r -O3
   x86_64-linux-gnu-gcc-8 /src/recursion.c -shared -fPIC -o /src/bin/recursion-x86_64-gcc8-0 -g -O0
   x86_64-linux-gnu-gcc-8 /src/recursion.c -shared -fPIC -o /src/bin/recursion-x86_64-gcc8-d -g -Og
   x86_64-linux-gnu-gcc-8 /src/recursion.c -shared -fPIC -o /src/bin/recursion-x86_64-gcc8-dr -g -O3
   x86_64-linux-gnu-gcc-8 /src/recursion.c -shared -fPIC -o /src/bin/recursion-x86_64-gcc8-r -O3
   clang-7 --version > /src/llvm7-x86_64.txt
   clang-7 -target x86_64-linux-gnu /src/inlines+loops.c -shared -fPIC -o /src/bin/inlines+loops-x86_64-llvm7-0 -g -O0
   clang-7 -target x86_64-linux-gnu /src/inlines+loops.c -shared -fPIC -o /src/bin/inlines+loops-x86_64-llvm7-d -g -Og
   clang-7 -target x86_64-linux-gnu /src/inlines+loops.c -shared -fPIC -o /src/bin/inlines+loops-x86_64-llvm7-dr -g -O3
   clang-7 -target x86_64-linux-gnu /src/inlines+loops.c -shared -fPIC -o /src/bin/inlines+loops-x86_64-llvm7-r -O3
   clang-7 -target x86_64-linux-gnu /src/recursion.c -shared -fPIC -o /src/bin/recursion-x86_64-llvm7-0 -g -O0
   clang-7 -target x86_64-linux-gnu /src/recursion.c -shared -fPIC -o /src/bin/recursion-x86_64-llvm7-d -g -Og
   clang-7 -target x86_64-linux-gnu /src/recursion.c -shared -fPIC -o /src/bin/recursion-x86_64-llvm7-dr -g -O3
   clang-7 -target x86_64-linux-gnu /src/recursion.c -shared -fPIC -o /src/bin/recursion-x86_64-llvm7-r -O3
   '

podman run --rm -v ./:/src --pull always docker.io/debian:bullseye-slim sh -exc \
  'apt-get update -yq && apt-get install -y gcc-x86-64-linux-gnu clang-11
   x86_64-linux-gnu-gcc-10 --version > /src/gcc10-x86_64.txt
   x86_64-linux-gnu-gcc-10 /src/inlines+loops.c -shared -fPIC -o /src/bin/inlines+loops-x86_64-gcc10-0 -g -O0
   x86_64-linux-gnu-gcc-10 /src/inlines+loops.c -shared -fPIC -o /src/bin/inlines+loops-x86_64-gcc10-d -g -Og
   x86_64-linux-gnu-gcc-10 /src/inlines+loops.c -shared -fPIC -o /src/bin/inlines+loops-x86_64-gcc10-dr -g -O3
   x86_64-linux-gnu-gcc-10 /src/inlines+loops.c -shared -fPIC -o /src/bin/inlines+loops-x86_64-gcc10-r -O3
   x86_64-linux-gnu-gcc-10 /src/recursion.c -shared -fPIC -o /src/bin/recursion-x86_64-gcc10-0 -g -O0
   x86_64-linux-gnu-gcc-10 /src/recursion.c -shared -fPIC -o /src/bin/recursion-x86_64-gcc10-d -g -Og
   x86_64-linux-gnu-gcc-10 /src/recursion.c -shared -fPIC -o /src/bin/recursion-x86_64-gcc10-dr -g -O3
   x86_64-linux-gnu-gcc-10 /src/recursion.c -shared -fPIC -o /src/bin/recursion-x86_64-gcc10-r -O3
   clang-11 --version > /src/llvm11-x86_64.txt
   clang-11 -target x86_64-linux-gnu /src/inlines+loops.c -shared -fPIC -o /src/bin/inlines+loops-x86_64-llvm11-0 -g -O0
   clang-11 -target x86_64-linux-gnu /src/inlines+loops.c -shared -fPIC -o /src/bin/inlines+loops-x86_64-llvm11-d -g -Og
   clang-11 -target x86_64-linux-gnu /src/inlines+loops.c -shared -fPIC -o /src/bin/inlines+loops-x86_64-llvm11-dr -g -O3
   clang-11 -target x86_64-linux-gnu /src/inlines+loops.c -shared -fPIC -o /src/bin/inlines+loops-x86_64-llvm11-r -O3
   clang-11 -target x86_64-linux-gnu /src/recursion.c -shared -fPIC -o /src/bin/recursion-x86_64-llvm11-0 -g -O0
   clang-11 -target x86_64-linux-gnu /src/recursion.c -shared -fPIC -o /src/bin/recursion-x86_64-llvm11-d -g -Og
   clang-11 -target x86_64-linux-gnu /src/recursion.c -shared -fPIC -o /src/bin/recursion-x86_64-llvm11-dr -g -O3
   clang-11 -target x86_64-linux-gnu /src/recursion.c -shared -fPIC -o /src/bin/recursion-x86_64-llvm11-r -O3
   '

podman run --rm -v ./:/src --pull always docker.io/debian:bookworm-slim sh -exc \
  'apt-get update -yq && apt-get install -y gcc-x86-64-linux-gnu clang-15
   x86_64-linux-gnu-gcc-12 --version > /src/gcc12-x86_64.txt
   x86_64-linux-gnu-gcc-12 /src/inlines+loops.c -shared -fPIC -o /src/bin/inlines+loops-x86_64-gcc12-0 -g -O0
   x86_64-linux-gnu-gcc-12 /src/inlines+loops.c -shared -fPIC -o /src/bin/inlines+loops-x86_64-gcc12-d -g -Og
   x86_64-linux-gnu-gcc-12 /src/inlines+loops.c -shared -fPIC -o /src/bin/inlines+loops-x86_64-gcc12-dr -g -O3
   x86_64-linux-gnu-gcc-12 /src/inlines+loops.c -shared -fPIC -o /src/bin/inlines+loops-x86_64-gcc12-r -O3
   x86_64-linux-gnu-gcc-12 /src/recursion.c -shared -fPIC -o /src/bin/recursion-x86_64-gcc12-0 -g -O0
   x86_64-linux-gnu-gcc-12 /src/recursion.c -shared -fPIC -o /src/bin/recursion-x86_64-gcc12-d -g -Og
   x86_64-linux-gnu-gcc-12 /src/recursion.c -shared -fPIC -o /src/bin/recursion-x86_64-gcc12-dr -g -O3
   x86_64-linux-gnu-gcc-12 /src/recursion.c -shared -fPIC -o /src/bin/recursion-x86_64-gcc12-r -O3
   clang-15 --version > /src/llvm15-x86_64.txt
   clang-15 -target x86_64-linux-gnu /src/inlines+loops.c -shared -fPIC -o /src/bin/inlines+loops-x86_64-llvm15-0 -g -O0
   clang-15 -target x86_64-linux-gnu /src/inlines+loops.c -shared -fPIC -o /src/bin/inlines+loops-x86_64-llvm15-d -g -Og
   clang-15 -target x86_64-linux-gnu /src/inlines+loops.c -shared -fPIC -o /src/bin/inlines+loops-x86_64-llvm15-dr -g -O3
   clang-15 -target x86_64-linux-gnu /src/inlines+loops.c -shared -fPIC -o /src/bin/inlines+loops-x86_64-llvm15-r -O3
   clang-15 -target x86_64-linux-gnu /src/recursion.c -shared -fPIC -o /src/bin/recursion-x86_64-llvm15-0 -g -O0
   clang-15 -target x86_64-linux-gnu /src/recursion.c -shared -fPIC -o /src/bin/recursion-x86_64-llvm15-d -g -Og
   clang-15 -target x86_64-linux-gnu /src/recursion.c -shared -fPIC -o /src/bin/recursion-x86_64-llvm15-dr -g -O3
   clang-15 -target x86_64-linux-gnu /src/recursion.c -shared -fPIC -o /src/bin/recursion-x86_64-llvm15-r -O3
   '

podman run --rm -v ./:/src --pull always docker.io/nvidia/cuda:10.2-devel-ubuntu18.04 sh -exc \
  'nvcc --version > /src/nvcc102.txt
   nvcc /src/inlines+loops.cu -cubin -arch=sm_62 -o /src/bin/inlines+loops-sm_62-nvcc102-0 -G
   nvcc /src/inlines+loops.cu -cubin -arch=sm_62 -o /src/bin/inlines+loops-sm_62-nvcc102-dr -lineinfo
   nvcc /src/inlines+loops.cu -cubin -arch=sm_62 -o /src/bin/inlines+loops-sm_62-nvcc102-r
   '

podman run --rm -v ./:/src --pull always docker.io/nvidia/cuda:11.2.2-devel-ubuntu20.04 sh -exc \
  'nvcc --version > /src/nvcc112.txt
   nvcc /src/inlines+loops.cu -cubin -arch=sm_75 -o /src/bin/inlines+loops-sm_75-nvcc112-0 -G
   nvcc /src/inlines+loops.cu -cubin -arch=sm_75 -o /src/bin/inlines+loops-sm_75-nvcc112-dr -lineinfo
   nvcc /src/inlines+loops.cu -cubin -arch=sm_75 -o /src/bin/inlines+loops-sm_75-nvcc112-r
   nvcc /src/recursion.cu -cubin -arch=sm_75 -o /src/bin/recursion-sm_75-nvcc112-0 -G
   nvcc /src/recursion.cu -cubin -arch=sm_75 -o /src/bin/recursion-sm_75-nvcc112-dr -lineinfo
   nvcc /src/recursion.cu -cubin -arch=sm_75 -o /src/bin/recursion-sm_75-nvcc112-r
   '

podman run --rm -v ./:/src --pull always docker.io/nvidia/cuda:11.8.0-devel-ubuntu22.04 sh -exc \
  'nvcc --version > /src/nvcc118.txt
   nvcc /src/inlines+loops.cu -cubin -arch=sm_87 -o /src/bin/inlines+loops-sm_87-nvcc118-0 -G
   nvcc /src/inlines+loops.cu -cubin -arch=sm_87 -o /src/bin/inlines+loops-sm_87-nvcc118-dr -lineinfo
   nvcc /src/inlines+loops.cu -cubin -arch=sm_87 -o /src/bin/inlines+loops-sm_87-nvcc118-r
   nvcc /src/recursion.cu -cubin -arch=sm_87 -o /src/bin/recursion-sm_87-nvcc118-0 -G
   nvcc /src/recursion.cu -cubin -arch=sm_87 -o /src/bin/recursion-sm_87-nvcc118-dr -lineinfo
   nvcc /src/recursion.cu -cubin -arch=sm_87 -o /src/bin/recursion-sm_87-nvcc118-r
   '

podman run --rm -v ./:/src --pull always docker.io/nvidia/cuda:12.0.0-devel-ubuntu22.04 sh -exc \
  'nvcc --version > /src/nvcc120.txt
   nvcc /src/inlines+loops.cu -cubin -arch=sm_90a -o /src/bin/inlines+loops-sm_90a-nvcc120-0 -G
   nvcc /src/inlines+loops.cu -cubin -arch=sm_90a -o /src/bin/inlines+loops-sm_90a-nvcc120-dr -lineinfo
   nvcc /src/inlines+loops.cu -cubin -arch=sm_90a -o /src/bin/inlines+loops-sm_90a-nvcc120-r
   nvcc /src/recursion.cu -cubin -arch=sm_90a -o /src/bin/recursion-sm_90a-nvcc120-0 -G
   nvcc /src/recursion.cu -cubin -arch=sm_90a -o /src/bin/recursion-sm_90a-nvcc120-dr -lineinfo
   nvcc /src/recursion.cu -cubin -arch=sm_90a -o /src/bin/recursion-sm_90a-nvcc120-r
   '
