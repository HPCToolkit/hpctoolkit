#include <stdio.h>
#define YESMPI 0
#ifdef YESMPI
#include <mpi.h>
#include <catamount/data.h>
#include <rca_lib.h>
#endif

void papi_pulse_fini(void);

long fib(long n) 
{
	if (n > 2) {
		return fib(n-1) + fib(n-2);
	} else return 1;
}
int main(int argc, char **argv)
{
	int myid;
#ifdef YESMPI
	rca_mesh_coord_t coords;
#endif
	long n, result; 

#ifdef YESMPI
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &myid);
#else
	myid = 0;
#endif

	if (argc < 2) {
	    if (myid == 0) printf("not enough arguments\n");
#ifdef YESMPI
	    MPI_Finalize();
#endif
	    exit(-1);
	}
	n = atoi(argv[1]);

#ifdef YESMPI
	rca_get_meshcoord(_my_pnid, &coords);
	printf("MPI rank %d maps to physical node %d (0x%x), mesh coordinate = [%d,%d,%d]\n", 
		myid, _my_pnid, _my_pnid, coords.mesh_x, coords.mesh_y, coords.mesh_z);
#else
	printf("MPI rank %d \n", myid );
#endif

#if 0
	result = 0;
#else
	result = fib(n);
#endif

#ifdef YESMPI
	MPI_Finalize();
#endif
	if (myid == 0) printf("fib(%ld) = %ld\n",n, result);
#if 0
	papi_pulse_fini();
#endif
}

