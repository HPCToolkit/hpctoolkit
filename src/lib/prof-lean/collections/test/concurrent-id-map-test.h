// TODO: improve tests

#include "test-util.h"

size_t alloc_counter = 0;

void* alloc(size_t size) {
  printf("alloc\n");
  ++alloc_counter;
  return malloc(size);
}


TEST(IDMapTest, Simple) {
  id_map_t map;
  id_map_init(&map, alloc);

  for (uint32_t i = 0; i < 2048; i += 128) {
    printf("%u: %p\n", i , id_map_get(&map, i));
  }

  printf("\n");

  for (uint32_t i = 2048; i < 2048*4; i += 128) {
    printf("%u: %p\n", i, id_map_get(&map, i));
  }

  printf("\n");

  for (uint32_t i = 2048; i < 2048*4; i += 128) {
    printf("%u: %p\n", i, id_map_get(&map, i));
  }

}


TEST_MAIN();
