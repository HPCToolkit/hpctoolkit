#ifndef PAPI_C_EXTENDED_INFO_H
#define PAPI_C_EXTENDED_INFO_H

typedef void (*get_event_set_proc_t)(int* ev_s);
typedef int (*add_event_proc_t)(int ev_s, int evcode);
typedef void (*finalize_event_set_proc_t)(void);
typedef void (*setup_proc_t)(void);
typedef void (*teardown_proc_t)(void);
typedef void (*start_proc_t)(void);
typedef void (*stop_proc_t)(void);
typedef bool (*pred_proc_t)(const char* name);

typedef struct sync_info_list_t {
  const pred_proc_t pred;
  const get_event_set_proc_t get_event_set;
  const add_event_proc_t add_event;
  const finalize_event_set_proc_t finalize_event_set;
  const setup_proc_t sync_setup;
  const teardown_proc_t sync_teardown;
  const start_proc_t sync_start;
  const stop_proc_t sync_stop;
  const bool process_only;
  struct sync_info_list_t* next;
} sync_info_list_t;

extern bool component_uses_sync_samples(int cidx);
extern get_event_set_proc_t component_get_event_set(int cidx);
extern add_event_proc_t component_add_event_proc(int cidx);
extern finalize_event_set_proc_t component_finalize_event_set(int cidx);
extern setup_proc_t sync_setup_for_component(int cidx);
extern teardown_proc_t sync_teardown_for_component(int cidx);
extern start_proc_t sync_start_for_component(int cidx);
extern stop_proc_t sync_stop_for_component(int cidx);
extern void papi_c_sync_register(sync_info_list_t* info);

#endif // PAPI_C_EXTENDED_INFO_H
