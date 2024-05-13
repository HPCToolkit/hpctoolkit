// SPDX-FileCopyrightText: 2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include "test-util.h"
#include <stdlib.h>

const size_t N = 100;


//  Helper functions for generating entry values
KEY_TYPE idx_to_key(size_t idx) {
  return (KEY_TYPE) (idx * 773 % 131);
}


VALUE_TYPE key_to_value(KEY_TYPE key) {
  return (VALUE_TYPE) (key ^ 0x12345);
}


VALUE_TYPE idx_to_value(size_t idx) {
  return key_to_value(idx_to_key(idx));
}


KEY_TYPE value_to_key(VALUE_TYPE value) {
  return (KEY_TYPE) (value ^ 0x12345);
}


my_splay_tree_entry_t *new_entry(size_t idx) {
  my_splay_tree_entry_t *entry
    = (my_splay_tree_entry_t *) malloc(sizeof(my_splay_tree_entry_t));

  entry->key = idx_to_key(idx);
  entry->value = idx_to_value(idx);

  return entry;
}


// For each callbacks
void handle_entry_fail(my_splay_tree_entry_t *entry, void *arg) {
  FAIL();
}


void handle_entry_verify(my_splay_tree_entry_t *entry, void *arg) {
  ASSERT_EQ(entry->key, key_to_value(entry->value));
}

typedef struct entry_order_arg_t {
  bool initialized;
  KEY_TYPE previous;
} entry_order_arg_t;

void handle_entry_order(my_splay_tree_entry_t *entry, void *arg) {
  entry_order_arg_t *data = (entry_order_arg_t *) arg;
  if (data->initialized) {
    ASSERT_GT(entry->key, data->previous);
    data->previous = entry->key;
  } else {
    data->initialized = true;
    data->previous = entry->key;
  }
}

// Test helpers
void test_empty_tree(my_splay_tree_t *tree) {
  ASSERT_TRUE(my_splay_tree_empty(tree));
  ASSERT_EQ(my_splay_tree_size(tree), 0);

  for (size_t i = 0; i <= N; ++i) {
    ASSERT_EQ(my_splay_tree_lookup(tree, idx_to_key(i)), NULL);
    ASSERT_EQ(my_splay_tree_delete(tree, idx_to_key(i)), NULL);
  }

  my_splay_tree_for_each(tree, handle_entry_fail, NULL);
}


void test_tree(my_splay_tree_t *tree, size_t expected_size) {
  if (expected_size == 0) {
    test_empty_tree(tree);
    return;
  }

  ASSERT_EQ(my_splay_tree_size(tree), expected_size);
  my_splay_tree_for_each(tree, handle_entry_verify, NULL);

  entry_order_arg_t arg = {.initialized = false};
  my_splay_tree_for_each(tree, handle_entry_order, &arg);
}


TEST(SplayTreeTest, IsEmpty) {
  my_splay_tree_t tree;
  my_splay_tree_init_splay_tree(&tree);

  test_empty_tree(&tree);
}


TEST(SplayTree, InsertLookupSingle) {
  my_splay_tree_t tree;
  my_splay_tree_init_splay_tree(&tree);

  my_splay_tree_entry_t entry;
  entry.key = idx_to_key(7);
  entry.value = idx_to_value(7);

  // Lookup and delete non-existing entry
  ASSERT_EQ(my_splay_tree_lookup(&tree, entry.key), NULL);
  ASSERT_EQ(my_splay_tree_delete(&tree, entry.key), NULL);

  // Insert the same entry twice
  {
    ASSERT_TRUE(my_splay_tree_insert(&tree, &entry));
    my_splay_tree_entry_t entry_dup;

    entry_dup.key = entry.key;
    entry_dup.value = entry.value;
    ASSERT_FALSE(my_splay_tree_insert(&tree, &entry_dup));
  }

  // Verify size and values
  ASSERT_EQ(my_splay_tree_size(&tree), 1);
  my_splay_tree_for_each(&tree, handle_entry_verify, NULL);

  // Lookup the same entry twice
  ASSERT_EQ(my_splay_tree_lookup(&tree, entry.key), &entry);
  ASSERT_EQ(my_splay_tree_lookup(&tree, entry.key), &entry);

  // Delete the same entry twice
  ASSERT_EQ(my_splay_tree_delete(&tree, entry.key), &entry);
  ASSERT_EQ(my_splay_tree_delete(&tree, entry.key), NULL);

  test_empty_tree(&tree);
}


TEST(SplayTreeTest, InsertLookup) {
  my_splay_tree_t tree;
  my_splay_tree_init_splay_tree(&tree);

  // Insert [0, N) keys
  for (size_t i = 0; i < N; ++i) {
    my_splay_tree_entry_t *entry = new_entry(i);

    ASSERT_TRUE(my_splay_tree_insert(&tree, entry));

    // Create an entry with the same key and try insert it
    my_splay_tree_entry_t *entry_dup = new_entry(i);
    ASSERT_FALSE(my_splay_tree_insert(&tree, entry_dup));
    free(entry_dup);

    // Verify size, values, and order
    test_tree(&tree, i + 1);
  }

  // Lookup [0, N] keys
  for (size_t i = 0; i < N; ++i) {
    my_splay_tree_entry_t *entry = my_splay_tree_lookup(&tree, idx_to_key(i));
    ASSERT_NE(entry, NULL);

    test_tree(&tree, N);
  }

  // Delete [0, N) keys
  for (size_t i = 0; i < N; ++i) {
    my_splay_tree_entry_t *entry = my_splay_tree_delete(&tree, idx_to_key(i));
    ASSERT_NE(entry, NULL);
    free(entry);

    ASSERT_EQ(my_splay_tree_delete(&tree, idx_to_key(i)), NULL);

    // Verify size, values, and order
    test_tree(&tree, N - i - 1);
  }

  test_empty_tree(&tree);
}


TEST_MAIN();
