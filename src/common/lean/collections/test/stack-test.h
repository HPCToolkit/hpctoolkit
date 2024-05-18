// SPDX-FileCopyrightText: 2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

#include "test-util.h"

#include <stdlib.h>

const size_t N = 100;


int idx_to_value(size_t idx) {
  return (int) (idx * 773 % 131);
}


static void handle_entry_fail(my_stack_entry_t *entry, void *arg) {
  FAIL();
}


static void handle_entry_count(my_stack_entry_t *entry, void *arg) {
  size_t *cnt = (size_t *) arg;
  ++(*cnt);
}


void test_empty_stack(my_stack_t *stack) {
  ASSERT_TRUE(my_stack_empty(stack));
  ASSERT_EQ(my_stack_top(stack), NULL);
  ASSERT_EQ(my_stack_pop(stack), NULL);

  my_stack_for_each_entry(stack, handle_entry_fail, NULL);
}


static void test_push(my_stack_t *stack, size_t nops, const size_t stack_size) {
  for (size_t i = 1; i <= nops; ++i) {
    my_stack_entry_t *entry = (my_stack_entry_t*) malloc(sizeof(my_stack_entry_t));
    my_stack_init_entry(entry);

    entry->value = idx_to_value(stack_size + i);
    my_stack_push(stack, entry);

    size_t cnt = 0;
    my_stack_for_each_entry(stack, handle_entry_count, &cnt);
    ASSERT_EQ(cnt, i + stack_size);

    ASSERT_FALSE(my_stack_empty(stack));
    ASSERT_EQ(my_stack_top(stack), entry);
  }
}


static void test_pop(my_stack_t *stack, size_t nops, const size_t stack_size) {
  for (size_t i = 1; i <= nops; ++i) {
    my_stack_entry_t *top_entry = my_stack_top(stack);
    ASSERT_EQ(top_entry->value, idx_to_value(stack_size - i + 1));

    my_stack_entry_t *pop_entry = my_stack_pop(stack);
    ASSERT_EQ(top_entry->value, idx_to_value(stack_size - i + 1));
    ASSERT_EQ(top_entry, pop_entry);

    size_t cnt = 0;
    my_stack_for_each_entry(stack, handle_entry_count, &cnt);
    ASSERT_EQ(cnt, stack_size - i);

    if (stack_size > i) {
      ASSERT_FALSE(my_stack_empty(stack));
    } else {
      ASSERT_TRUE(my_stack_empty(stack));
    }

    free(pop_entry);
  }
}



TEST(StackTest, IsEmpty) {
  my_stack_t stack;
  my_stack_init_stack(&stack);

  test_empty_stack(&stack);
}


TEST(StackTest, PushPopSingle) {
  my_stack_t stack;
  my_stack_init_stack(&stack);

  my_stack_entry_t *entry
    = (my_stack_entry_t*) malloc(sizeof(my_stack_entry_t));
  my_stack_init_entry(entry);

  my_stack_push(&stack, entry);

  size_t cnt = 0;
  my_stack_for_each_entry(&stack, handle_entry_count, &cnt);
  ASSERT_EQ(cnt, 1);

  ASSERT_FALSE(my_stack_empty(&stack));
  ASSERT_EQ(my_stack_top(&stack), entry);
  ASSERT_EQ(my_stack_pop(&stack), entry);

  free(entry);
  test_empty_stack(&stack);
}


TEST(StackTest, PushPopEmptyStack) {
  my_stack_t stack;
  my_stack_init_stack(&stack);

  for (size_t i = 1; i <= N; ++i) {
    test_push(&stack, i, 0);
    test_pop(&stack, i, i);

    test_empty_stack(&stack);
  }
}


TEST(StackTest, PushPop) {
  my_stack_t stack;
  my_stack_init_stack(&stack);

  test_push(&stack, N, 0);
  for (size_t i = 1; i <= N; ++i) {
    test_push(&stack, i, N);
    test_pop(&stack, i, N + i);
  }
  test_pop(&stack, N, N);

  test_empty_stack(&stack);
}


TEST_MAIN()
