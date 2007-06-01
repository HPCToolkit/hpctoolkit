/* -*-Mode: C;-*- */

/****************************************************************************
//
// Nathan Tallent
//
*****************************************************************************/

#ifndef my_gauss_h
#define my_gauss_h

/************************** System Include Files ****************************/

#if defined(MY_PTHREAD)
#  include <pthread.h>
#endif

/*************************** User Include Files *****************************/

/**************************** Forward Declarations **************************/

/****************************************************************************
 * Gaussian Elimination with Partial Pivoting
 *
 * Given a system of linear equations Ax = b create Ux = y where U is
 * upper triangular.  A is converted to U _in place_.
 *
 * The algorithm:
 * 1. is designed for row-major layout.
 * 2. executes swaps to implement the partial pivoting (as opposed to
 *    recording a permutation vector)
 *
 ****************************************************************************/


#if defined(__cplusplus)
extern "C" {
#endif

#if defined(__UPC__)
  void
  GE_Driver_UPC(shared double* shared* const restrict A, 
		shared double* const restrict x,
		shared double* const restrict b, int n);
#elif defined(_OPENMP)
  void
  GE_Driver_OMP(double** const restrict A, 
		double* const restrict x,
		double* const restrict b, int n);
#elif defined(MY_PTHREAD)
  extern pthread_key_t KEY_ThreadID;
  void
  GE_Driver_PThreads(double** const restrict A, 
		     double* const restrict x,
		     double* const restrict b, int n);
#endif

/****************************************************************************
 * Gaussian Elimination
 ****************************************************************************/

void
GaussianElimination(SHARED double* SHARED* const restrict A, 
		    SHARED double* const restrict b, int n);


#define GAUSS_SWAP_SCALAR(x,y,t) \
  { (t) = (x); (x) = (y); (y) = (t); }


/****************************************************************************
 * Gaussian Elimination pipeline dependency tracking 
 ****************************************************************************/

// NOTE: can support up to 32 threads
typedef struct RowDep_s {
  volatile int pivot;     // pivot point for row k; k if no swapping occured
  volatile uint32_t done; // threads who have completed
} RowDep_t;

#define RowDep_pivot_NULL -1
#define RowDep_done_NULL 0x0

#define RowDep_Init(x) \
  { x.pivot = RowDep_pivot_NULL; x.done = RowDep_done_NULL; }


typedef struct TheDeps_s {
  SHARED RowDep_t* deps;
  int n;
  uint32_t RowDep_done_ALL; /* 0xffffffff;  32 bits; (2^T - 1) */

} TheDeps_t;


#ifdef __UPC__
#  define TheDeps_Init_Helper(x, n) \
    {                                                             \
      int blksz = sizeof(RowDep_t);                               \
      int nblk  = n + 1;                                          \
      (x).deps = (shared RowDep_t*)upc_all_alloc(nblk, blksz);    \
    }                                                             \
    upc_barrier;                                                  \
    upc_forall (i = 0; i < (n+1); ++i; i) {                       \
      RowDep_Init((x).deps[i]);                                   \
    }
#else
#  define TheDeps_Init_Helper(x, n) \
    (x).deps = (RowDep_t*)malloc(sizeof(RowDep_t)*(n+1));         \
    for (i = 0; i < (n+1); ++i) {                                 \
      RowDep_Init((x).deps[i]);                                   \
    }
#endif


#define TheDeps_Init(x, N, TH) \
  {                                                               \
    int i;                                                        \
    TheDeps_Init_Helper(x, N);                                    \
    (x).n = N;                                                    \
    (x).RowDep_done_ALL = (0x1 << (TH)) - 1; /* 2^TH - 1 */       \
    (x).deps[0].done = (x).RowDep_done_ALL;                       \
  }

#define TheDeps_Fini(x) \
  {                                                               \
    FREE((x).deps);                                               \
    (x).deps = NULL; (x).n = 0;                                   \
  }

#define TheDeps_isPivotNULL(x, k) \
  ((x).deps[(k)].pivot == RowDep_pivot_NULL)
#define TheDeps_isReadyToFinish(x, k) \
  ((x).deps[(k)].pivot != RowDep_pivot_NULL)
#define TheDeps_pivot(x, k) \
  ((x).deps[(k)].pivot)

#define TheDeps_done(x, k) \
  ((x).deps[(k)].done)
#define TheDeps_isDone(x, k, th) \
  ((x).deps[(k)].done & (0x1 << th))
#define TheDeps_isallDone(x, k) \
  ((x).deps[(k)].done == (x).RowDep_done_ALL)
#define TheDeps_setDone(x, k, th) \
  ((x).deps[(k)].done |= (0x1 << th))

#define TheDeps_DUMP(x, th) \
  { int i;                                                                  \
    printf("DEPS [T%d]: ", th);                                             \
    for (i = 0; i <= (x).n; ++i) {                                          \
      printf("(%d %#x) ", TheDeps_pivot(x, i), TheDeps_done(x, i));         \
    }                                                                       \
    printf("\n");                                                           \
  }


/****************************************************************************/

#if !defined(MIN)
#  define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif
#if !defined(MAX)
#  define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

/****************************************************************************
 * From main.c 
 ****************************************************************************/

extern int OPT_NTHREADS;
extern int OPT_DEBUG;
extern int OPT_VERBOSE;
extern SHARED PerfInfo_t GLBL_PerfInfo;

void
Init_Ax_b(SHARED double* SHARED* restrict A, 
	  SHARED double* restrict x, 
	  SHARED double* restrict b, int n);

void
Dump_Ax_b(SHARED double* SHARED* const restrict A, 
	  SHARED double* const restrict x, 
	  SHARED double* const restrict b, int n);


#if defined(__cplusplus)
} /* extern "C" */
#endif


/****************************************************************************/

#endif /* my_gauss_h */
