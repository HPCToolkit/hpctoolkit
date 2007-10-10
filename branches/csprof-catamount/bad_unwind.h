#ifndef BAD_UNWIND_H
#define BAD_UNWIND_H
#include <setjmp.h>

/* jmp_buf is an array of 1 (struct) element
   consequently, a jmp_buf can be passed by ref, but a
   jmp_buf cannot be returned as a value from a function (since
   it is an array). The signature for setjmp and longjmp take
   a jmp_buf type as the first argument. The jmp_buf argument is
   passed as a pointer, since it is an array type.
   
   So, to arrange for a routine to return a pointer to jmp_buf, it
   is necessary to use the struct subterfuge, as type 'jmp_buf *'
   does not pass the type checker for setjmp, longjmp.

   Sample usage:
     _jb *routine(void){ ... }

     _jb *it = routine();

     if(! setjmp(it->jb)){
     }

  Somewhere else:

     _jb *it = routine();

     longjmp(it->jb,1);
 */

typedef struct {
  jmp_buf jb;
} _jb;

_jb *get_bad_unwind(void);
#endif
