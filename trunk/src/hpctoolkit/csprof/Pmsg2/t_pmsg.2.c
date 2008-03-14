#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "pmsg2.h"
#include "pmsg_lookup.h"

static int _find(char *s){
  int i;
  _item_t *item;

  for(i=0,item = &lookup[0]; i < sizeof(lookup)/sizeof(_item_t); item++,i++){
    // printf("item[%d] = %s -> %d\n",i,item->key,item->val);
    if (! strcmp(s,item->key)){
      return item->val;
    }
  }
  return -1;
}

#define CHECK(s) do { \
  int _v = _find(s); \
  if (_v >= 0){ \
    printf("%s found, val = %d\n",s,_v); \
  } \
  else { \
    printf("%s NOT found\n",s); \
  } \
} while(0)

int main(void){
  char tstr[] = "foo bar baz frob";
  char *tok;

  for(tok = strtok(tstr," ");tok;tok = strtok(NULL," ")){
    printf("token = %s\n",tok);
  }
  CHECK("foo");
  CHECK("bar");
  CHECK("baz");

  return 0;
}
