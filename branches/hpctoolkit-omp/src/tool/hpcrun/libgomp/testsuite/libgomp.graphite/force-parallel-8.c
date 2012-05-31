#define N 1500

int foo(void)
{
  int i, j;
  int x[N][N], y[N];

  for (i = 0; i < N; i++)
    {
      y[i] = i;

      for (j = 0; j < N; j++)
	{
	  if (j > 500)
	    {
	      x[i][j] = i + j + 3;
	      y[j] = i*j + 10;
	    }
	  else
	    x[i][j] = x[i][j]*3;
	}
    }

  return x[2][5]*y[8];
}

int main(void)
{
  foo();

  return 0;
}

/* Check that parallel code generation part make the right answer.  */
/* { dg-final { scan-tree-dump-times "2 loops carried no dependency" 1 "graphite" } } */
/* { dg-final { cleanup-tree-dump "graphite" } } */
/* { dg-final { scan-tree-dump-times "loopfn.0" 5 "optimized" } } */
/* { dg-final { scan-tree-dump-times "loopfn.1" 5 "optimized" } } */
/* { dg-final { cleanup-tree-dump "parloops" } } */
/* { dg-final { cleanup-tree-dump "optimized" } } */
