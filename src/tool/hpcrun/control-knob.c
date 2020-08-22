#include "control-knob.h"
#include <utilities/tokenize.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static char *control_knob_names[] = {
#define DEFINE_KNOB_NAMES(knob_name) \
  #knob_name,

  FORALL_KNOBS(DEFINE_KNOB_NAMES) 

#undef DEFINE_KNOB_NAMES
};


typedef struct control_knob {
  char *name;
  char *value;
} control_knob_t;

control_knob_t control_knobs[HPCRUN_NUM_CONTROL_KNOBS];


static void control_knob_process_string(char *in);


void
control_knob_init()
{
  char *s = getenv("HPCRUN_CONTROL_KNOBS");
  if (s) {
#define INIT_KNOBS(knob_name) \
  control_knobs[knob_name].name = control_knob_names[knob_name]; \
  control_knobs[knob_name].value = NULL;

  FORALL_KNOBS(INIT_KNOBS) 

#undef INIT_KNOBS

    control_knob_process_string(s);
  }
}


char *
control_knob_value_get(control_category c)
{
  if (c < HPCRUN_NUM_CONTROL_KNOBS) {
    return control_knobs[c].value;
  }
  return NULL;
}


int
control_knob_value_get_int(control_category c)
{
  char *ret = NULL;
  if ((ret = control_knob_value_get(c)) != NULL) {
    // FIXME(Keren): atoi does not differentiate between zero and failure
    return atoi(ret);
  }
  return 0;
}


static int
control_knob_name_lookup(char *n)
{
#define LOOKUP_KNOBS(knob_name)  \
  if (strcmp(control_knobs[knob_name].name, n) == 0) {  \
    return knob_name;                                   \
  }

  FORALL_KNOBS(LOOKUP_KNOBS) 

#undef LOOKUP_KNOBS

  return -1;
}


static void
control_knob_process_string(char *in)
{
  char *save;
  for (char *f = start_tok(in); more_tok(); f = next_tok()){
    char *tmp = strdup(f);
    char *name = strtok_r(tmp, "=", &save);
    int i = -1;

    if (name != NULL && (i = control_knob_name_lookup(name)) != -1) {
      char *value = strtok_r(NULL, "=", &save);
      control_knobs[i].value = value;
    } else {
      fprintf(stderr, "\tcontrol token %s not recognized\n\n", f);
    }
  }
}
