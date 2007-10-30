#include <signal.h>
#include <stdlib.h>

extern int real_sigprocmask(int, const sigset_t *, sigset_t *);
extern int real_sigaction(int sig, struct sigaction *act,
			  struct sigaction *oact);
extern sig_t real_signal(int sig, sig_t func);

extern void *real_calloc(size_t nmemb, size_t size);
extern void *real_malloc(size_t size);
extern void *real_realloc(void *ptr, size_t size);

int sigprocmask(int mask, const sigset_t *set1, sigset_t *set2){
  return real_sigprocmask(mask,set1,set2);
}
int sigaction(int sig, const struct sigaction *act,struct sigaction *oact){
  return real_sigaction(sig,act,oact);
}
sig_t signal(int sig, sig_t func){
  return real_signal(sig,func);
}

void *calloc(size_t nmemb, size_t size){
  return real_calloc(nmemb,size);
}
void *malloc(size_t size){
  return real_malloc(size);
}
void *realloc(void *ptr, size_t size){
  return real_realloc(ptr,size);
}
