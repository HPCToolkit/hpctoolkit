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
const char *GPU_SYNC    = "<gpu sync>";
const char *GPU_KERNEL  = "<gpu kernel>";
const char *GPU_TRACE   = "<gpu trace>";

const char *NO_ACTIVITY = "<no activity>";
const char *PARTIAL_CALLPATH = "<partial call paths>";

const int  TYPE_NORMAL_PROC  = 0;  // nothing special. Default value
const int  TYPE_PLACE_FOLDER = 1;  // the proc is just a a place holder
const int  TYPE_ROOT         = 2;  // the proc is a root tyoe, shown in a separate view in hpcviewer
const int  TYPE_INVISIBLE    = 3;  // the proc shouldn't be shown in hpcviewer at all

//******************************************************************************
// types
//******************************************************************************

class NameMapCompare {
public:
  bool operator()(const char *n1,  const char *n2) const {
    return strcmp(n1,n2) < 0;
  }
};


typedef std::map<const char *, const char *, NameMapCompare> NameMappings_t;
typedef std::map<const char *, int, NameMapCompare>          TypeMappings_t;

typedef struct {
  const char *in;
  const char *out;
} NameMapping;

/** to map from procedure name to its status  
 *  0: normal procedure
 *  1: place folder
 *  2: invisible 
 **/
typedef struct {
  const char *name;
  const int   type;
} ProcedureMapping;

//******************************************************************************
// private data
//******************************************************************************

static NameMapping renamingTable[] = { 
  { "monitor_main",            PROGRAM_ROOT          },
  { "monitor_main_fence1",     PROGRAM_ROOT          },
  { "monitor_main_fence2",     PROGRAM_ROOT          },
  { "monitor_main_fence3",     PROGRAM_ROOT          },
  { "monitor_main_fence4",     PROGRAM_ROOT          },
 
  { "monitor_begin_thread",    THREAD_ROOT           },
  { "monitor_thread_fence1",   THREAD_ROOT           },
  { "monitor_thread_fence2",   THREAD_ROOT           },
  { "monitor_thread_fence3",   THREAD_ROOT           },
  { "monitor_thread_fence4",   THREAD_ROOT           },

  { "ompt_idle_state",         OMP_IDLE              },
  { "ompt_overhead_state",     OMP_OVERHEAD          },
  { "ompt_barrier_wait_state", OMP_BARRIER_WAIT	     },
  { "ompt_task_wait_state",    OMP_TASK_WAIT	       },
  { "ompt_mutex_wait_state",   OMP_MUTEX_WAIT	       },

  { "ompt_tgt_alloc",          OMP_TGT_ALLOC         },
  { "ompt_tgt_copyin",         OMP_TGT_COPYIN        },
  { "ompt_tgt_copyout",        OMP_TGT_COPYOUT       },
  { "ompt_tgt_delete",         OMP_TGT_DELETE        },
  { "ompt_tgt_alloc",          OMP_TGT_ALLOC         },
  { "ompt_tgt_kernel",         OMP_TGT_KERNEL        },

  { "ompt_region_unresolved",  OMP_REGION_UNR        },

  { "gpu_op_copy",         GPU_COPY              },
  { "gpu_op_copyin",       GPU_COPYIN            },
  { "gpu_op_copyout",      GPU_COPYOUT           },
  { "gpu_op_alloc",        GPU_ALLOC             },
  { "gpu_op_delete",       GPU_DELETE            },
  { "gpu_op_sync",         GPU_SYNC              },
  { "gpu_op_kernel",       GPU_KERNEL            },
  { "gpu_op_trace",        GPU_TRACE             },

  { "hpcrun_no_activity",  NO_ACTIVITY           }
};


static ProcedureMapping typeTable[] = {
  { PROGRAM_ROOT,       TYPE_PLACE_FOLDER }, 
  { THREAD_ROOT,        TYPE_PLACE_FOLDER }, 
  { GUARD_NAME,	        TYPE_PLACE_FOLDER }, 

  { NO_ACTIVITY,        TYPE_INVISIBLE    },
  { PARTIAL_CALLPATH,   TYPE_PLACE_FOLDER },

  { OMP_IDLE,           TYPE_PLACE_FOLDER }, 
  { OMP_OVERHEAD,       TYPE_PLACE_FOLDER }, 
  { OMP_BARRIER_WAIT,   TYPE_PLACE_FOLDER }, 
  { OMP_TASK_WAIT,      TYPE_PLACE_FOLDER }, 
  { OMP_MUTEX_WAIT,     TYPE_PLACE_FOLDER },

  { OMP_TGT_ALLOC,      TYPE_PLACE_FOLDER }, 
  { OMP_TGT_COPYIN,     TYPE_PLACE_FOLDER }, 
  { OMP_TGT_COPYOUT,    TYPE_PLACE_FOLDER }, 
  { OMP_TGT_DELETE,     TYPE_PLACE_FOLDER },
  { OMP_TGT_ALLOC,      TYPE_PLACE_FOLDER }, 
  { OMP_TGT_KERNEL,     TYPE_PLACE_FOLDER },

  { GPU_COPY,           TYPE_PLACE_FOLDER }, 
  { GPU_COPYIN,	        TYPE_PLACE_FOLDER }, 
  { GPU_COPYOUT,        TYPE_PLACE_FOLDER }, 
  { GPU_ALLOC,          TYPE_PLACE_FOLDER }, 
  { GPU_DELETE,	        TYPE_PLACE_FOLDER },
  { GPU_SYNC,           TYPE_PLACE_FOLDER }, 
  { GPU_KERNEL,	        TYPE_PLACE_FOLDER }, 
  { GPU_TRACE,          TYPE_INVISIBLE    }
};

static NameMappings_t renamingMap;
static TypeMappings_t typeProcedureMap;

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
    renamingMap[renamingTable[i].in] =  renamingTable[i].out;
  }
  for (unsigned int i=0; i < sizeof(typeTable)/sizeof(ProcedureMapping); i++) {
    typeProcedureMap[typeTable[i].name] = typeTable[i].type;
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
    name_new = it->second;
  }

  // here we want to find the type of the procedure 
  TypeMappings_t::iterator itp = typeProcedureMap.find(name_new);
  if (itp != typeProcedureMap.end()) {
    type_procedure = itp->second;
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
 "bar"
};

main()
{
  for(int i = 0; i < (sizeof(tests) / sizeof(const char *)); i++) {
    std::cout << "input name = '" << tests[i] 
              << "', output name = '" <<  normalize_name(tests[i]) 
              << "'" << std::endl;
  }
}

#endif
