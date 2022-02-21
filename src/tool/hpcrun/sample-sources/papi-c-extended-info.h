#ifndef PAPI_C_EXTENDED_INFO_H
#define PAPI_C_EXTENDED_INFO_H

typedef void (*get_event_set_proc_t)(int* event_set);
typedef void (*add_event_proc_t)(int event_set, int evcode);
typedef void (*finalize_event_set_proc_t)(void);
typedef void (*setup_proc_t)(void);
typedef void (*teardown_proc_t)(void);
typedef void (*start_proc_t)(void);
typedef void (*read_proc_t)(long long *values);
typedef void (*stop_proc_t)(long long *values);
typedef bool (*pred_proc_t)(const char* name);

typedef struct sync_info_list_t {
  const pred_proc_t pred;
  const get_event_set_proc_t get_event_set;
  const add_event_proc_t add_event;
  const finalize_event_set_proc_t finalize_event_set;
  const bool is_gpu_sync;
  const setup_proc_t setup;
  const teardown_proc_t teardown;
  const start_proc_t start;
  const read_proc_t read;
  const stop_proc_t stop;
  const bool process_only;
  struct sync_info_list_t* next;
} sync_info_list_t;

extern const char* component_get_name(int cidx);
extern bool component_uses_sync_samples(int cidx);
extern get_event_set_proc_t component_get_event_set(int cidx);
extern add_event_proc_t component_add_event_proc(int cidx);
extern finalize_event_set_proc_t component_finalize_event_set(int cidx);
extern setup_proc_t sync_setup_for_component(int cidx);
extern teardown_proc_t sync_teardown_for_component(int cidx);
extern start_proc_t sync_start_for_component(int cidx);
extern read_proc_t sync_read_for_component(int cidx);
extern stop_proc_t sync_stop_for_component(int cidx);
extern void papi_c_sync_register(sync_info_list_t* info);

#endif // PAPI_C_EXTENDED_INFO_H
