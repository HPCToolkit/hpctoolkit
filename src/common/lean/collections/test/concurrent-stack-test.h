// SPDX-FileCopyrightText: 2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

#include "test-util.h"
#include <stdlib.h>
#include <pthread.h>


const size_t N = 100;


int idx_to_value(size_t thread_id, size_t idx) {
  return (int) ((idx * 773 + thread_id * 769) % 131);
}


my_cstack_entry_t *new_entry(size_t thread_id, size_t idx) {
  my_cstack_entry_t *entry
    = (my_cstack_entry_t *) malloc(sizeof(my_cstack_entry_t));

  entry->thread_id = thread_id;
  entry->idx = idx;
  entry->value = idx_to_value(thread_id, idx);

  return entry;
}


void handle_entry_fail(my_cstack_entry_t *entry, void *arg) {
  FAIL();
}


void handle_entry_verify(my_cstack_entry_t *entry, void *arg) {
  if (arg != NULL) {
    size_t *cnt = (size_t *) arg;
    ++(*cnt);
  }

  ASSERT_EQ(idx_to_value(entry->thread_id, entry->idx), entry->value);
}


void test_empty_stack(my_cstack_t *stack) {
  ASSERT_EQ(my_cstack_pop(stack), NULL);
  ASSERT_EQ(my_cstack_pop_all(stack), NULL);
  my_cstack_for_each(stack, handle_entry_fail, NULL);
}


void test_push(my_cstack_t *stack, size_t thread_id, size_t nops, size_t size) {
  for (size_t i = 1; i <= nops; ++i) {
    my_cstack_entry_t *entry = new_entry(thread_id, i);
    my_cstack_push(stack, entry);

    size_t cnt = 0;
    my_cstack_for_each(stack, handle_entry_verify, &cnt);
    ASSERT_EQ(cnt, size + i);
  }
}


void test_pop(my_cstack_t *stack, size_t thread_id, size_t nops, size_t size) {
  for (size_t i = 1; i <= nops; ++i) {
    my_cstack_entry_t *entry = my_cstack_pop(stack);
    handle_entry_verify(entry, NULL);
    free(entry);

    size_t cnt = 0;
    my_cstack_for_each(stack, handle_entry_verify, &cnt);
    ASSERT_EQ(cnt, size - i);
  }
}


TEST(ConcurrentStackTest, SerialIsEmpty) {
  my_cstack_t stack;
  my_cstack_init(&stack);

  test_empty_stack(&stack);
}


TEST(ConcurrentStackTest, SerialSingleElement) {
  my_cstack_t stack;
  my_cstack_init(&stack);

  // Create entry
  my_cstack_entry_t *entry = new_entry(5, 7);

  // Push entry and verify the stack
  my_cstack_push(&stack, entry);
  size_t cnt = 0;
  my_cstack_for_each(&stack, handle_entry_verify, &cnt);
  ASSERT_EQ(cnt, 1);

  // Pop entry
  {
    my_cstack_entry_t *entry_pop = my_cstack_pop(&stack);
    ASSERT_EQ(entry_pop, entry);

    cnt = 0;
    my_cstack_for_each(&stack, handle_entry_verify, &cnt);
    ASSERT_EQ(cnt, 0);
  }

  // Push the entry again
  my_cstack_push(&stack, entry);
  my_cstack_for_each(&stack, handle_entry_verify, NULL);

  cnt = 0;
  my_cstack_for_each(&stack, handle_entry_verify, &cnt);
  ASSERT_EQ(cnt, 1);

  // Pop(all) entry
  {
    my_cstack_entry_t *entry_pop = my_cstack_pop(&stack);
    ASSERT_EQ(entry_pop, entry);

    cnt = 0;
    my_cstack_for_each(&stack, handle_entry_verify, &cnt);
    ASSERT_EQ(cnt, 0);
  }

  free(entry);
}


TEST(ConcurrentStackTest, SerialMutlipleElementsEmpty) {
  my_cstack_t stack;
  my_cstack_init(&stack);

  for (size_t i = 1; i < N; ++i) {
    test_push(&stack, 5, i, 0);
    test_pop(&stack, 5, i, i);

    test_empty_stack(&stack);
  }

  test_empty_stack(&stack);
}


TEST(ConcurrentStackTest, SerialMutlipleElements) {
  my_cstack_t stack;
  my_cstack_init(&stack);

  test_push(&stack, 5, N, 0);
  for (size_t i = 1; i < N; ++i) {
    test_push(&stack, 5, i, N);
    test_pop(&stack, 5, i, N + i);
  }
  test_pop(&stack, 5, N, N);

  test_empty_stack(&stack);
}



typedef struct thread_data_t {
  size_t thread_id;
  size_t num_threads;

  my_cstack_t *consume_stack;
  my_cstack_t *produce_stack;

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
      my_cstack_entry_t *entry = my_cstack_pop(data->consume_stack);

      if (entry) {
        --to_consume;
        // handle_entry_verify(entry, NULL);

        if (data->forward) {
          my_cstack_push(data->produce_stack, entry);
        } else {
          free(entry);
        }
      }
    }

    if (to_produce > 0) {
      my_cstack_entry_t *entry = new_entry(data->thread_id, to_produce);
      my_cstack_push(data->produce_stack, entry);
      --to_produce;
    }
  }

  return NULL;
}


TEST(ConcurrentStackTest, ConcurrentSPSC) {
  const size_t NUM_MESSAGES = N;

  my_cstack_t stack;
  my_cstack_init(&stack);


  thread_data_t consumer_data = {
    .thread_id = 1,
    .num_threads = 2,

    .consume_stack = &stack,
    .produce_stack = NULL,

    .to_produce = 0,
    .to_consume = NUM_MESSAGES,
    .forward = false
  };

  pthread_t consumer_thread;
  pthread_create(&consumer_thread, NULL, thread_fn, &consumer_data);


  thread_data_t producer_data = {
    .thread_id = 0,
    .num_threads = 2,

    .consume_stack = NULL,
    .produce_stack = &stack,

    .to_produce = NUM_MESSAGES,
    .to_consume = 0,
    .forward = false
  };

  pthread_t producer_thread;
  pthread_create(&producer_thread, NULL, thread_fn, &producer_data);

  pthread_join(consumer_thread, NULL);
  pthread_join(producer_thread, NULL);
}


TEST(ConcurrentStackTest, ConcurrentMPSC) {
  const size_t NUM_MESSAGES = N;
  const size_t NUM_PRODUCERS = 8;


  my_cstack_t stack;
  my_cstack_init(&stack);

  pthread_t producer_threads[NUM_PRODUCERS];
  thread_data_t producer_data[NUM_PRODUCERS];

  for (size_t i = 0; i < NUM_PRODUCERS; ++i) {
    producer_data[i] = (thread_data_t) {
      .thread_id = i + 1,
      .num_threads = NUM_PRODUCERS + 1,

      .consume_stack = NULL,
      .produce_stack = &stack,

      .to_produce = NUM_MESSAGES,
      .to_consume = 0,
      .forward = false
    };

    pthread_create(&producer_threads[i], NULL, thread_fn, &producer_data[i]);
  }


  thread_data_t consumer_data = {
    .thread_id = 0,
    .num_threads = NUM_PRODUCERS + 1,

    .consume_stack = &stack,
    .produce_stack = NULL,

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


TEST(ConcurrentStackTest, ConcurrentRing) {
  const size_t NUM_MESSAGES = N;
  const size_t NUM_THREADS = 8;

  my_cstack_t stacks[NUM_THREADS];
  for (size_t i = 0; i < NUM_THREADS; ++i) {
    my_cstack_init(&stacks[i]);
  }

  pthread_t threads[NUM_THREADS];
  thread_data_t thread_data[NUM_THREADS];

  for (size_t i = 0; i < NUM_THREADS; ++i) {
    thread_data[i] = (thread_data_t) {
      .thread_id = i,
      .num_threads = NUM_THREADS,

      .consume_stack = &stacks[i],
      .produce_stack = &stacks[(i + 1) % NUM_THREADS],

      .to_produce = NUM_MESSAGES,
      .to_consume = NUM_THREADS * NUM_MESSAGES,
      .forward = true
    };

    pthread_create(&threads[i], NULL, thread_fn, &thread_data[i]);
  }

  for (size_t i = 0; i < NUM_THREADS; ++i) {
    pthread_join(threads[i], NULL);
  }

  for (size_t i = 0; i < NUM_THREADS; ++i) {
    size_t cnt = 0;
    my_cstack_for_each(&stacks[0], handle_entry_verify, &cnt);
    ASSERT_EQ(cnt, NUM_MESSAGES);
  }
}


TEST_MAIN()
