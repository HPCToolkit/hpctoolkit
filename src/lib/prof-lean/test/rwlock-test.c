#include <alloca.h>
#include <math.h>
#include <getopt.h>
#define __USE_XOPEN2K
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include "pfq-rwlock.h"

static volatile int finished;
static volatile unsigned long int total_sum;
static int *thread_sum;
static int *thread_odds;
static pthread_barrier_t barrier;
static pthread_rwlock_t rw_lock;
static pfq_rwlock_t pfq_lock;

static void *
pthread_rwlock_reader(void *ndx)
{
  int i = (int)(intptr_t)ndx;
  int sum = 0;
  int num_odds = 0;
  pthread_barrier_wait(&barrier);
  while (!finished) {
    pthread_rwlock_rdlock(&rw_lock);
    if (total_sum & 1)
      ++num_odds;
    pthread_rwlock_unlock(&rw_lock);
    ++sum;
  }
  thread_sum[i] = sum;
  thread_odds[i] = num_odds;
  return NULL;
}


static void *
pthread_rwlock_writer(void *ndx)
{
  int i = (int)(intptr_t)ndx;
  int sum = 0;
  int num_odds = 0;
  pthread_barrier_wait(&barrier);
  while (!finished) {
    pthread_rwlock_wrlock(&rw_lock);
    if (total_sum & 1)
      ++num_odds;
    ++total_sum;
    ++total_sum;
    pthread_rwlock_unlock(&rw_lock);
    ++sum;
  }
  thread_sum[i] = sum;
  thread_odds[i] = num_odds;
  return NULL;
}

static void *
pthread_pfq_reader(void *ndx)
{
  int i = (int)(intptr_t)ndx;
  int sum = 0;
  int num_odds = 0;
  pthread_barrier_wait(&barrier);
  while (!finished) {
    pfq_rwlock_read_lock(&pfq_lock);
    if (total_sum & 1)
      ++num_odds;
    pfq_rwlock_read_unlock(&pfq_lock);
    ++sum;
  }
  thread_sum[i] = sum;
  thread_odds[i] = num_odds;
  return NULL;
}


static void *
pthread_pfq_writer(void *ndx)
{
  int i = (int)(intptr_t)ndx;
  int sum = 0;
  int num_odds = 0;
  pthread_barrier_wait(&barrier);
  while (!finished) {
    pfq_rwlock_node_t me;
    pfq_rwlock_write_lock(&pfq_lock, &me);
    if (total_sum & 1)
      ++num_odds;
    ++total_sum;
    ++total_sum;
    pfq_rwlock_write_unlock(&pfq_lock, &me);
    ++sum;
  }
  thread_sum[i] = sum;
  thread_odds[i] = num_odds;
  return NULL;
}

static void
addtest(int num_secs, int num_readers, int num_writers, const char msg[], void *(*reader)(void *), void *(*writer)(void *))
{
  int i;
  int maxOps = 0, minOps = INT_MAX;
  double meanOps = 0., dMeanOps = 0., varOps = 0., dVarOps = 0.;
  pthread_t *thread = alloca((num_readers+num_writers) * sizeof(thread[0]));
  int *tsums = alloca((num_readers+num_writers) * sizeof(tsums[0]));
  thread_sum = &tsums[0];
  int *todds = alloca((num_readers+num_writers) * sizeof(todds[0]));
  thread_odds = todds;

  finished = 0;
  total_sum = 0;
  for (i = 0; i < num_readers; ++i) {
    if (0 != pthread_create(&thread[i], NULL, reader, (void *)(intptr_t)i)) {
      fprintf(stderr, "Error creating reader thread %d\n", i);
      exit(-1);
    }
  }
  for (i = 0; i < num_writers; ++i) {
    if (0 != pthread_create(&thread[num_readers+i], NULL, writer, (void *)(intptr_t)(num_readers+i))) {
      fprintf(stderr, "Error creating writer thread %d\n", i);
      exit(-1);
    }
  }

  pthread_barrier_wait(&barrier);
  sleep(num_secs);
  finished = 1;

  puts(msg);
  printf("Readers:\n");
  for (i = 0; i < num_readers + num_writers; ++i) {
    if (0 != pthread_join(thread[i], NULL)) {
      fprintf(stderr, "Error finishing thread %d\n", i);
      exit(-1);
    }
  }

  maxOps = 0, minOps = INT_MAX;
  meanOps = 0., dMeanOps = 0., varOps = 0., dVarOps = 0.;
  for (i = 0; i < num_readers; ++i) {
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
  printf("stddev ops: %10.5f\n", sqrt(varOps/(num_readers - 1)));

  printf("Writers:\n");
  maxOps = 0, minOps = INT_MAX;
  meanOps = 0., dMeanOps = 0., varOps = 0., dVarOps = 0.;
  for (i = 0; i < num_writers; ++i) {
    printf("ops, odds counts for writer thread %d: %d %d\n", i,
	   thread_sum[num_readers+i], thread_odds[num_readers+i]);
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
  printf("stddev ops: %10.5f\n", sqrt(varOps/(num_readers - 1)));
  printf("total ops: %lu\n", total_sum / 2);
}

static int
compute(int num_secs, int num_readers, int num_writers)
{
  pthread_barrier_init(&barrier, NULL, num_readers+num_writers+1);

  pthread_rwlock_init(&rw_lock, NULL);
  addtest(num_secs, num_readers, num_writers, "Results for pthread_rwlock test", pthread_rwlock_reader, pthread_rwlock_writer);
  pthread_rwlock_destroy(&rw_lock);

  pfq_rwlock_init(&pfq_lock);
  addtest(num_secs, num_readers, num_writers, "Results for pthread_pfq test", pthread_pfq_reader, pthread_pfq_writer);

  pthread_barrier_destroy(&barrier);
  return 0;
}

int main(int argc, char *argv[])
{
  int num_secs = 10;
  int num_readers = 2;
  int num_writers = 1;
  int ch;
  while ((ch = getopt(argc, argv, "s:r:w:")) != -1) {
    switch (ch) {
    case 's':
      num_secs = atoi(optarg);
      break;
    case 'r':
      num_readers = atoi(optarg);
      break;
    case 'w':
      num_writers = atoi(optarg);
      break;
    default:
      fprintf(stderr, "Usage: <cmd> [-s num_seconds] [-r num_readers]] [-w num_writers]\n");
      exit(1);
    }
  }
  return compute(num_secs, num_readers, num_writers);
}
