#ifndef TOKEN_ITER
#define TOKEN_ITER
#include <string.h>
#define Token_iterate(tok, s, delim, b) \
{ \
  char* _last; \
  if (s) { \
    char _tmp[strlen(s)+1]; \
    strcpy(_tmp, s); \
    for(char* tok=strtok_r(_tmp, delim, &_last); tok; tok=strtok_r(NULL, delim, &_last)) b \
  } \
}
#endif // TOKEN_ITER
