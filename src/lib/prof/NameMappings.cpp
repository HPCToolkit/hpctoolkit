//******************************************************************************
//******************************************************************************
// NameMappings.cpp
//
// Purpose: 
//    To support renamings, this file supports one operation: normalize_name,
//    while maps input name into its output name. If no remapping exists, the
//    input name and the output name will agree.
//******************************************************************************
//******************************************************************************



//******************************************************************************
// global include files
//******************************************************************************

#include <iostream>
#include <map>
#include <string.h>

#include <lib/support/dictionary.h>


//******************************************************************************
// constants
//******************************************************************************

const char *PROGRAM_ROOT     = "<program root>";
const char *THREAD_ROOT      = "<thread root>";

const char *OMP_IDLE	     = "<omp idle>";
const char *OMP_OVERHEAD     = "<omp overhead>";
const char *OMP_BARRIER_WAIT = "<omp barrier>";
const char *OMP_TASK_WAIT    = "<omp task wait>"; 
const char *OMP_MUTEX_WAIT   = "<omp mutex wait>";
const char *OMP_REGION_UNR   = "<omp region unresolved>";

const char *OMP_TGT_ALLOC    = "<omp tgt alloc>";
const char *OMP_TGT_DELETE   = "<omp tgt delete>";
const char *OMP_TGT_COPYIN   = "<omp tgt copyin>";
const char *OMP_TGT_COPYOUT  = "<omp tgt copyout>";
const char *OMP_TGT_KERNEL   = "<omp tgt kernel>";

const char *GPU_COPY    = "<gpu copy>";
const char *GPU_COPYIN  = "<gpu copyin>";
const char *GPU_COPYOUT = "<gpu copyout>";
const char *GPU_ALLOC   = "<gpu alloc>";
const char *GPU_DELETE  = "<gpu delete>";
const char *GPU_MEMSET  = "<gpu memset>";
const char *GPU_SYNC    = "<gpu sync>";
const char *GPU_KERNEL  = "<gpu kernel>";

const char *NO_ACTIVITY = "<no activity>";
const char *PARTIAL_CALLPATH = "<partial call paths>";

const int  TYPE_NORMAL_PROC  = 0;  // nothing special. Default value
const int  TYPE_PLACEHOLDER  = 1;  // the proc is just a a place holder
const int  TYPE_ROOT         = 2;  // the proc is a root tyoe, shown in a separate view in hpcviewer
const int  TYPE_ELIDED       = 3;  // the proc shouldn't be shown in hpcviewer at all
const int  TYPE_TOPDOWN_PLACEHOLDER  = 4;  // special place holder for top-down tree only.

//******************************************************************************
// types
//******************************************************************************

class NameMapCompare {
public:
  bool operator()(const char *n1,  const char *n2) const {
    return strcmp(n1,n2) < 0;
  }
};

typedef struct {
  const char *in;
  const char *out;

/** to map from procedure name to its status  
 *  0: normal procedure
 *  1: place folder
 *  2: invisible 
 **/
  const int  type;
} NameMapping;


typedef std::map<const char *, NameMapping*, NameMapCompare> NameMappings_t;


//******************************************************************************
// private data
//******************************************************************************

static NameMapping renamingTable[] = { 
  { "monitor_main",            PROGRAM_ROOT,     TYPE_TOPDOWN_PLACEHOLDER },
  { "monitor_main_fence1",     PROGRAM_ROOT,     TYPE_TOPDOWN_PLACEHOLDER },
  { "monitor_main_fence2",     PROGRAM_ROOT,     TYPE_TOPDOWN_PLACEHOLDER },
  { "monitor_main_fence3",     PROGRAM_ROOT,     TYPE_TOPDOWN_PLACEHOLDER },
  { "monitor_main_fence4",     PROGRAM_ROOT,     TYPE_TOPDOWN_PLACEHOLDER },
 
  { "monitor_begin_thread",    THREAD_ROOT,      TYPE_TOPDOWN_PLACEHOLDER },
  { "monitor_thread_fence1",   THREAD_ROOT,      TYPE_TOPDOWN_PLACEHOLDER },
  { "monitor_thread_fence2",   THREAD_ROOT,      TYPE_TOPDOWN_PLACEHOLDER },
  { "monitor_thread_fence3",   THREAD_ROOT,      TYPE_TOPDOWN_PLACEHOLDER },
 
  { "monitor_thread_fence4",   THREAD_ROOT,      TYPE_TOPDOWN_PLACEHOLDER },

  { "ompt_idle_state",         OMP_IDLE        , TYPE_PLACEHOLDER   },
  { "ompt_overhead_state",     OMP_OVERHEAD    , TYPE_PLACEHOLDER   },
  { "ompt_barrier_wait_state", OMP_BARRIER_WAIT, TYPE_PLACEHOLDER   },
  { "ompt_task_wait_state",    OMP_TASK_WAIT   , TYPE_PLACEHOLDER   },
  { "ompt_mutex_wait_state",   OMP_MUTEX_WAIT  , TYPE_PLACEHOLDER   },

  { "ompt_tgt_alloc",          OMP_TGT_ALLOC  , TYPE_PLACEHOLDER    },
  { "ompt_tgt_copyin",         OMP_TGT_COPYIN , TYPE_PLACEHOLDER    },
  { "ompt_tgt_copyout",        OMP_TGT_COPYOUT, TYPE_PLACEHOLDER    },
  { "ompt_tgt_delete",         OMP_TGT_DELETE , TYPE_PLACEHOLDER    },
  { "ompt_tgt_alloc",          OMP_TGT_ALLOC  , TYPE_PLACEHOLDER    },
  { "ompt_tgt_kernel",         OMP_TGT_KERNEL , TYPE_PLACEHOLDER    },

  { "ompt_region_unresolved",  OMP_REGION_UNR,  TYPE_PLACEHOLDER    },

  { "gpu_op_copy",         GPU_COPY          ,  TYPE_PLACEHOLDER    },
  { "gpu_op_copyin",       GPU_COPYIN        ,  TYPE_PLACEHOLDER    },
  { "gpu_op_copyout",      GPU_COPYOUT       ,  TYPE_PLACEHOLDER    },
  { "gpu_op_alloc",        GPU_ALLOC         ,  TYPE_PLACEHOLDER    },
  { "gpu_op_delete",       GPU_DELETE        ,  TYPE_PLACEHOLDER    },
  { "gpu_op_memset",       GPU_MEMSET        ,  TYPE_PLACEHOLDER    },
  { "gpu_op_sync",         GPU_SYNC          ,  TYPE_PLACEHOLDER    },
  { "gpu_op_kernel",       GPU_KERNEL        ,  TYPE_PLACEHOLDER    },
  { "gpu_op_trace",        GPU_KERNEL        ,  TYPE_ELIDED         },

  { "hpcrun_no_activity",  NO_ACTIVITY       ,  TYPE_ELIDED         },
  { PARTIAL_CALLPATH,      PARTIAL_CALLPATH  ,  TYPE_PLACEHOLDER    }
};


static NameMappings_t renamingMap;

//******************************************************************************
// private operations
//******************************************************************************


static void 
normalize_name_load_renamings()
{
  static int initialized = 0;
  if (initialized) return;
  initialized = 1;

  for (unsigned int i=0; i < sizeof(renamingTable)/sizeof(NameMapping); i++) {
    renamingMap[renamingTable[i].in] =  &renamingTable[i];
  }
}


static const char *
normalize_name_rename(const char *in, int &type_procedure)
{
  // check if the name has to be changed or not
  const char *name_new = in;
  NameMappings_t::iterator it = renamingMap.find(in);
    
  if (it != renamingMap.end()) {
    // it has to be renamed.
    name_new = it->second->out;
    type_procedure = it->second->type;
  }

  return name_new;
}



//******************************************************************************
// interface operations
//******************************************************************************

const char *
normalize_name(const char *in, int &fake_procedure)
{
  normalize_name_load_renamings();
  return normalize_name_rename(in, fake_procedure);
}

#if 0

const char * tests[] = {
 "omp_idle_state", 
 "foo",
 "monitor_main_fence2",
 "omp_mutex_wait_state",
 "hpcrun_no_activity",
 "gpu_op_trace",
 "bar"
};

main()
{
  for(int i = 0; i < (sizeof(tests) / sizeof(const char *)); i++) {
    int type;
    const char *out = normalize_name(tests[i], type);
    std::cout << "input name = '" << tests[i] 
              << "', output name = '" <<  out
              << "', output type = '" <<  type
              << "'" << std::endl;
  }
}

#endif
