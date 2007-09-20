#include <stdarg.h>

#define PRINT(buf) fwrite_unlocked(buf,1,n,stderr);
#define PSTR(str) do { \
size_t i = strlen(str); \
fwrite_unlocked(str, 1, i, stderr); \
} while(0)

static inline void MSG(int level, char *format, ...)
{
    va_list args;
    char buf[512];

    va_start(args, format);
    int n;

    flockfile(stderr);
    n = sprintf(buf, "csprof msg [%d][%lx]: ", level, pthread_self());
    PRINT(buf);

    n = vsprintf(buf, format, args);
    PRINT(buf);
    PSTR("\n");
    fflush_unlocked(stderr);
    funlockfile(stderr);

    va_end(args);
}
