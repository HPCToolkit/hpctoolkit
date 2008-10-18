#ifndef THREAD_DATA_H
#define THREAD_DATA_H

typedef struct _td {
  int id;
  int count;
} thread_data_t;

extern thread_data_t *get_thread_data(void);
extern void *alloc_thread_data(unsigned int id);
extern void free_thread_data(void);

#endif // THREAD_DATA_H
