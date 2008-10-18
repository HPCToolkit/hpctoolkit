#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <ucontext.h>

#include "general.h"
#include "util.h"
#include "mem.h"
#include "unsafe.h"

extern void *__start;

/* FIXME: these constants need to be automagically generated (and sorted!) */

/* double ugh */
#ifdef CSPROF_FIXED_LIBCALLS
extern void *libcall1_end;
extern void *libcall2_end;
extern void *libcall3_end;
extern void *libcall4_end;
#endif

/* these arrays should be automagically generated in some fashion */
/* we don't include the trampoline function and so forth in here because
   we don't know where libcsprof is going to be loaded.  I suppose we
   could throw them into the mix and then qsort() everything before we
   begin... FIXME? */
struct bracket {
    void *lo;
    void *hi;
};

static void *unsafe_procedure_start_addresses[] =
{
  /* loader */
  (void *) 0x3ff80006790,

  /* libc */
  (void *) 0x3ff800d1e70,	/* cerror, invalid RPD */
  (void *) 0x3ff800d3fa0,       /* seterrno */
  (void *) 0x3ff800d5680,       /* sigprocmask */
  (void *) 0x3ff800d7d30,       /* sigprocmask */
  (void *) 0x3ff800dc1a0,       /* exc_add_gp_range */
  (void *) 0x3ff800dde90,       /* exit */
  (void *) 0x3ff800de050,       /* exc_add_pc_range_table */
  (void *) 0x3ff800e06c0,
  (void *) 0x3ff800e0960,
  (void *) 0x3ff800e37e0,
  (void *) 0x3ff801159b0,
  (void *) 0x3ff8011de70,
  (void *) 0x3ff801b2ce0,
                       
  (void *) 0x3ff801ed290,
                       
  (void *) 0x3ff80259a60,
#if defined(CSPROF_PTHREADS)                       
  (void *) 0x3ff80576560,
  (void *) 0x3ff8057d190,
#endif
};

static void *unsafe_procedure_end_addresses[] =
{
  (void *) 0x3ff800225d0, /* loader */
      
  /* libc */
  (void *) 0x3ff800d1eac,       /* cerror: invalid RPD */
  (void *) 0x3ff800d3fdc,       /* seterrno: calls cerror */
  (void *) 0x3ff800d571c,       /* sigprocmask: might be in libcsprof */
  (void *) 0x3ff800d7dbc,       /* sigprocmask: The Return */
  (void *) 0x3ff800dc27c,       /* exc_add_gp_range */
  (void *) 0x3ff800de04c,       /* exit */
  (void *) 0x3ff800de15c,       /* exc_add_pc_range_table */
  (void *) 0x3ff800e079c,       /* setjmp: nlx */
  (void *) 0x3ff800e0a3c,       /* sigstack */
  (void *) 0x3ff800e38dc,       /* sigsetjmp: nlx */
  (void *) 0x3ff80115a5c,       /* random conversion */
  (void *) 0x3ff8011dedc,       /* longjmp: nlx */
  (void *) 0x3ff801b2d3c,       /* longjmp_resume */
      
  (void *) 0x3ff801eebac,       /* mutex-related stuff */

  /* libm */
  (void *) 0x3ff80259c78,
      
  /* pthreads */
#if defined(CSPROF_PTHREADS)
  (void *) 0x3ff8057669c, /* pthread_sigmask */
  (void *) 0x3ff8057d2b8 /* __upcUpcall */
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
        if(*mid < x) {
            lo = mid + 1;
        }
        else {
            hi = mid;
        }
    }

    return lo - array - 1;
}

static inline int
csprof_addr_is_unsafe_in_table(void *addr, void **start_table, void **end_table, size_t n_entries)
{
  void *lower = start_table[0];
  void *upper = end_table[n_entries-1];

  if(addr < lower || addr > upper) {
    /* doesn't even lie in the unsafe range */
    return 0;
  }
  else {
    unsigned int pos = bisect(start_table, addr, n_entries);

    DBGMSG_PUB(1, "Found table[%d] = %p for address %p",
	       pos, start_table[pos], addr);

    return start_table[pos] <= addr && addr <= end_table[pos];
  }
}

#define N_ELEMS(x) (sizeof((x))/sizeof((x)[0]))

static inline int
csprof_is_unsafe_library(void *addr)
{
    return csprof_addr_is_unsafe_in_table(addr,
					  unsafe_procedure_start_addresses,
					  unsafe_procedure_end_addresses,
					  N_ELEMS(unsafe_procedure_start_addresses));
}

static int
csprof_addr_is_unsafe(void *addr)
{
    return csprof_is_unsafe_library(addr);
}

int
csprof_context_is_unsafe(void *context)
{
    struct ucontext *ctx = (struct ucontext *)context;
    void *pc = ctx->uc_mcontext.sc_pc;
    /* with the old scheme (hardcode everything into the signal handler),
       we only checked the return address for exit()...but things have
       gotten so much faster with this scheme that we might as well check
       the return address for everything */
    void *ra = ctx->uc_mcontext.sc_regs[26];

    return
#if defined(CSPROF_FIXED_LIBCALLS)
        /* ugh, we were *trying* to get away from this sort of thing */
        /* take advantage of the libcall$ being contiguious in the source */
#if defined(CSPROF_THREADS)
        ((&libcall1 <= pc) && (pc <= &libcall4_end)) ||
#else
        ((&libcall2 <= pc) && (pc <= &libcall3_end)) ||
#endif
#endif
        /* don't do anything before main() begins, either */
        (((void *)pc) < &__start) ||
        csprof_addr_is_unsafe(pc) || csprof_is_unsafe_library(ra);
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
