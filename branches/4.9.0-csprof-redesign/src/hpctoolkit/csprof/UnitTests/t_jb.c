#include <setjmp.h>
#include <stdio.h>

static jmp_buf jb;

typedef struct {
  jmp_buf unw;
} _jb_type;

_jb_type *get_it(void){
  return (_jb_type *)&jb;
}

void goback(void){
  _jb_type *it = get_it();

  longjmp(it->unw,1);
}

void inner(void){
  printf("Hit the inner routine, about to goback\n");
  goback();
}

int main(int argc, char *argv[]){
  _jb_type *it = get_it();

  if (!setjmp(it->unw)){
    inner();
    printf("Normal return from inner\n");
  }
  else {
    printf("AB normal return f inner\n");
  }
  printf("Past entire loop\n");
  return 0;
}
