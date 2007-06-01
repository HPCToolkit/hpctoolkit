/* -*-Mode: C;-*- */

/****************************************************************************
//
// Gaussian Elimination Driver
//
// Nathan Tallent
// COMP 422, Assignment 2, Gaussian Elimination
// 
*****************************************************************************/

/************************** System Include Files ****************************/

#define _SGI_REENTRANT_FUNCTIONS
#include <stdlib.h> /* for random() */
#define __USE_POSIX2
#include <unistd.h> /* for getopt(); cf. stdlib.h */
#include <stdio.h>
#include <string.h> /* for memset() */
#include <math.h>
#include <inttypes.h>

/*************************** User Include Files *****************************/

#include "gauss.h"
#include "perf.h"
#include "upc_helper.h"

/**************************** Forward Declarations **************************/

const char* OPT_CMD = NULL;
int OPT_DEBUG = 0;
int OPT_VERBOSE = 0;
int OPT_N = 5;
int OPT_NTHREADS = 1;

SHARED PerfInfo_t GLBL_PerfInfo;

// Use this to get around UPC.  UPC complains that taking the address
// of one of these pointers amounts to converting a shared pointer to
// a private pointer.
typedef struct GE_data_s {
  SHARED double* SHARED* restrict A;
  SHARED double* restrict x;
  SHARED double* restrict b;
  int n;
} GE_data_t;

static void 
ParseOptions(int argc, char* const argv[]);

static void
Alloc_Ax_b(GE_data_t* D);

static void
Dealloc_Ax_b(GE_data_t* D);

void
Init_Ax_b(SHARED double* SHARED* restrict A, 
	  SHARED double* restrict x, 
	  SHARED double* restrict b, int n);

extern void
Dump_Ax_b(SHARED double* SHARED* const restrict A, 
	  SHARED double* const restrict x, 
	  SHARED double* const restrict b, int n);

static void
BackSubstitution(SHARED double* SHARED* const restrict A, 
		 SHARED double* const restrict x, 
		 SHARED double* const restrict b, int n);

static double*
Residual(SHARED double* SHARED* const restrict A, 
	 SHARED double* const restrict x, 
	 SHARED double* const restrict b, int n);

static double
L2_norm(double* const restrict x, int n);


/****************************************************************************
 * Driver
 ****************************************************************************/

int 
main(int argc, char * const argv[])
{
  GE_data_t D;
  double* r;
  double norm;

#ifdef __UPC__
  upc_lock_t* ParseOptions_lock;
  ParseOptions_lock = upc_all_lock_alloc();
  upc_lock(ParseOptions_lock);
#endif
  ParseOptions(argc, argv);
  D.n = OPT_N;
#ifdef __UPC__
  upc_unlock(ParseOptions_lock);
  upc_barrier;

  if (MYTHREAD == 0) {
  upc_lock_free(ParseOptions_lock);
#endif
  PerfInfo_init(&GLBL_PerfInfo);
#ifdef __UPC__
  }
#endif

  // 1. Allocate matrix
  Alloc_Ax_b(&D);

  // 2. Initialize and Apply Gaussian Elimination. 
  //    We use this this 'unclean' division in order to take advantage
  //    of the IRIX's first-touch page placement.
#if defined(__UPC__)
  upc_barrier; // ensure all allocation is done
  GE_Driver_UPC(D.A, D.x, D.b, D.n);
#elif defined(_OPENMP)
  GE_Driver_OMP(D.A, D.x, D.b, D.n);
#elif defined(MY_PTHREAD)
  GE_Driver_PThreads(D.A, D.x, D.b, D.n);
#else
  Init_Ax_b(D.A, D.x, D.b, D.n);
  if (OPT_DEBUG >= 1) { Dump_Ax_b(D.A, D.x, D.b, D.n); }

  PerfInfo_startTimer(&GLBL_PerfInfo);
  GaussianElimination(D.A, D.b, D.n);
  PerfInfo_stopTimer(&GLBL_PerfInfo);
#endif

#ifdef __UPC__
  if (MYTHREAD == 0) {
#endif
    
  // 3. Use back-substitution to compute L2-norm of residual
  if (OPT_DEBUG >= 1) { Dump_Ax_b(D.A, D.x, D.b, D.n); }
  BackSubstitution(D.A, D.x, D.b, D.n);
  if (OPT_DEBUG >= 1 || OPT_VERBOSE >= 2) { Dump_Ax_b(D.A, D.x, D.b, D.n); }
  r = Residual(D.A, D.x, D.b, D.n);
  norm = L2_norm(r, D.n);
  fprintf(stdout, "\nL2-norm of residual: %.10e\n", norm);
  
  // 4. Print performance information
  PerfInfo_dump(&GLBL_PerfInfo, stdout);
  
  Dealloc_Ax_b(&D);
  free(r);
  
#ifdef __UPC__
  }
  upc_barrier;
#endif
  
  return 0;
}


/****************************************************************************
 * Helpers
 ****************************************************************************/

static void 
PrintUsage(FILE* fs)
{
#ifdef __UPC__
  if (MYTHREAD == 0) {
#endif
  fprintf(stderr, "Usage:\n  %s [-d <debug-level>] [-v <verbose-level> [-t <num-threads>] <matrix-size>\n", OPT_CMD);
#ifdef __UPC__
  }
#endif

}


static void 
PrintError(FILE* fs, const char* msg)
{
#ifdef __UPC__
  if (MYTHREAD == 0) {
#endif
  fprintf(fs, "%s: %s\n", OPT_CMD, msg);
  fprintf(fs, "Try `%s -h' for more information.\n", OPT_CMD);
#ifdef __UPC__
  }
#endif
}


static void 
ParseOptions(int argc, char * const argv[])
{
  int c;
  
  OPT_CMD = argv[0];
  
  getoptreset(); // for UPC! This is not reentrant!
  while ((c = getopt(argc, argv, "hd:v:t:")) != EOF) {
    switch (c) {
      
    // Special options that are immediately evaluated
    case 'h': {
      PrintUsage(stderr);
      EXIT(0); 
      break; 
    }
    case 'd': { // debug
      OPT_DEBUG = atoi(optarg);
      if (! (OPT_DEBUG >= 0) ) {
	PrintError(stderr, "Invalid debug level.");
	EXIT(1);
      }
      break; 
    }
    case 'v': { // verbose
      OPT_VERBOSE = atoi(optarg);
      if (! (OPT_VERBOSE >= 0) ) {
	PrintError(stderr, "Invalid verbosity level.");
	EXIT(1);
      }
      break; 
    }
    
    // Other options
    case 't': {
      OPT_NTHREADS = atoi(optarg);
      if (! (OPT_NTHREADS >= 1) ) {
	PrintError(stderr, "Invalid number of threads.");
	EXIT(1);
      }
      break; 
    }
    
    // Error
    case ':':
    case '?': {
      PrintError(stderr, "Error parsing arguments.");
      EXIT(1);
      break; 
    }
    }
  }
  
  /* Get required argumnets */
  if (argv[optind] == NULL) {
    PrintError(stderr, "Missing argument.");
    EXIT(1);
  }
  
  /* Matrix size */
  OPT_N = atoi(argv[optind]);
  if (! (OPT_N > 0) ) {
    PrintError(stderr, "Invalid matrix size");
    EXIT(1);
  }
}


// Allocate A, x and b.
static void
Alloc_Ax_b(GE_data_t* D)
{
  int i;
  
#ifdef __UPC__
  int blksz_sdp = sizeof(shared double*);
  int blksz_d = sizeof(double);
  int nblk = D->n;
  
  D->A = (shared double* shared*)upc_all_alloc(nblk, blksz_sdp);
  D->x = (shared double*)upc_all_alloc(nblk, blksz_d);
  D->b = (shared double*)upc_all_alloc(nblk, blksz_d);
  
  upc_barrier;
  // GOOD, but WRONG distribution.
  for (i = 0; i < D->n; ++i) {
    D->A[i] = (shared double*)upc_all_alloc(nblk, blksz_d);
  }

  // BAD, but 'right' distribution.
  // upc_forall (i = 0; i < D->n; ++i; i) {
  //   D->A[i] = (shared double*)upc_alloc(nblk * blksz_d); // set affinity
  // }

#if 0
  // Sanity check the allocation results
  upc_lock_t* lock = upc_all_lock_alloc();
  upc_barrier;
  upc_lock(lock);
  printf("[T%d] A[:] = %#x [", MYTHREAD, upc_addrfield((shared void*)D->A));
  for (i = 0; i < D->n; ++i) {
    printf("%#x ", upc_addrfield((shared void*)D->A[i]));
  }
  printf("]\n");
  upc_unlock(lock);
  
  upc_barrier;
  EXIT(0);
#endif

#else
  D->A = (double**)malloc(sizeof(double*)*D->n);
  for (i = 0; i < D->n; ++i) {
    D->A[i] = (double*)malloc(sizeof(double)*D->n);
  }
  
  D->x = (double*)malloc(sizeof(double)*D->n);
  D->b = (double*)malloc(sizeof(double)*D->n);
#endif
}


// Deallocate A, x and b.
static void
Dealloc_Ax_b(GE_data_t* D)
{
  int i;
  for (i = 0; i < D->n; ++i) {
    FREE(D->A[i]);
  }
  FREE(D->A);
  
  FREE(D->x);
  FREE(D->b);

  D->A = NULL;
  D->x = D->b = NULL;
}


// Initialize A, x and b.  Elements of A and b are between 1 and 1000.
//
// NOTE: This is important because of IRIX's first-touch page
// placement policy.
void
Init_Ax_b(SHARED double* SHARED* restrict A, 
	  SHARED double* restrict x, 
	  SHARED double* restrict b, int n)
{
#define RAND_BETWEEN_1_1000 \
  ((((double)rand_r(&seed) / (double)RAND_MAX) * 999.0) + 1.0)
  
  unsigned seed = 1;
  
  int i, j;
  int i_beg  = 0;
  int i_incr = 1;
  
#if defined(__UPC__)
  seed = (unsigned)(n + 1);
  if (THREADS > 1) { rand_r(&seed); }
  
  upc_forall (i = 0; i < n; ++i; i) {
    seed = (unsigned)i; // for UPC sanity -- could do better
    for (j = 0; j < n; ++j) {
      A[i][j] = RAND_BETWEEN_1_1000;
    }
  }
  
  upc_forall (i = 0; i < n; ++i; i) {
    seed = (unsigned)i; // for UPC sanity -- could do better
    b[i] = RAND_BETWEEN_1_1000;
    x[i] = 0.0;
  }
#else
  
# if defined(MY_PTHREAD)
  {
    int th = (int)pthread_getspecific(KEY_ThreadID); // my thread id
    i_beg = th;
    i_incr = OPT_NTHREADS; // set in GE_Driver_PThreads()
  }
# endif

# pragma omp for schedule(static,1) private(i,j)
  for (i = i_beg; i < n; i += i_incr) {
    seed = (unsigned)i;
    for (j = 0; j < n; ++j) {
      A[i][j] = RAND_BETWEEN_1_1000;
    }
  }
  
# pragma omp for schedule(static) private(i)
  for (i = i_beg; i < n; i += i_incr) {
    seed = (unsigned)i;
    b[i] = RAND_BETWEEN_1_1000;
    x[i] = 0.0;
  }
#endif

  // memset(x, '\0', sizeof(double)*n); // UPC does not like this
#undef RAND_BETWEEN_1_1000
}


// Dump A, x and b.
void
Dump_Ax_b(SHARED double* SHARED* const restrict A, 
	  SHARED double* const restrict x, 
	  SHARED double* const restrict b, int n)
{
  int i, j;
  //fprintf(stdout, "%*50c\n", '-');
  fprintf(stdout, "-------------------------------------------------------\n");
  for (i = 0; i < n; ++i) {
    for (j = 0; j < n; ++j) {
      fprintf(stdout, "% 5.3f ", A[i][j]);
    }
    if (x) { fprintf(stdout, "| % 5.3f ", x[i]); }
    if (b) { fprintf(stdout, "| % 5.3f ", b[i]); }
    fprintf(stdout, "\n");
  }
}


// Given an n x n matrix A in upper triangular form, a vector x and a
// vector b, compute x (in place) using back-substitution.
// x[i] = b[i] = SUM_{j = i+1}^{k} ( A[i][j] * x[j] )
static void
BackSubstitution(SHARED double* SHARED* const restrict A, 
		 SHARED double* const restrict x, 
		 SHARED double* const restrict b, int n)
{
  int i, j;
  
  for (i = n - 1; i >= 0; --i) {
    x[i] = b[i];
    for (j = i + 1; j < n; ++j) {
      x[i] -= A[i][j] * x[j];
    }
  }
}


// Given an n x n matrix A, a vector x and a vector b, compute and
// return the residual vector r where
//   r = Ax - b 
static double*
Residual(SHARED double* SHARED* const restrict A, 
	 SHARED double* const restrict x, 
	 SHARED double* const restrict b, int n)
{
  double* r;
  int i, j;
  
  r = (double*)malloc(sizeof(double)*n);

  /* Compute Ax */
  for (i = 0; i < n; ++i) {
    r[i] = 0; /* initialize r[i] */
    for (j = 0; j < n; ++j) {
      r[i] += A[i][j] * x[j];
    }
  }
  
  /* Compute Ax - b */
  for (i = 0; i < n; ++i) {
    r[i] = r[i] - b[i];
  }
  
  return r;
}


// Given a vector x of size 'n', return its L2-norm: 
//   sqrt(x_1^2 + ... + x_n^2)
static double
L2_norm(double* const restrict x, int n)
{
  double norm = 0;
  int i;
  for (i = 0; i < n; ++i) {
    norm += (x[i] * x[i]);
  }
  norm = sqrt(norm);
  return norm;
}
