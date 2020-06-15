#ifndef HPCRUN_CONTROL_KNOB_H
#define HPCRUN_CONTROL_KNOB_H

#define FORALL_KNOBS(macro)  \
  macro(HPCRUN_CUDA_DEVICE_BUFFER_SIZE, 8388608)     \
  macro(HPCRUN_CUDA_DEVICE_SEMAPHORE_SIZE, 65536)  \
	macro(STREAMS_PER_THREAD, 65536)	\
	macro(MAX_THREADS_CONSUMERS, 256) \

typedef enum {
#define DEFINE_ENUM_KNOBS(knob_name, knob_value)  \
  knob_name ,

  FORALL_KNOBS(DEFINE_ENUM_KNOBS) 

#undef DEFINE_ENUM_KNOBS

#define COUNT_FORALL_CLAUSE(a,b) + 1
#define NUM_CLAUSES(forall_macro) 0 forall_macro(COUNT_FORALL_CLAUSE)

  HPCRUN_NUM_CONTROL_KNOBS = NUM_CLAUSES(FORALL_KNOBS)

#undef NUM_CLAUSES
#undef COUNT_FORALL_CLAUSE
} control_category;

void control_knob_init();

char *control_knob_value_get(control_category c);

int control_knob_value_get_int(control_category c);

#endif
