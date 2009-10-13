// Forward defs, override in preload

void csprof_set_rank(int rank) __attribute__ ((weak, alias ("_csprof_set_rank")));
void _csprof_set_rank(int rank){
}

