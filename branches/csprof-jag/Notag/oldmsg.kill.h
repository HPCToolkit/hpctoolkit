#ifdef NO
static inline void MSG(int level, char *format, ...)
#if defined(CSPROF_PERF)
{
}
#else
{
    va_list args;
    char buf[512];
    va_start(args, format);
#if 0
    if (level & CSPROF_MSG_LVL) {
#else
    {
#endif
        int n;
#ifdef CSPROF_THREADS
        pthread_mutex_lock(&mylock);
#endif
        flockfile(stderr);
#ifdef CSPROF_THREADS
	n = sprintf(buf, "csprof msg [%d][%lx]: ", level, pthread_self());
#else
        n = sprintf(buf, "csprof msg [%d]: ", level);
#endif
        PRINT(buf);
        n = vsprintf(buf, format, args);
        PRINT(buf);
        PSTR("\n");
        fflush_unlocked(stderr);
        funlockfile(stderr);
#ifdef CSPROF_THREADS
        pthread_mutex_unlock(&mylock);
#endif
    }
    va_end(args);
}
#endif
#endif

#ifdef NO
static inline void EMSG(char *format, ...){
  va_list args;
  char buf[512];
  int n;
  va_start(args, format);

#ifdef CSPROF_THREADS
  pthread_mutex_lock(&mylock);
#endif
  flockfile(stderr);
#ifdef CSPROF_THREADS
  n = sprintf(buf, "EMSG[%lx]: ", pthread_self());
#else
  n = sprintf(buf, "EMSG: ");
#endif
  PRINT(buf);
  n = vsprintf(buf, format, args);
  PRINT(buf);
  PSTR("\n");
  fflush_unlocked(stderr);
  funlockfile(stderr);
#ifdef CSPROF_THREADS
  pthread_mutex_unlock(&mylock);
#endif
  va_end(args);

}
#endif

