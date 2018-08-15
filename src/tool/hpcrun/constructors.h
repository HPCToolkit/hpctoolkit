#ifndef __constructors__h
#define __constructors__h

#define concat_names(x,y ) x ## y
#define HPCRUN_CONSTRUCTOR(x) \
  __attribute__((constructor)) \
  void concat_names(hpcrun_constructor_, x) 

#endif
