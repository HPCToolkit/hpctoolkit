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
thread_data_t *get_reuse_td();
void delete_dw_entry(struct entry_t *entry);
void insert_dw_entry(struct entry_t* entry);
struct entry_t *fetch_dw_entry(struct entry_t **pointer);
void set_dw_pointer(struct entry_t **pointer);
void add_defer_td(thread_data_t *td);
#endif
