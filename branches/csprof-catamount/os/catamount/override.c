#include <assert.h>
#include <signal.h>
#include <stdlib.h>

#include "pmsg.h"

extern int __real_sigprocmask(int, const sigset_t *, sigset_t *);
extern int __real_sigaction(int sig, const struct sigaction *act,
			  struct sigaction *oact);
extern sig_t __real_signal(int sig, sig_t func);

extern void *__real_calloc(size_t nmemb, size_t size);
extern void *__real_malloc(size_t size);
extern void *__real_realloc(void *ptr, size_t size);

int __wrap_sigprocmask(int mask, const sigset_t *set1, sigset_t *set2){
  return __real_sigprocmask(mask,set1,set2);
}
int __wrap_sigaction(int sig, const struct sigaction *act,struct sigaction *oact){
  return __real_sigaction(sig,act,oact);
}
sig_t __wrap_signal(int sig, sig_t func){
  return __real_signal(sig,func);
}

void *__wrap_calloc(size_t nmemb, size_t size){
  return __real_calloc(nmemb,size);
}
void *__wrap_realloc(void *ptr, size_t size){
  return __real_realloc(ptr,size);
}

/* override various functions here */

int _csprof_in_malloc = 0;
int csprof_need_more = 0;

void *__wrap_malloc(size_t s){
  void *alloc;

  _csprof_in_malloc = 1;
  alloc = __real_malloc(s);
  if (csprof_need_more){
    assert(0);
    /* alloc more space here */
    csprof_need_more = 0;
  }
  _csprof_in_malloc = 0;
  return alloc;
}

void *__wrap_pthread_create(void *dc){
  EMSG("Called pthread_create!!");
  assert(0);
}
