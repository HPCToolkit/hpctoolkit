#include "control-knob.h"
#include <utilities/tokenize.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct control_knob {
  struct control_knob *next;
  char *name;
  char *value;
  control_knob_type type;
} control_knob_t;


static control_knob_t *control_knobs = NULL;


static control_knob_t *
control_knob_name_lookup(char *in)
{
  control_knob_t *iter = control_knobs;

  while (iter != NULL){
    if(strcmp(iter->name, in) == 0) return iter;

    iter = iter->next;
  }
  return NULL;
}


void
control_knob_register(char *name, char *value, control_knob_type type)
{
  control_knob_t *iter = control_knob_name_lookup(name);

  if (iter == NULL) {
    iter = (control_knob_t*) malloc(sizeof(control_knob_t));
    iter->name = strdup(name);
    iter->type = type;
    iter->next = control_knobs;
    control_knobs = iter;
  }
  iter->value = strdup(value);
  
}


static void
control_knob_default_register(){
  control_knob_register("STREAMS_PER_TRACING_THREAD", "4", ck_int);
  control_knob_register("MAX_COMPLETION_CALLBACK_THREADS", "1000", ck_int);
}


void
control_knob_init()
{
  control_knob_default_register();

  char *in = getenv("HPCRUN_CONTROL_KNOBS");
  if (in == NULL) return;

  char *save;
  for (char *f = start_tok(in); more_tok(); f = next_tok()){
    char *tmp = strdup(f);
    char *name = strtok_r(tmp, "=", &save);
    control_knob_t *iter = NULL;

    if (name != NULL && (iter = control_knob_name_lookup(name))) {
      char *value = strtok_r(NULL, "=", &save);
      iter->value = value;
    } else {
      fprintf(stderr, "\tcontrol token %s not recognized\n\n", f);
    }
  }

}


int
control_knob_value_get_int(char *in, int *value)
{
  control_knob_t *iter = control_knob_name_lookup(in);
  if (iter) {
    if (iter->type == ck_int) {
      *value = atoi(iter->value);
      return 0;
    }else{
      fprintf(stderr,"Control register type is not int.\n");
      return 1;
    }
  }
  fprintf(stderr,"No such name in Control register\n");
  return 2;
}


int
control_knob_value_get_float(char *in, float *value)
{
  control_knob_t *iter = control_knob_name_lookup(in);
  if (iter) {
    if (iter->type == ck_float) {
      *value = atof(iter->value);
      return 0;
    }else{
      fprintf(stderr,"Control register type is not float.\n");
      return 1;
    }
  }
  fprintf(stderr,"No such name in Control register\n");
  return 2;
}


int
control_knob_value_get_string(char *in, char **value)
{
  control_knob_t *iter = control_knob_name_lookup(in);
  if (iter) {
    if (iter->type == ck_string) {
      *value = iter->value;
      return 0;
    }else{
      fprintf(stderr,"Control register type is not string.\n");
      return 1;
    }
  }
  fprintf(stderr,"No such name in Control register\n");
  return 2;
}

