#include <alloca.h>
#include <math.h>
#include <getopt.h>
#define __USE_XOPEN2K
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include "mcs-lock.h"
#include "spinlock.h"

static volatile int finished;
static volatile int total_sum;
static int *thread_sum;
static int *thread_odds;
static pthread_barrier_t barrier;
static pthread_mutex_t mutex;
static pthread_spinlock_t spinlock;
static mcs_lock_t m_c_s_lock;
static spinlock_t spin_lock;

static void *
sync_add_test(void *ndx)
{
  int i = (int)(intptr_t)ndx;
  int sum = 0;
  int num_odds = 0;
  pthread_barrier_wait(&barrier);
  while (!finished) {
    if (total_sum & 1)
      ++num_odds;
    __sync_add_and_fetch(&total_sum, 2);
    ++sum;
  }
  thread_sum[i] = sum;
  thread_odds[i] = num_odds;
  return NULL;
}

static void *
pthread_mutex_test(void *ndx)
{
  int i = (int)(intptr_t)ndx;
  int sum = 0;
  int num_odds = 0;
  pthread_barrier_wait(&barrier);
  while (!finished) {
    pthread_mutex_lock(&mutex);
    if (total_sum & 1)
      ++num_odds;
    ++total_sum;
    ++total_sum;
    pthread_mutex_unlock(&mutex);
    ++sum;
  }
  thread_sum[i] = sum;
  thread_odds[i] = num_odds;
  return NULL;
}

static void *
pthread_spin_test(void *ndx)
{
  int i = (int)(intptr_t)ndx;
  int sum = 0;
  int num_odds = 0;
  pthread_barrier_wait(&barrier);
  while (!finished) {
    pthread_spin_lock(&spinlock);
    if (total_sum & 1)
      ++num_odds;
    ++total_sum;
    ++total_sum;
    pthread_spin_unlock(&spinlock);
    ++sum;
  }
  thread_sum[i] = sum;
  thread_odds[i] = num_odds;
  return NULL;
}

static void *
pthread_mcs_test(void *ndx)
{
  int i = (int)(intptr_t)ndx;
  int sum = 0;
  int num_odds = 0;
  pthread_barrier_wait(&barrier);
  while (!finished) {
    mcs_node_t me;
    mcs_lock(&m_c_s_lock, &me);
    if (total_sum & 1)
      ++num_odds;
    ++total_sum;
    ++total_sum;
    mcs_unlock(&m_c_s_lock, &me);
    ++sum;
  }
  thread_sum[i] = sum;
  thread_odds[i] = num_odds;
  return NULL;
}

static void *
our_spinlock_test(void *ndx)
{
  int i = (int)(intptr_t)ndx;
  int sum = 0;
  int num_odds = 0;
  pthread_barrier_wait(&barrier);
  while (!finished) {
    spinlock_lock(&spin_lock);
    if (total_sum & 1)
      ++num_odds;
    ++total_sum;
    ++total_sum;
    spinlock_unlock(&spin_lock);
    ++sum;
  }
  thread_sum[i] = sum;
  thread_odds[i] = num_odds;
  return NULL;
}

static void
addtest(int num_secs, int num_threads, const char msg[], void *(*adder)(void *))
{
  int i;
  int maxOps = 0, minOps = INT_MAX;
  double meanOps = 0., dMeanOps = 0., varOps = 0., dVarOps = 0.;
  pthread_t *adder_thread = alloca(num_threads * sizeof(adder_thread[0]));
  int *tsums = alloca(num_threads * sizeof(tsums[0]));
  thread_sum = &tsums[0];
  int *todds = alloca(num_threads * sizeof(todds[0]));
  thread_odds = todds;

  finished = 0;
  total_sum = 0;
  for (i = 0; i < num_threads; ++i) {
    if (0 != pthread_create(&adder_thread[i], NULL, adder, (void *)(intptr_t)i)) {
      fprintf(stderr, "Error creating thread %d\n", i);
      exit(-1);
    }
  }

  pthread_barrier_wait(&barrier);
  sleep(num_secs);
  finished = 1;

  puts(msg);
  for (i = 0; i < num_threads; ++i) {
    if (0 != pthread_join(adder_thread[i], NULL)) {
      fprintf(stderr, "Error finishing thread %d\n", i);
      exit(-1);
    }
    printf("ops, odds counts for reader thread %d: %d %d\n", i, thread_sum[i], thread_odds[i]);
    if (thread_sum[i] > maxOps)
      maxOps = thread_sum[i];
    if (thread_sum[i] < minOps)
      minOps = thread_sum[i];

    /* update avg, variance */ {
      double oldMean = meanOps;
      double oldVar = varOps;
      double d = ((thread_sum[i] - meanOps) - dMeanOps) / (i + 1);
      double t = d + dMeanOps;
      meanOps = oldMean + t;
      dMeanOps = (oldMean - meanOps) + t;
      d = ((d*d*(i*(i+1)) - varOps) - dVarOps) / (i + 1);
      t = d + dVarOps;
      varOps = oldVar + t;
      dVarOps = (oldVar - varOps) + t;
    }
  }
  printf("min ops: %d\n", minOps);
  printf("max ops: %d\n", maxOps);
  printf("avg ops: %10.5f\n", meanOps);
  printf("stddev ops: %10.5f\n", sqrt(varOps/(num_threads - 1)));
  printf("total ops: %d\n", total_sum / 2);
}

static int
compute(int num_secs, int num_threads)
{
  pthread_barrier_init(&barrier, NULL, num_threads+1);
  addtest(num_secs, num_threads, "Results for sync_add test:", sync_add_test);

  pthread_mutex_init(&mutex, NULL);
  addtest(num_secs, num_threads, "Results for pthread_mutex test", pthread_mutex_test);
  pthread_mutex_destroy(&mutex);

  pthread_spin_init(&spinlock, PTHREAD_PROCESS_PRIVATE);
  addtest(num_secs, num_threads, "Results for pthread_spin_lock test", pthread_spin_test);
  pthread_spin_destroy(&spinlock);

  mcs_init(&m_c_s_lock);
  addtest(num_secs, num_threads, "Results for pthread_mcs test", pthread_mcs_test);

  spinlock_init(&spin_lock);
  addtest(num_secs, num_threads, "Results for our spinlock test", our_spinlock_test);

  pthread_barrier_destroy(&barrier);
  return 0;
}

int main(int argc, char *argv[])
{
  int num_secs = 10;
  int num_threads = 2;
  int ch;
  while ((ch = getopt(argc, argv, "s:t:")) != -1) {
    switch (ch) {
    case 's':
      num_secs = atoi(optarg);
      break;
    case 't':
      num_threads = atoi(optarg);
      break;
    default:
      fprintf(stderr, "Usage: <cmd> [-s num_seconds] [-t num_threads]\n");
      exit(1);
    }
  }
  return compute(num_secs, num_threads);
}
