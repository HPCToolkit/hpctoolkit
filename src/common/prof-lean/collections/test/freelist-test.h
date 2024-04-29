#include "test-util.h"

#include <stdlib.h>


size_t expected_size = 0;
size_t alloc_counter = 0;

void* alloc(size_t size) {
  if (expected_size) {
    ASSERT_EQ(expected_size, size);
  }
  ++alloc_counter;
  printf("alloc\n");
  return malloc(size);
}


TEST(FreelistTest, Simple) {
  my_freelist_t freelist;
  my_freelist_init(&freelist, alloc);

  const size_t NUM_ENTRIES = 5;
  my_freelist_entry_t *entries[NUM_ENTRIES];

  for (size_t i = 0; i < NUM_ENTRIES; ++i) {
    entries[i] = my_freelist_allocate(&freelist);
    printf("%p\n", entries[i]);
  }

  for (size_t i = 0; i < NUM_ENTRIES; ++i) {
    my_freelist_free(&freelist, entries[i]);
  }

  for (size_t i = 0; i < NUM_ENTRIES; ++i) {
    entries[i] = my_freelist_allocate(&freelist);
    printf("%p\n", entries[i]);
  }
}


TEST_MAIN();
