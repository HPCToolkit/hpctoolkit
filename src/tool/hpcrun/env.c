// -*-Mode: C;-*-
// $Id$

const char* CSPROF_FNM_PFX          = "csprof.";   /* general filename prefix */
const char* CSPROF_PSTATE_FNM_SFX   = ".cspstate"; /* pstate filename suffix */
const char* CSPROF_PROFILE_FNM_SFX  = "csp";       /* profile filename suffix */
const char* CSPROF_TRACE_FNM_SFX    = "cst";       /* trace filename suffix */
const char* CSPROF_LOG_FNM_SFX      = "log";       /* debug log filename suffix */

const char* CSPROF_OPT_LUSH_AGENTS   = "CSPROF_OPT_LUSH_AGENTS";

/* Names for option environment variables */
const char* CSPROF_OPT_OUT_PATH      = "CSPROF_OPT_OUT_PATH";
const char* CSPROF_OPT_SAMPLE_PERIOD = "CSPROF_OPT_SAMPLE_PERIOD";
const char* CSPROF_OPT_MEM_SZ        = "CSPROF_OPT_MEM_SZ";
const char* CSPROF_OPT_TRACE         = "CSPROF_OPT_TRACE";

const char* CSPROF_OPT_VERBOSITY     = "CSPROF_OPT_VERBOSITY";
const char* CSPROF_OPT_DEBUG         = "CSPROF_OPT_DEBUG";

const char* CSPROF_OPT_MAX_METRICS   = "CSPROF_OPT_MAX_METRICS";

const char* SWITCH_TO_PAPI           = "SWITCH_TO_PAPI";
const char* PAPI_EVENT_LIST          = "PAPI_EVENT_LIST";

#ifdef CSPROF_PAPI
const char* CSPROF_OPT_EVENT         = "CSPROF_OPT_PAPI_EVENT";
#endif
