#ifdef CSPROF_THREADS
#define EMSG(fmt,...) do {\
 char buf[256]; \
 int n; \
 n = sprintf(buf, "EMSG[%lx]: " fmt "\n",pthread_self(), ##__VA_ARGS__); \
 WRITE(2,buf,n); \
} while(0)
#else
#define EMSG(fmt,...) do {\
 char buf[256]; \
 int n; \
 n = sprintf(buf,"EMSG: " fmt "\n", ##__VA_ARGS__); \
 WRITE(2,buf,n); \
} while(0)
#endif

#ifdef CSPROF_THREADS
#define D1 "[%d][%lx]: "
#define DA(l) l, pthread_self()
#else
#define D1 "[%d]: "
#define DA(l) l
#endif

#if defined(CSPROF_PERF)
#define DBGMSG_PUB(l,fmt,...)
#else
#define DBGMSG_PUB(l,fmt,...) do {\
 char buf[256]; \
 int n; \
 n = sprintf(buf,D1 fmt "\n", DA(l), ##__VA_ARGS__); \
 WRITE(2,buf,n); \
} while(0)
#endif

#define ERRMSGa(fmt,f,l,...) do {\
 char buf[256]; \
 int n; \
 n = sprintf(buf,"csprof error [%s:%d]:" fmt "\n", f, l, ##__VA_ARGS__); \
 WRITE(2,buf,n); \
} while(0)

#define ERRMSG(fmt,f,l,...) ERRMSGa(fmt,f,l, ##__VA_ARGS__)

#define DIE(fmt,f,l,...) do {\
  ERRMSG(fmt,f,l, ##__VA_ARGS__); \
  abort(); \
} while(0)

#ifdef INLINE_FN
static inline void MSG(int level, char *format, ...)
#if defined(CSPROF_PERF)
{
}
#else
{
  va_list args;
  char fstr[256];
  char buf[512];
  va_start(args, format);
  int n;

#ifdef CSPROF_THREADS
  sprintf(fstr,"csprof msg [%d][%lx]: ",level, pthread_self());
#else
  sprintf(fstr,"csprof msg [%d]: ",level);
#endif
  strcat(fstr,format);
  strcat(fstr,"\n");
  n = vsprintf(buf,fstr,args);
  WRITE(2,buf,n);
  va_end(args);
}
#endif

static inline void EMSG(char *format, ...){
  va_list args;
  char fstr[256];
  char buf[512];
  va_start(args, format);
  int n;

#ifdef CSPROF_THREADS
  n = sprintf(fstr, "EMSG[%lx]: ", pthread_self());
#else
  n = sprintf(fstr, "EMSG: ");
#endif
  strcat(fstr,format);
  strcat(fstr,"\n");
  n = vsprintf(buf,fstr,args);
  WRITE(2,buf,n);
  va_end(args);
}

static inline void DBGMSG_PUB(int level, char *format, ...)
#if defined(CSPROF_PERF)
{
}
#else
{
  va_list args;
  char fstr[256];
  char buf[512];
  va_start(args, format);
  int n;

#ifdef CSPROF_THREADS
  sprintf(fstr,"[%d][%lx]: ",level, pthread_self());
#else
  sprintf(fstr,"[%d]: ",level);
#endif
  strcat(fstr,format);
  strcat(fstr,"\n");
  n = vsprintf(buf,fstr,args);
  WRITE(2,buf,n);
  va_end(args);
}
#endif

static inline void ERRMSGa(char *format, char *file, int line, va_list args) 
#if defined(CSPROF_PERF)
{
}
#else
{
  char fstr[256];
  char buf[512];
  int n;

  sprintf(fstr,"csprof error [%s:%d]:", file, line);

  strcat(fstr,format);
  strcat(fstr,"\n");
  n = vsprintf(buf,fstr,args);
  WRITE(2,buf,n);
  va_end(args);
}
#endif

static inline void ERRMSG(char *format, char *file, int line, ...) 
#if 0
{
}
#else
{
    va_list args;
    va_start(args, line);
    ERRMSGa(format, file, line, args);
    va_end(args);
}
#endif

static inline void DIE(char *format, char *file, int line, ...) 
{
    va_list args;
    va_start(args, line);
    ERRMSGa(format, file, line, args);
    va_end(args);
    abort();
}
#endif

#define IFMSG(level) if (level & CSPROF_MSG_LVL)

#define IFDBG(level) if (level & CSPROF_DBG_LVL)

#define IFDBG_PUB(level) if (level & CSPROF_DBG_LVL_PUB)


/* mostly to shut the compiler up.  Digital's compiler automagically
   converts arithmetic on void pointers to arithmetic on character
   pointers, but it complains while doing so.  messages are so tiresome,
   plus it's not guaranteed that other compilers will implement the
   same semantics; this macro ensures that the semantics are the same
   everywhere. */
#define VPTR_ADD(vptr, amount) ((void *)(((char *)(vptr)) + (amount)))

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif
#endif
