This directory contains a series of small binaries, each built from a single source file. The following suffixes are
used to mark which compiler and options were used to compile the binary:

|    Suffix    |   Compiler   | X=`0`          | X=`d`    | X=`dr`      | X=`r` |
| ------------:| ------------ | -------------- | -------- | ----------- | ----- |
| `-gcc12-X`   | GCC 12.2.0   | `-g -O0`       | `-g -Og` | `-g -O3`    | `-O3` |
| `-gcc10-X`   | GCC 10.2.1   |
| `-gcc8-X`    | GCC 8.3.0    |
| `-llvm15-X`  | Clang 15.0.6 |
| `-llvm11-X`  | Clang 11.0.1 |
| `-llvm7-X`   | Clang 7.0.1  |
| |
| `-nvcc120-X` | NVCC 12.0.76  | `-G -lineinfo` | N/A      | `-lineinfo` | (None) |
| `-nvcc117-X` | NVCC 11.7.89  |
| `-nvcc112-X` | NVCC 11.2.152 |
| `-nvcc102-X` | NVCC 10.2.89  |

The following source files are compiled as above:
  - `inlines+loops.*`: Inlined functions and loops, interleaved in a few different patterns.
  - `recursion.*`: Recursive functions and call graphs.

All binaries can be built from scratch by running `./build.sh`, provided a working Podman installation.
