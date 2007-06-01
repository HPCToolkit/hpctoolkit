/* -*-Mode: C;-*- */

/****************************************************************************
//
// Gaussian Elimination
//
// Nathan Tallent
// COMP 422, Assignment 2, Gaussian Elimination
// 
*****************************************************************************/

/************************** System Include Files ****************************/

#include <pthread.h>

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <inttypes.h>

/*************************** User Include Files *****************************/

#include "gauss.h"

/**************************** Forward Declarations **************************/

#define MY_DEBUG 0

void
GE(double** const restrict A, 
   double* const restrict b, int n, 
   int num_threads, TheDeps_t* deps);

static void
GE_FinishRow(int call_k, int todo_k, 
	     double** const restrict A, 
	     double* const restrict b, int n,
	     int TH, int th, TheDeps_t* deps);

void* 
GE_pthread(void* x);

/****************************************************************************
 * Gaussian Elimination (see gauss.h for general description)
 ****************************************************************************/

static TheDeps_t DEPS;
static pthread_mutex_t GE_lock_b = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t GE_lock_deps = PTHREAD_MUTEX_INITIALIZER;

static atomic_reservoir_t GE_AtomicRes;
static atomic_var_t* GE_barrier1, *GE_barrier2;

pthread_key_t KEY_ThreadID;

#define MyBarrier(var, TH)                           \
  {                                                  \
    atomic_fetch_and_increment(var);                 \
    while (atomic_load(var) != TH) { ; }             \
  }


static int THE_TH = 0;
static double** restrict THE_A;
static double* restrict THE_x;
static double* restrict THE_b;
static int THE_n;


void
GE_Driver_PThreads(double** const restrict A, 
		   double* const restrict x,
		   double* const restrict b, int n)
{
  int i;
  pthread_t* threads;
  pthread_attr_t attr;

  THE_A = A; THE_x = x; THE_b = b; THE_n = n;

  GE_AtomicRes = atomic_alloc_reservoir(USE_DEFAULT_PM, 2, NULL);
  GE_barrier1 = atomic_alloc_variable(GE_AtomicRes, NULL);
  GE_barrier2 = atomic_alloc_variable(GE_AtomicRes, NULL);
  atomic_store(GE_barrier1, 0);
  atomic_store(GE_barrier2, 0);

  // NOTE: Do not allow more than n threads.
  THE_TH = MIN(n, OPT_NTHREADS);
  OPT_NTHREADS = THE_TH; // WARNING: note this assignment
  pthread_setconcurrency(THE_TH);
  
  threads = (pthread_t*)malloc(sizeof(pthread_t)*THE_TH);
  pthread_key_create(&KEY_ThreadID, NULL);
  
  pthread_attr_init(&attr);
  // pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
  for (i = 0; i < THE_TH; ++i) {
    pthread_create(&threads[i], &attr, GE_pthread, (void*)(int)i);
  }
  
  for (i = 0; i < THE_TH; ++i) {
    pthread_join(threads[i], NULL);
  }
  
  free(threads);
  pthread_key_delete(KEY_ThreadID);
  atomic_free_reservoir(GE_AtomicRes);
}


void* 
GE_pthread(void* x)
{
  int th = (int)x;
  
  // Store thread id in TSD
  pthread_setspecific(KEY_ThreadID, (void*)th);
 
  Init_Ax_b(THE_A, THE_x, THE_b, THE_n); 
  
  if (th == 0) { // could use pthread_once()
    if (OPT_DEBUG >= 1) { Dump_Ax_b(THE_A, THE_x, THE_b, THE_n); }
    PerfInfo_startTimer(&GLBL_PerfInfo);
    TheDeps_Init(DEPS, THE_n, THE_TH); 
  }
  MyBarrier(GE_barrier1, THE_TH);

  GaussianElimination(THE_A, THE_b, THE_n);
  
  MyBarrier(GE_barrier2, THE_TH);
  if (th == 0) { // could use pthread_once()
    TheDeps_Fini(DEPS); 
    PerfInfo_stopTimer(&GLBL_PerfInfo); 
  }
  
  pthread_exit((void *) NULL);
  return 0;
}


void
GaussianElimination(double** const restrict A, 
		    double* const restrict b, int n)
{
  // NOTE: we should already be in a parallel region
  int TH = THE_TH;
  GE(A, b, n, TH, &DEPS);
}


// Gaussian Elimination: 1-D pipelining by row, cyclic distribution
// [See write-up for more information]
void
GE(double** const restrict A, 
   double* const restrict b, int n, 
   int num_threads, TheDeps_t* deps)
{
  int j, k;
  int pivot_col;
  double T;
  volatile int done_k;
  
  int th = (int)pthread_getspecific(KEY_ThreadID); // my thread id
  int TH = num_threads; // total number of threads
  
  done_k = 0;
  
  // -------------------------------------------------------
  // Parallel For
  // -------------------------------------------------------
  for (k = th; k < n + TH; k += TH) {
    // NOTE: (k >= n) only for Wind-down
    
    // -----------------------------------------------------
    // Steady-state/Wind-down: Evaluate all dependencies for next iteration 
    // -----------------------------------------------------
    while (done_k < k && done_k < n) {
      while (!TheDeps_isReadyToFinish(*deps, done_k)) { ; }
      GE_FinishRow(k, done_k, A, b, n, TH, th, deps);
      done_k++;
    }
    
    // -----------------------------------------------------
    // Wind-up/Steady-state:
    // -----------------------------------------------------
    if (k < n) {      
      // 1. Select pivot column A[k,pivot_col] {local}
      pivot_col = k;
      for (j = k + 1; j < n; ++j) {
	if (abs(A[k][j]) > abs(A[k][pivot_col])) {
	  pivot_col = j;
	}
      }
      
      if (MY_DEBUG && OPT_DEBUG >= 2) {
	printf("==> It%d[T%d] (done:%d) pvt:%d\n", k, th, done_k, pivot_col);
      }
      if (MY_DEBUG && OPT_DEBUG >= 3) { Dump_Ax_b(A, NULL, b, n); }
            
      // 2. Swap on A and b {local}
      if (pivot_col != k) {
	// DEPENDENCY: cannot start swapping on k until all prior
	// eliminations of A and b are complete.
	while (!TheDeps_isallDone(*deps, k)) { ; }
	GAUSS_SWAP_SCALAR(A[k][k], A[k][pivot_col], T);
	
	pthread_mutex_lock(&GE_lock_b);
	GAUSS_SWAP_SCALAR(b[k], b[pivot_col], T);
	pthread_mutex_unlock(&GE_lock_b);
      }

      // 3. Eliminate on A and b {local}
      for (j = k + 1; j < n; ++j) {
	A[k][j] = A[k][j] / A[k][k];
      }
      b[k] = b[k] / A[k][k];
      A[k][k] = 1.0;

      // 3. Defer other swaps and eliminations
      
      // 4. Commit end-of-iteration tag
      TheDeps_pivot(*deps, k) = pivot_col; // atomic in hardware
    }
  }
}


static void
GE_FinishRow(int call_k, int todo_k, 
	     double** const restrict A, 
	     double* const restrict b, int n,
	     int TH, int th, TheDeps_t* deps)
{
  int i, j;
  double T;
  int a, k_beg, k_dep;
  int pivot_col = TheDeps_pivot(*deps, todo_k);
  
  // INVARIANT: At this point we are guaranteed that for *this thread*
  // all rows prior to todo_k have been finished.  Other threads may
  // not be at the same point and we must handle potential dependencies.
  //
  // DEPENDENCIES: We must not perform swaps for row todo_k unless
  // eliminations for any prior row j that would be affected by the
  // todo_k swaps are complete (where j < todo_k).  It is sufficient
  // to require that rows 0 ... todo_k-1 should all be processed, but
  // this is more than necessary.
  // 
  // Since I own every row [th + (a)(TH)] for a >= 0, all dependencies
  // for largest row k_dep such that:
  //   k_dep = [th + (a)(TH)] < todo_k
  // must be resolved.
  
  // 0a. Prior pivoting swaps for 'todo_k' (for my rows)
  //printf("...FinIt(%d->%d)[T%d] pvt:%d\n", call_k, todo_k, th, pivot_col);
  if (pivot_col != todo_k) {
    if (!TheDeps_isallDone(*deps, todo_k)) { 
      // INVARIANT: todo_k >= 1 since deps for row 0 are always satisified
      a = (todo_k - th) / TH; // use int division to get floor
      k_dep = th + a * TH;    // may be greater than todo_k - 1
      if ( !(k_dep < todo_k) ) { k_dep = todo_k-1; }
      printf("...FinIt(%d->%d)[T%d]: dep %d\n", call_k, todo_k, th, k_dep);
      while (!TheDeps_isallDone(*deps, k_dep)) { ; }
    }
    
    for (i = th; i < n; i += TH) {
      // We have already swapped row todo_k!
      if (i != todo_k) {
	//printf("...FinIt(%d->%d)[T%d]: swap %d\n", call_k, todo_k, th, i);
	GAUSS_SWAP_SCALAR(A[i][todo_k], A[i][pivot_col], T);
      }
    }
  }
  
  // 0b. Prior eliminations for 'todo_k' (for my rows)
  //
  // Since I own every row [th + (a)(TH)] for a >= 0, we want to begin
  // iteration at the smallest row, k_beg, such that:
  //   k_beg = [th + (a)(TH)] >= todo_k + 1
  // Below we solve for a and then k_beg.
  a = (int)ceil( (double)((todo_k+1) - th) / (double)TH );
  k_beg = th + a * TH;
  
  for (i = k_beg; i < n; i += TH) {
    //printf("...FinIt(%d->%d)[T%d]: elim %d\n", call_k, todo_k, th, i);
    for (j = todo_k + 1; j < n; ++j) {
      A[i][j] -= A[i][todo_k] * A[todo_k][j];
    }
    b[i] -= A[i][todo_k] * b[todo_k];
    A[i][todo_k] = 0.0;
  }
  
  pthread_mutex_lock(&GE_lock_deps);
  TheDeps_setDone(*deps, todo_k+1, th);
  pthread_mutex_unlock(&GE_lock_deps);
}

/****************************************************************************/
