#ifndef PAPI_C_EXTENDED_INFO_H
#define PAPI_C_EXTENDED_INFO_H

typedef void (*start_proc_t)(void);
typedef bool (*pred_proc_t)(const char* name);

typedef struct sync_info_list_t {
  const pred_proc_t pred;
  const start_proc_t sync_start;
  struct sync_info_list_t* next;
} sync_info_list_t;

extern bool component_uses_sync_samples(int cidx);
extern start_proc_t sync_start_for_component(int cidx);
extern void papi_c_sync_register(sync_info_list_t* info);

#endif // PAPI_C_EXTENDED_INFO_H
