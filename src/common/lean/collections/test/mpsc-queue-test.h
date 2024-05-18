// SPDX-FileCopyrightText: 2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include "test-util.h"
#include <stdlib.h>
#include <pthread.h>


const size_t N = 100;


int idx_to_value(size_t thread_id, size_t idx) {
  return (int) ((idx * 773 + thread_id * 769) % 131);
}


my_mpscq_entry_t *new_entry(size_t thread_id, size_t idx) {
  my_mpscq_entry_t *entry
    = (my_mpscq_entry_t *) malloc(sizeof(my_mpscq_entry_t));
  entry->idx = idx;
  entry->thread_id = thread_id;
  entry->value = idx_to_value(thread_id, idx);
  return entry;
}


void test_empty_queue(my_mpscq_t *queue) {
  ASSERT_EQ(my_mpscq_dequeue(queue), NULL);
}


TEST(MpscQueueTest, SerialIsEmpty) {
  my_mpscq_t queue;
  my_mpscq_init(&queue);

  test_empty_queue(&queue);
}


TEST(MpscQueueTest, SerialSingleElement) {
  my_mpscq_t queue;
  my_mpscq_init(&queue);

  /* Create and enqueu entry */
  my_mpscq_entry_t *entry = new_entry(5, 7);
  my_mpscq_enqueue(&queue, entry);
// test_empty_queue(&queue);
  /* Dequeue entry */
  my_mpscq_entry_t *entry_deq = my_mpscq_dequeue(&queue);
  ASSERT_EQ(entry, entry_deq);

  /* Enqueue and dequeue the entry again */
  my_mpscq_enqueue(&queue, entry);
  entry_deq = my_mpscq_dequeue(&queue);
  ASSERT_EQ(entry, entry_deq);

  free(entry);
}



typedef struct thread_data_t {
  size_t thread_id;
  size_t num_threads;

  my_mpscq_t *consume_queue;
  my_mpscq_t *produce_queue;

  size_t to_produce;
  size_t to_consume;
  bool forward;
} thread_data_t;

void* thread_fn(void *arg) {
  thread_data_t *data = (thread_data_t *) arg;

  size_t to_produce = data->to_produce;
  size_t to_consume = data->to_consume;

  while (to_produce > 0 || to_consume > 0) {
    if (to_consume > 0) {
      my_mpscq_entry_t *entry = my_mpscq_dequeue(data->consume_queue);

      if (entry) {
        --to_consume;
        // handle_entry_verify(entry, NULL);

        if (data->forward) {
          my_mpscq_enqueue(data->produce_queue, entry);
        } else {
          free(entry);
        }
      }
    }

    if (to_produce > 0) {
      my_mpscq_entry_t *entry = new_entry(data->thread_id, to_produce);
      my_mpscq_enqueue(data->produce_queue, entry);
      --to_produce;
    }
  }

  return NULL;
}


TEST(MpscQueueTest, ConcurrentMPSC) {
  const size_t NUM_MESSAGES = N;
  const size_t NUM_PRODUCERS = 8;


  my_mpscq_t stack;
  my_mpscq_init(&stack);

  pthread_t producer_threads[NUM_PRODUCERS];
  thread_data_t producer_data[NUM_PRODUCERS];

  for (size_t i = 0; i < NUM_PRODUCERS; ++i) {
    producer_data[i] = (thread_data_t) {
      .thread_id = i + 1,
      .num_threads = NUM_PRODUCERS + 1,

      .consume_queue = NULL,
      .produce_queue = &stack,

      .to_produce = NUM_MESSAGES,
      .to_consume = 0,
      .forward = false
    };

    pthread_create(&producer_threads[i], NULL, thread_fn, &producer_data[i]);
  }


  thread_data_t consumer_data = {
    .thread_id = 0,
    .num_threads = NUM_PRODUCERS + 1,

    .consume_queue = &stack,
    .produce_queue = NULL,

    .to_produce = 0,
    .to_consume = NUM_PRODUCERS * NUM_MESSAGES,
    .forward = false
  };

  pthread_t consumer_thread;
  pthread_create(&consumer_thread, NULL, thread_fn, &consumer_data);


  for (size_t i = 0; i < NUM_PRODUCERS; ++i) {
    pthread_join(producer_threads[i], NULL);
  }
  pthread_join(consumer_thread, NULL);
}

TEST_MAIN()
