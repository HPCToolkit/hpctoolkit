#ifndef EVLIST_H
#define EVLIST_H

#define MAX_EVL_SIZE 1024
#define MAX_EVENTS 16

typedef struct _ev_t {
  int event;
  long thresh;
} _ev_t;



typedef struct evlist_t {
  char evl_spec[MAX_EVL_SIZE];
  int nevents;
  _ev_t events[MAX_EVENTS];
} evlist_t;

#endif // EVLIST_H
