/*
 * The MPI hello, world program.
 *
 * This program can cause problems for hpcrun with the OpenMPI
 * compiler.
 *
 * $Id$
 */
#include <mpi.h>
#include <stdio.h>

int
main(int argc, char **argv)
{
    int size, rank;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    printf("main: mpi size = %d, rank = %d\n", size, rank);
    MPI_Finalize();

    return (0);
}
