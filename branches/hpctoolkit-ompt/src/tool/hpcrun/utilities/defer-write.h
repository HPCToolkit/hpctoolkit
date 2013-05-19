#ifndef defer_write_h
#define defer_write_h

#include <thread_data.h>

// type definition
struct entry_t {
  thread_data_t *td;
  struct entry_t *prev;
  struct entry_t *next;
  bool flag;
};

// operations
struct entry_t *new_dw_entry();
void insert_dw_entry(struct entry_t* entry);
void add_defer_td(thread_data_t *td);
void write_other_td();
#endif
