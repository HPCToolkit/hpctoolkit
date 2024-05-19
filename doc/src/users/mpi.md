<!--
SPDX-FileCopyrightText: 2002-2023 Rice University
SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project

SPDX-License-Identifier: CC-BY-4.0
-->

# Monitoring MPI Applications

HPCToolkit's measurement subsystem can measure each process and thread in an execution of an MPI program.
HPCToolkit can be used with pure MPI programs as well as hybrid programs that use multithreading, e.g. OpenMP or Pthreads, within MPI processes.

HPCToolkit supports C, C++ and Fortran MPI programs.
It has been successfully tested with MPICH, MVAPICH and OpenMPI and should work with almost all MPI implementations.

## Running and Analyzing MPI Programs

For a dynamically linked application binary `app`, use a command line similar to the following example:

> `<mpi-launcher> hpcrun -e <event>:<period> ... app [app-arguments]`

Observe that the MPI launcher (`mpirun`, `mpiexec`, etc.) is used to launch `hpcrun`, which is then used to launch the application program.

To use HPCToolkit to monitor statically linked binaries, use `hpclink` to build a statically linked version of your application that includes HPCToolkit's monitoring library.
For example, to link your application binary `app`:

> ```
> hpclink <linker> -o app <linker-arguments>
> ```

Then, set the `HPCRUN_EVENT_LIST` environment variable in the launch script before running the application:

> ```
> export HPCRUN_EVENT_LIST="CYCLES@f200"
> <mpi-launcher> app [app-arguments]
> ```

See the Chapter [6](#chpt:statically-linked-apps) for more information.

In this example, `s3d_f90.x` is the Fortran S3D program compiled with OpenMPI and run with the command line

> `mpiexec -n 4 hpcrun -e PAPI_TOT_CYC:2500000 ./s3d_f90.x`

This produced 12 files in the following abbreviated `ls` listing:

> ```
> krentel 1889240 Feb 18  s3d_f90.x-000000-000-72815673-21063.hpcrun
> krentel    9848 Feb 18  s3d_f90.x-000000-001-72815673-21063.hpcrun
> krentel 1914680 Feb 18  s3d_f90.x-000001-000-72815673-21064.hpcrun
> krentel    9848 Feb 18  s3d_f90.x-000001-001-72815673-21064.hpcrun
> krentel 1908030 Feb 18  s3d_f90.x-000002-000-72815673-21065.hpcrun
> krentel    7974 Feb 18  s3d_f90.x-000002-001-72815673-21065.hpcrun
> krentel 1912220 Feb 18  s3d_f90.x-000003-000-72815673-21066.hpcrun
> krentel    9848 Feb 18  s3d_f90.x-000003-001-72815673-21066.hpcrun
> krentel  147635 Feb 18  s3d_f90.x-72815673-21063.log
> krentel  142777 Feb 18  s3d_f90.x-72815673-21064.log
> krentel  161266 Feb 18  s3d_f90.x-72815673-21065.log
> krentel  143335 Feb 18  s3d_f90.x-72815673-21066.log
> ```

Here, there are four processes and two threads per process.
Looking at the file names, `s3d_f90.x` is the name of the program binary, `000000-000` through `000003-001` are the MPI rank and thread numbers, and `21063` through `21066` are the process IDs.

We see from the file sizes that OpenMPI is spawning one helper thread per process.
Technically, the smaller `.hpcrun` files imply only a smaller calling-context tree (CCT), not necessarily fewer samples.
But in this case, the helper threads are not doing much work.

Just one thing.
Early in the program, preferably right after `MPI_Init()`, the program should call `MPI_Comm_rank()` with communicator `MPI_COMM_WORLD`.
Nearly all MPI programs already do this, so this is rarely a problem.
For example, in C, the program might begin with:

> ```
> int main(int argc, char **argv)
> {
>     int size, rank;
>
>     MPI_Init(&argc, &argv);
>     MPI_Comm_size(MPI_COMM_WORLD, &size);
>     MPI_Comm_rank(MPI_COMM_WORLD, &rank);
>     ...
> }
> ```

*Note:* The first call to `MPI_Comm_rank()` should use `MPI_COMM_WORLD`.
This sets the process's MPI rank in the eyes of `hpcrun`.
Other communicators are allowed, but the first call should use `MPI_COMM_WORLD`.

Also, the call to `MPI_Comm_rank()` should be unconditional, that is all processes should make this call.
Actually, the call to `MPI_Comm_size()` is not necessary (for `hpcrun`), although most MPI programs normally call both `MPI_Comm_size()` and `MPI_Comm_rank()`.

Although the matrix of all possible MPI variants, versions, compilers, architectures and systems is very large, HPCToolkit has been tested successfully with MPICH, MVAPICH and OpenMPI and should work with most MPI implementations.

C, C++ and Fortran are supported.

## Building and Installing HPCToolkit

No, HPCToolkit is designed to work with multiple MPI implementations at the same time.
That is, you don't need to provide an `mpi.h` include path, and you don't need to compile multiple versions of HPCToolkit, one for each MPI implementation.

The technically-minded reader will note that each MPI implementation uses a different value for `MPI_COMM_WORLD` and may wonder how this is possible.
`hpcrun` (actually `libmonitor`) waits for the application to call `MPI_Comm_rank()` and uses the same communicator value that the application uses.
This is why we need the application to call `MPI_Comm_rank()` with communicator `MPI_COMM_WORLD`.
