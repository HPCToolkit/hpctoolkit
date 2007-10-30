#ifdef NOMON
#ifdef __GNUC__
void csprof_init_internal() __attribute__((constructor));
void csprof_fini_internal() __attribute__((destructor));
#endif

#ifndef __GNUC__
/* Implicit interface */
void
_init()
{
  csprof_init_internal();
}

void
_fini()
{
  csprof_fini_internal();
}
#endif
#endif /* NOMON */
