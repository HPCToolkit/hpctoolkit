#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int Driver_VprofTest2();

int main()
{
  fputs("Running test2: vprof test2\n", stderr);
  Driver_VprofTest2();

  return 0;
}

void
test_dgemm(int transa, int transb, int m, int n, int l,
           double *a, int lda, double *b, int ldb,
           double *c, int ldc);

int
Driver_VprofTest2()
{
  int n = 300;
  int i;

  double *a = malloc(n*n*sizeof(double));
  double *b = malloc(n*n*sizeof(double));
  double *c = malloc(n*n*sizeof(double));

  for (i=0; i<5; ++i) {
      test_dgemm(0,0,n,n,n,a,n,b,n,c,n);
      test_dgemm(0,1,n,n,n,a,n,b,n,c,n);
      test_dgemm(1,0,n,n,n,a,n,b,n,c,n);
      test_dgemm(1,1,n,n,n,a,n,b,n,c,n);
    }

  free(a);
  free(b);
  free(c);

  return 0;
}



double
u01()
{
  static unsigned long seed = 123456789;
  seed = seed * 12345 + 1234567;
  return (seed % 256)/255.0;
}

void
ref_dgemm_tt(int m, int n, int l,
             double alpha, double *a, int lda,
             double *b, int ldb,
             double beta, double *c, int ldc)
{
  int i, j, k;
  for (i=0; i<m; i++) {
      for (j=0; j<n; j++) {
          double tmp = 0.0;
          for (k=0; k<l; k++) {
              tmp += a[k*lda+i] * b[j*ldb+k];
            }
          c[i*ldc+j] += tmp;
        }
    }
}

void
ref_dgemm_tn(int m, int n, int l,
             double alpha, double *a, int lda,
             double *b, int ldb,
             double beta, double *c, int ldc)
{
  int i, j, k;
  for (i=0; i<m; i++) {
      for (j=0; j<n; j++) {
          double tmp = 0.0;
          for (k=0; k<l; k++) {
              tmp += a[k*lda+i] * b[k*ldb+j];
            }
          c[i*ldc+j] += tmp;
        }
    }
}

void
ref_dgemm_nt(int m, int n, int l,
             double alpha, double *a, int lda,
             double *b, int ldb,
             double beta, double *c, int ldc)
{
  int i, j, k;
  for (i=0; i<m; i++) {
      for (j=0; j<n; j++) {
          double tmp = 0.0;
          for (k=0; k<l; k++) {
              tmp += a[i*lda+k] * b[j*ldb+k];
            }
          c[i*ldc+j] += tmp;
        }
    }
}

void
ref_dgemm_nn(int m, int n, int l,
             double alpha, double *a, int lda,
             double *b, int ldb,
             double beta, double *c, int ldc)
{
  int i, j, k;
  for (i=0; i<m; i++) {
      for (j=0; j<n; j++) {
          double tmp = 0.0;
          for (k=0; k<l; k++) {
              tmp += a[i*lda+k] * b[k*ldb+j];
            }
          c[i*ldc+j] += tmp;
        }
    }
}

void
ref_dgemm(int ta, int tb, int m, int n, int l,
          double alpha, double *a, int lda,
          double *b, int ldb,
          double beta, double *c, int ldc)
{
  if (l == 0) {
    int i,j;
    for (i=0; i<m; i++) {
      for (j=0; j<n; j++) {
        c[i*ldc+j] *= beta;
        }
      }
    return;
    }
  if (ta) {
    if (tb) ref_dgemm_tt(m, n, l, alpha, a, lda, b, ldb, beta, c, ldc);
    else    ref_dgemm_tn(m, n, l, alpha, a, lda, b, ldb, beta, c, ldc);
    }
  else if (tb) {
      ref_dgemm_nt(m, n, l, alpha, a, lda, b, ldb, beta, c, ldc);
    }
  else ref_dgemm_nn(m, n, l, alpha, a, lda, b, ldb, beta, c, ldc);
}

void
randomize_matrix(int trans, int m, int n, double *a, int lda)
{
  int i, j;
  if (trans) { int tmp = m; m = n; n = tmp; }
  for (i=0; i<m; i++)
      for (j=0; j<n; j++)
          a[i*lda+j] = 100.0*u01();
}

void
test_dgemm(int transa, int transb, int m, int n, int l,
           double *a, int lda, double *b, int ldb,
           double *c, int ldc)
{
  randomize_matrix(0, m, n, c, ldc);
  randomize_matrix(transa, m, l, a, lda);
  randomize_matrix(transb, l, n, b, ldb);
  ref_dgemm(transa, transb, m, n, l, 1.5, a, lda, b, ldb, 0.33, c, ldc);
}

//***************************************************************************
