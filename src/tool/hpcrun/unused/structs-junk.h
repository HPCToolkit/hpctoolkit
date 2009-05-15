/* not currently used */
/* contents of the persistent state file for the profiler */
typedef struct csprof_pstate_t { 
    long int hostid;            /* host id of the machine FIXME */
    pid_t pid;                  /* the process's pid */
    pthread_t thrid;            /* the particular thread */
    unsigned int ninit;         /* how many times the pid has been init'd */
} csprof_pstate_t;

typedef enum csprof_status_e {
  CSPROF_STATUS_UNINIT = 0, /* lib has not been initialized at all */
  CSPROF_STATUS_INIT,       /* lib has been initialized */
  CSPROF_STATUS_FINI        /* lib has been finalized (after init) */
} csprof_status_t;
