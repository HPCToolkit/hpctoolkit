/* FIXME: ugly platform-dependent stuff */
#ifdef CSPROF_FIXED_LIBCALLS
int (*csprof_pthread_create)(pthread_t *, const pthread_attr_t *,
                             pthread_func *, void *)
    = 0x3ff805877d0;
void (*csprof_pthread_exit)(void *)
    = 0x3ff80587c70;
int (*csprof_pthread_sigmask)(int, const sigset_t *, sigset_t *)
    = 0x3ff80576560;
#else
int (*csprof_pthread_create)(pthread_t *, const pthread_attr_t *,
                             pthread_func *, void *);
void (*csprof_pthread_exit)(void *);
#endif
