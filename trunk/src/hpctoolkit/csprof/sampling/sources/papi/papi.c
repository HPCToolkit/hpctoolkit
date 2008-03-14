/* driver implementation using PAPI's overflow signal support */

#include "papi.h"
#include "env.h"
#include "general.h"
#include "xpthread.h"
#include "structs.h"
#include "csproflib_private.h"


/* the event table */

typedef struct papi_event {
  int code;                 /* PAPI event code */
  const char *name;         /* PAPI event name */
  const char *description;  /* Event description */
} papi_event_t;

papi_event_t csprof_event_table[] = {
{ PAPI_L1_DCM,  "PAPI_L1_DCM",  "Level 1 data cache misses" },
{ PAPI_L1_ICM,  "PAPI_L1_ICM",  "Level 1 instruction cache misses" },
{ PAPI_L2_DCM,  "PAPI_L2_DCM",  "Level 2 data cache misses" },
{ PAPI_L2_ICM,  "PAPI_L2_ICM",  "Level 2 instruction cache misses" },
{ PAPI_L3_DCM,  "PAPI_L3_DCM",  "Level 3 data cache misses" },
{ PAPI_L3_ICM,  "PAPI_L3_ICM",  "Level 3 instruction cache misses" },
{ PAPI_L1_TCM,  "PAPI_L1_TCM",  "Level 1 total cache misses" },
{ PAPI_L2_TCM,  "PAPI_L2_TCM",  "Level 2 total cache misses" },
{ PAPI_L3_TCM,  "PAPI_L3_TCM",  "Level 3 total cache misses" },
{ PAPI_CA_SNP,  "PAPI_CA_SNP",  "Snoops" },
{ PAPI_CA_SHR,  "PAPI_CA_SHR",  "Request access to shared cache line (SMP)" },
{ PAPI_CA_CLN,  "PAPI_CA_CLN",  "Request access to clean cache line (SMP)" },
{ PAPI_CA_INV,  "PAPI_CA_INV",  "Cache Line Invalidation (SMP)" },
{ PAPI_CA_ITV,  "PAPI_CA_ITV",  "Cache Line Intervention (SMP)" },
{ PAPI_L3_LDM,  "PAPI_L3_LDM",  "Level 3 load misses " },
{ PAPI_L3_STM,  "PAPI_L3_STM",  "Level 3 store misses " },
{ PAPI_BRU_IDL, "PAPI_BRU_IDL", "Cycles branch units are idle" },
{ PAPI_FXU_IDL, "PAPI_FXU_IDL", "Cycles integer units are idle" },
{ PAPI_FPU_IDL, "PAPI_FPU_IDL", "Cycles floating point units are idle" },
{ PAPI_LSU_IDL, "PAPI_LSU_IDL", "Cycles load/store units are idle" },
{ PAPI_TLB_DM,  "PAPI_TLB_DM",  "Data translation lookaside buffer misses" },
{ PAPI_TLB_IM,  "PAPI_TLB_IM",  "Instr translation lookaside buffer misses" },
{ PAPI_TLB_TL,  "PAPI_TLB_TL",  "Total translation lookaside buffer misses" },
{ PAPI_L1_LDM,  "PAPI_L1_LDM",  "Level 1 load misses " },
{ PAPI_L1_STM,  "PAPI_L1_STM",  "Level 1 store misses " },
{ PAPI_L2_LDM,  "PAPI_L2_LDM",  "Level 2 load misses " },
{ PAPI_L2_STM,  "PAPI_L2_STM",  "Level 2 store misses " },
{ PAPI_L1_DCH,  "PAPI_L1_DCH",  "Level 1 D cache hits" },
{ PAPI_L2_DCH,  "PAPI_L2_DCH",  "Level 2 D cache hits" },
{ PAPI_L3_DCH,  "PAPI_L3_DCH",  "Level 3 D cache hits" },
{ PAPI_TLB_SD,  "PAPI_TLB_SD",  "TLB shootdowns" },
{ PAPI_CSR_FAL, "PAPI_CSR_FAL", "Failed store conditional instructions" },
{ PAPI_CSR_SUC, "PAPI_CSR_SUC", "Successful store conditional instructions" },
{ PAPI_CSR_TOT, "PAPI_CSR_TOT", "Total store conditional instructions" },
{ PAPI_MEM_SCY, "PAPI_MEM_SCY", "Cycles Stalled Waiting for Memory Access" },
{ PAPI_MEM_RCY, "PAPI_MEM_RCY", "Cycles Stalled Waiting for Memory Read" },
{ PAPI_MEM_WCY, "PAPI_MEM_WCY", "Cycles Stalled Waiting for Memory Write" },
{ PAPI_STL_ICY, "PAPI_STL_ICY", "Cycles with No Instruction Issue" },
{ PAPI_FUL_ICY, "PAPI_FUL_ICY", "Cycles with Maximum Instruction Issue" },
{ PAPI_STL_CCY, "PAPI_STL_CCY", "Cycles with No Instruction Completion" },
{ PAPI_FUL_CCY, "PAPI_FUL_CCY", "Cycles with Maximum Instruction Completion" },
{ PAPI_HW_INT,  "PAPI_HW_INT",  "Hardware interrupts " },
{ PAPI_BR_UCN,  "PAPI_BR_UCN",  "Unconditional branch instructions executed" },
{ PAPI_BR_CN,   "PAPI_BR_CN",   "Conditional branch instructions executed" },
{ PAPI_BR_TKN,  "PAPI_BR_TKN",  "Conditional branch instructions taken" },
{ PAPI_BR_NTK,  "PAPI_BR_NTK",  "Conditional branch instructions not taken" },
{ PAPI_BR_MSP,  "PAPI_BR_MSP",  "Conditional branch instructions mispred" },
{ PAPI_BR_PRC,  "PAPI_BR_PRC",  "Conditional branch instructions corr. pred" },
{ PAPI_FMA_INS, "PAPI_FMA_INS", "FMA instructions completed" },
{ PAPI_TOT_IIS, "PAPI_TOT_IIS", "Total instructions issued" },
{ PAPI_TOT_INS, "PAPI_TOT_INS", "Total instructions executed" },
{ PAPI_INT_INS, "PAPI_INT_INS", "Integer instructions executed" },
{ PAPI_FP_INS,  "PAPI_FP_INS",  "Floating point instructions executed" },
{ PAPI_LD_INS,  "PAPI_LD_INS",  "Load instructions executed" },
{ PAPI_SR_INS,  "PAPI_SR_INS",  "Store instructions executed" },
{ PAPI_BR_INS,  "PAPI_BR_INS",  "Total branch instructions executed" },
{ PAPI_VEC_INS, "PAPI_VEC_INS", "Vector/SIMD instructions executed" },
#if 0
{ PAPI_FLOPS,   "PAPI_FLOPS",   "Floating Point instructions per second" },
#endif
{ PAPI_RES_STL, "PAPI_RES_STL", "Any resource stalls" },
{ PAPI_FP_STAL, "PAPI_FP_STAL", "FP units are stalled " },
{ PAPI_TOT_CYC, "PAPI_TOT_CYC", "Total cycles" },
#if 0
{ PAPI_IPS,     "PAPI_IPS",     "Instructions executed per second" },
#endif
{ PAPI_LST_INS, "PAPI_LST_INS", "Total load/store inst. executed" },
{ PAPI_SYC_INS, "PAPI_SYC_INS", "Synchronization instructions executed" },

#if 0 /* Begin Rice I2 additions (FIXME) */
{ ITA2_BACK_END_BUBBLE_ALL, "ITA2_BACK_END_BUBBLE_ALL", "<fixme>" },
#endif /* End Rice I2 additions */

{ -1,            NULL,          "Invalid event type" }
};


/* functions for finding events */
const papi_event_t *
csprof_event_by_name(const char *name)
{ 
    papi_event_t *i = csprof_event_table;
    for (; i->name != NULL; i++) {
        if (strcmp(name, i->name) == 0) return i;
    }
    return NULL;
}

const papi_event_t *
csprof_event_by_code(int code)
{
    papi_event_t *i = csprof_event_table;
    for (; i->name != NULL; i++) {
        if (i->code == code) return i;
    }
    return NULL;
}


/* the actual code */

#define CSPROF_PAPI_DEFAULT_EVENT PAPI_TOT_CYC
#define CSPROF_EVENT_THRESHOLD 100000000UL

#ifdef CSPROF_THREADS
static pthread_key_t papi_eventset_key;
#else
static int csprof_event_set = PAPI_NULL;
#endif

/* FIXME: need to figure out some way to support multiple events */
static int csprof_papi_event;

static int
csprof_get_papi_event_set()
{
#ifdef CSPROF_THREADS
    return (int)pthread_getspecific(papi_eventset_key);
#else
    return csprof_event_set;
#endif
}

static void
csprof_set_papi_event_set(int event_set)
{
#ifdef CSPROF_THREADS
    pthread_setspecific(papi_eventset_key, (void *)event_set);
#else
    csprof_event_set = event_set;
#endif
}

/* FIXME: should allow multiple events */
static int
csprof_create_papi_event_set(int event)
{
    int the_event_set, ret;

    ret = PAPI_create_eventset(&the_event_set);

    if(ret != PAPI_OK) {
        DIE("Couldn't create PAPI event set", __FILE__, __LINE__);
    }

    /* FIXME: need to get real event handling code in here */
    ret = PAPI_add_event(the_event_set, event);

    if(ret != PAPI_OK) {
        DIE("Couldn't add events to PAPI event set", __FILE__, __LINE__);
    }

    return the_event_set;
}

static int
csprof_determine_papi_event()
{
    char *s = getenv(CSPROF_OPT_EVENT);

    if(s) {
        const papi_event_t *event = csprof_event_by_name(s);
        /* some global variable somewhere needs to be set */
        if(event == NULL) {
            DIE("value of option `%s' [%s] not a valid PAPI event name,"
                __FILE__, __LINE__, CSPROF_OPT_EVENT, s);
        }
        else {
            return event->code;
        }
    }
    else {
        return CSPROF_PAPI_DEFAULT_EVENT;
    }
}


/* the core functions of the driver */

static void
csprof_papi_signal_handler(int event_set, void *address,
                           long_long overflow_vector, void *context)
{
    CSPROF_SIGNAL_HANDLER_GUTS(context);
}

void
csprof_driver_init(csprof_state_t *state, csprof_options_t *opts)
{
    int ret;
    int the_event_set;
#ifdef CSPROF_THREADS
    long thread = pthread_self();
#else
    long thread = 0;
#endif

    ret = PAPI_library_init(PAPI_VER_CURRENT);

    if(ret != PAPI_VER_CURRENT) {
        DIE("PAPI library initialization error", __FILE__, __LINE__);
    }

#ifdef CSPROF_THREADS
    ret = PAPI_thread_init(pthread_self);

    if(ret != PAPI_OK) {
        DIE("PAPI thread initialization error", __FILE__, __LINE__);
    }

    pthread_key_create(&papi_eventset_key, NULL);
#endif

    csprof_papi_event = csprof_determine_papi_event();

    the_event_set = csprof_create_papi_event_set(csprof_papi_event);

    csprof_set_papi_event_set(the_event_set);

    ret = PAPI_overflow(the_event_set, csprof_papi_event,
                        opts->sample_period, 0, csprof_papi_signal_handler);

    if(ret != PAPI_OK) {
        DIE("Couldn't set overflow handler on thread %lx", __FILE__,
            __LINE__, thread);
    }

    ret = PAPI_start(the_event_set);

    if(ret != PAPI_OK) {
        /* this is probably bad */
        DIE("Couldn't initialize PAPI on thread %lx", __FILE__, __LINE__, thread);
    }

    sigemptyset(&prof_sigset);
    sigaddset(&prof_sigset, PAPI_SIGNAL);
}

void
csprof_driver_fini(csprof_state_t *state, csprof_options_t *opts)
{
    int the_event_set = csprof_get_papi_event_set();

    PAPI_stop(the_event_set, NULL);
    PAPI_rem_event(&the_event_set, csprof_papi_event);
    PAPI_destroy_eventset(&the_event_set);

    the_event_set = PAPI_NULL;
    csprof_set_papi_event_set(the_event_set);
}

#ifdef CSPROF_THREADS

void
csprof_driver_thread_init(csprof_state_t *state)
{
    int ret;
    int the_event_set;

    the_event_set = csprof_create_papi_event_set(CSPROF_PAPI_EVENT);

    csprof_set_papi_event_set(the_event_set);

    ret = PAPI_start(the_event_set);

    if(ret != PAPI_OK) {
        /* this is probably bad */
        DIE("Couldn't initialize PAPI on thread %lx", __FILE__, __LINE__,
            pthread_self());
    }
}

void
csprof_driver_thread_fini(csprof_state_t *state)
{
    int ret;
    int the_event_set = csprof_get_papi_event_set();

    MSG(1,"PAPI csprof driver thread fini");
    ret = PAPI_stop(the_event_set, NULL);

    if(ret != PAPI_OK) {
        DIE("Couldn't stop PAPI on thread %lx", __FILE__, __LINE__,
            pthread_self());
    }

    PAPI_rem_event(&the_event_set, csprof_papi_event);
    PAPI_destroy_eventset(&the_event_set);

    the_event_set = PAPI_NULL;
    csprof_set_papi_event_set(the_event_set);
}

#endif

void
csprof_driver_suspend(csprof_state_t *state)
{
    int reg;

    ret = PAPI_stop(csprof_get_papi_event_set(), NULL);

    if(ret != PAPI_OK) {
        DIE("Couldn't suspend PAPI", __FILE__, __LINE__);
    }
}

void csprof_driver_resume(csprof_state_t *state)
{
    int reg;

    ret = PAPI_start(csprof_get_papi_event_set(), NULL);

    if(ret != PAPI_OK) {
        DIE("Couldn't resume PAPI", __FILE__, __LINE__);
    }
}
