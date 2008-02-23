/* override various functions here */

int _csprof_in_malloc = 0;
int csprof_need_more = 0;

void *malloc(size_t s){
  void *alloc;

  _csprof_in_malloc = 1;
  alloc = real_malloc(s);
  if (csprof_need_more){
    /* alloc more space here */
    csprof_need_more = 0;
  }
  _csprof_in_malloc = 0;
}

void *calloc(size_t c,size_t s){
}

void *valloc(size_t s){
}

realloc(void *p,size_t s){
}

reallocf(void *p,size_t s{
}
