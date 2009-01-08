#ifndef SPLAY_INTERVAL_H
#define SPLAY_INTERVAL_H

struct splay_interval_s {
  struct splay_interval_s *next;
  struct splay_interval_s *prev;

  void *start;
  void *end;
};
typedef struct splay_interval_s splay_interval_t;

typedef struct interval_status_t {
  char *first_undecoded_ins;
  splay_interval_t *first;
  int errcode;
} interval_status;

typedef interval_status build_intervals_fn_t(char *ins, unsigned int len);

#endif//SPLAY_INTERVAL_H
