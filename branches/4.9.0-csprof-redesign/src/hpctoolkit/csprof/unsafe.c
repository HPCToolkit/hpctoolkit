#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <ucontext.h>

#include "mem.h"
#include "interface.h"

/* FIXME: these constants need to be automagically generated */

/* don't even think about trying to do anything clever while we're in
   the dynamic loader. /sbin/loader contains *no* runtime procedure
   descriptor information whatsoever.  so these constants are the
   endpoints of the .text section for /sbin/loader */
#define LOADER_START_ADDRESS ((void *)0x3ff80006790)
#define LOADER_END_ADDRESS ((void *)0x3ff800225d0)

#define PTHREAD_SIGMASK_START_ADDRESS ((void *)0x3ff80576560)
#define PTHREAD_SIGMASK_END_ADDRESS ((void *)0x3ff805766a0)
#define EXIT_START_ADDRESS ((void *)0x3ff800dde90)
#define EXIT_END_ADDRESS ((void *)0x3ff800de050)
/* amazing.  there are *two* different sigprocmask's in libc */
#define SIGPROCMASK_START_ADDRESS ((void *)0x3ff800d5680)
#define SIGPROCMASK_END_ADDRESS ((void *)0x3ff800d571c)
#define SIGPROCMASK_REAL_START_ADDRESS ((void *)0x3ff800d7d30)
#define SIGPROCMASK_REAL_END_ADDRESS ((void *)0x3ff800d7dbc)
/* longjmp seems to be problematic, too.  *sigh* */
#define LONGJMP_START_ADDRESS ((void *)0x3ff8011de70)
#define LONGJMP_END_ADDRESS ((void *)0x3ff8011dedc)
/* in multiple ways */
#define LONGJUMP_RESUME_START_ADDRESS ((void *)0x3ff801b2ce0)
#define LONGJUMP_RESUME_END_ADDRESS ((void *)0x3ff801b2d3c)
/* there has got to be a better way to disallow sigsetjmp--or even
   allow it in some very limited contexts */
#define SIGSETJMP_START_ADDRESS ((void *)0x3ff800e37e0)
#define SIGSETJMP_END_ADDRESS ((void *)0x3ff800e38dc)
/* sigh */
#define SIGSTACK_START_ADDRESS ((void *)0x3ff800e0960)
#define SIGSTACK_END_ADDRESS ((void *)0x3ff800e0a3c)

/* this is a truly evil libm function */
/* why do this, you ask?  Turns out that function at 0x3ff80259a60 in
   libm does something extremely funky on our version of Tru64 Unix.  it
   declares it uses the stack, saves the return address on the stack,
   but in some code paths, doesn't reload the saved address from the
   stack, instead preferring to use the value directly from the register
   (which hasn't been modified).  this is a slimy practice and should be
   reported as a bug; we are doing something equally slimy as a counter */
#define LIBM_EVIL_FUNCTION_START_ADDRESS ((void *)0x3ff80259a60)
#define LIBM_EVIL_FUNCTION_END_ADDRESS ((void *)0x3ff80259c78)

/* `__upcUpcall', for whatever reason, is not recognized as a linkable
   symbol, even though `nm' informs us that said symbol can be located
   in libpthread.so.  hence, we define it here ourselves...very fragile */
#define CSPROF__UPCUPCALL_START_ADDRESS ((void *)0x3ff8057d190)
#define CSPROF__UPCUPCALL_END_ADDRESS ((void *)0x3ff8057d2b8)

/* double ugh */
#ifdef CSPROF_FIXED_LIBCALLS
extern void *libcall1_end;
extern void *libcall2_end;
extern void *libcall3_end;
extern void *libcall4_end;
#endif

/* these arrays need to be sorted lowest address to highest */
/* these arrays should also be automagically generated in some fashion */
/* we don't include the trampoline function and so forth in here because
   we don't know where libcsprof is going to be loaded.  I suppose we
   could throw them into the mix and then qsort() everything before we
   begin... FIXME? */
static void *invalid_static_procedures_start[] =
{
    /* loader gets first dibs */
    LOADER_START_ADDRESS,

    /* libc has a couple places we fear to tread */
    SIGPROCMASK_START_ADDRESS,
    SIGPROCMASK_REAL_START_ADDRESS,
    EXIT_START_ADDRESS,
    SIGSTACK_START_ADDRESS,
    SIGSETJMP_START_ADDRESS,
    LONGJMP_START_ADDRESS,
    LONGJUMP_RESUME_START_ADDRESS,
    
    /* libm has at least one evil function */
    LIBM_EVIL_FUNCTION_START_ADDRESS,

#ifdef CSPROF_THREADS
    /* pthread has some nasty places */
    PTHREAD_SIGMASK_START_ADDRESS,
    CSPROF__UPCUPCALL_START_ADDRESS
#endif
};

static void *invalid_static_procedures_end[] =
{
    /* and now for the ending constants */
    LOADER_END_ADDRESS,

    /* libc */
    SIGPROCMASK_END_ADDRESS,
    SIGPROCMASK_REAL_END_ADDRESS,
    EXIT_END_ADDRESS,
    SIGSTACK_END_ADDRESS,
    SIGSETJMP_END_ADDRESS,
    LONGJMP_END_ADDRESS,
    LONGJUMP_RESUME_END_ADDRESS,

    /* libm */
    LIBM_EVIL_FUNCTION_END_ADDRESS,

#ifdef CSPROF_THREADS
    /* pthreads */
    PTHREAD_SIGMASK_END_ADDRESS,
    CSPROF__UPCUPCALL_END_ADDRESS
#endif
};

static void **invalid_loaded_procedures_start = NULL;
static void **invalid_loaded_procedures_end = NULL;
static size_t n_loaded_procedures;

static unsigned int
bisect(void **array, void *x, size_t n)
{
    void **lo = array;
    void **hi = array + n;

    while(lo < hi) {
        size_t dist = hi - lo;
        void **mid = lo + (dist/2);
        if(x < *mid) {
            hi = mid;
        }
        else {
            lo = mid + 1;
        }
    }

    return lo - array;
}

static inline int
csprof_addr_is_unsafe_in_table(void *addr, void **table_start,
                               void **table_end, size_t n_entries)
{
    
  /*** MWF GIANT HACK:
   *** Assume this is ok for the moment
   ***/

    return 0;

    if(table_start == NULL || table_end == NULL) {
        /* can't tell */
        return 0;
    }
    else {
        void *lower = table_start[0];
        void *upper = table_end[n_entries-1];

        if(addr < lower || addr > upper) {
            /* doesn't even lie in the unsafe range */
            return 0;
        }
        else {
            unsigned int pos = bisect(&table_start[0], addr, n_entries);

            return addr <= table_end[pos];
        }
    }
}

static inline int
csprof_is_unsafe_library(void *addr)
{
    MSG(1,"calling is unsafe lib");
  /***  MWF GIANT HACK for NOW  ***/
    return 0;
  /***  END HACK ***/
#ifdef NO
    return csprof_addr_is_unsafe_in_table(addr, invalid_static_procedures_start,
                                          invalid_static_procedures_end,
                                          sizeof(invalid_static_procedures_start)/sizeof(invalid_static_procedures_start[0]));
#endif
}

static int
csprof_addr_is_unsafe(void *addr)
{
    MSG(1,"checking addr is unsafe: %lx",addr);
    return csprof_is_unsafe_library(addr);
#if 0
        || csprof_addr_is_unsafe_in_table(addr, invalid_loaded_procedures_start,
                                          invalid_loaded_procedures_start, n_loaded_procedures);
#endif
}

int
csprof_context_is_unsafe(void *context)
{
    extern int s3;
    struct ucontext *ctx = (struct ucontext *)context;
    void *pc = ctx->uc_mcontext.sc_pc;
    /* with the old scheme (hardcode everything into the signal handler),
       we only checked the return address for exit()...but things have
       gotten so much faster with this scheme that we might as well check
       the return address for everything */
    void *ra = ctx->uc_mcontext.sc_regs[26];

    s3 += 1;
    MSG(1,"csprof context is unsafe called: pc = %lx, ra = %lx",pc,ra);
    return 0;

#ifdef NO    
    return
#if defined(CSPROF_FIXED_LIBCALLS)
       /* ugh, we were *trying* to get away from this sort of thing */
       ((&libcall2 <= pc) && (pc <= &libcall2_end)) ||
       ((&libcall3 <= pc) && (pc <= &libcall3_end)) ||
#if defined(CSPROF_THREADS)
       ((&libcall1 <= pc) && (pc <= &libcall1_end)) ||
       ((&libcall4 <= pc) && (pc <= &libcall4_end)) ||
#endif
#endif
        csprof_addr_is_unsafe(pc) || csprof_is_unsafe_library(ra);
#endif
}

void
csprof_add_unsafe_addresses_from_file(char *filename)
{
#if 1
    return;
#else
    int fd = open(filename, O_RDONLY);
    size_t n_bytes;
    unsigned long n_addrs;
    size_t n_addr_bytes;

    if(fd == -1) {
        return;
    }

    n_bytes = read(fd, &n_addrs, sizeof(n_addrs));

    if(n_bytes != sizeof(n_addrs)) {
        goto clean_fd;
    }

    n_addr_bytes = n_addrs * sizeof(void *);

    invalid_loaded_procedures_start = csprof_malloc(n_addr_bytes);

    if(invalid_loaded_procedures_start == NULL) {
        goto clean_fd;
    }

    invalid_loaded_procedures_end = csprof_malloc(n_addr_bytes);

    if(invalid_loaded_procedures_end == NULL) {
        goto clean_procedures_start;
    }

    n_bytes = read(fd, &invalid_loaded_procedures_start, n_addr_bytes);

    if(n_bytes != n_addr_bytes) {
        goto clean_procedures_end;
    }

    n_bytes = read(fd, &invalid_loaded_procedures_end, n_addr_bytes);

    if(n_bytes != n_addr_bytes) {
        goto clean_procedures_end;
    }

    n_loaded_procedures = n_addrs;

    goto clean_fd;

 clean_procedures_end:
    free(invalid_loaded_procedures_end);
    invalid_loaded_procedures_end = NULL;
 clean_procedures_start:
    free(invalid_loaded_procedures_start);
    invalid_loaded_procedures_start = NULL;
 clean_fd:
    close(fd);

    return;
#endif
}
