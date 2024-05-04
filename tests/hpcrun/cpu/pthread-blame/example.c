// SPDX-FileCopyrightText: 2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

#include <pthread.h>
#include <error.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>

void* body(void* v_lock) {
  pthread_mutex_t* lock = v_lock;

  int err;
  if ((err = pthread_mutex_lock(lock)) != 0)
    error(1, err, "Failed to lock mutex from thread");
  if ((err = pthread_mutex_unlock(lock)) != 0)
    error(1, err, "Failed to unlock mutex from thread");
  return NULL;
}

int main() {
  pthread_mutex_t lock;

  int err;
  if ((err = pthread_mutex_init(&lock, NULL)) != 0)
    error(1, err, "Failed to create mutex");

  if ((err = pthread_mutex_lock(&lock)) != 0)
    error(1, err, "Failed to acquire mutex from main");

  pthread_t t;
  if ((err = pthread_create(&t, NULL, body, &lock)) != 0)
    error(1, err, "Failed to spawn thread");

  // Wait a while to accrue blame from the other thread
  struct timespec ts = {0, 500000000};
  while ((err = clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, &ts)) == EINTR);
  if (err != 0)
    error(1, err, "Failed during sleep");

  if ((err = pthread_mutex_unlock(&lock)) != 0)
    error(1, err, "Failed to release mutex from main");

  if ((err = pthread_join(t, NULL)) != 0)
    error(1, err, "Failed to join thread");

  if ((err =pthread_mutex_destroy(&lock)) != 0)
    error(1, err, "Failed to destroy mutex");
}
