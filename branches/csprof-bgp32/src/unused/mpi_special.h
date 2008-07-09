#ifndef MPI_SPECIAL_H
#define MPI_SPECIAL_H

#ifdef MPI_SPECIAL
#define STATIC_RANK 0
static int chosen_rank = STATIC_RANK;

#define MPI_SPECIAL_SKIP() do { \
  if (csprof_mpirank() < 0){ \
    PMSG(SAMPLE,"sample before mpi_init"); \
    return; \
  } \
  if (csprof_mpirank() != chosen_rank){ \
    PMSG(SAMPLE,"got sample event, but csprof_rank = %d",csprof_mpirank());\
    return; \
  } \
} while(0)
#else
#define MPI_SPECIAL_SKIP()
#endif
#endif
