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
const char *OMP_TGT_COPYIN   = "<omp tgt copyin>";
const char *OMP_TGT_COPYOUT  = "<omp tgt copyout>";
const char *OMP_TGT_DELETE   = "<omp tgt delete>";
const char *OMP_TGT_KERNEL   = "<omp tgt kernel>";

const char *CUDA_MEM_ALLOC   = "<cuda alloc>";
const char *CUDA_MEMCPY      = "<cuda copy>";
const char *CUDA_KERNEL      = "<cuda kernel>";
const char *CUDA_SYNC        = "<cuda sync>";

const char *NO_THREAD_ROOT   = "<no thread>";


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


typedef struct {
  const char *in;
  const char *out;
} NameMapping;


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
  { "ompt_task_wait_state",    OMP_TASK_WAIT	     },
  { "ompt_mutex_wait_state",   OMP_MUTEX_WAIT	     },

  { "ompt_tgt_alloc",          OMP_TGT_ALLOC         },
  { "ompt_tgt_copyin",         OMP_TGT_COPYIN        },
  { "ompt_tgt_copyout",        OMP_TGT_COPYOUT       },
  { "ompt_tgt_delete",         OMP_TGT_DELETE        },
  { "ompt_tgt_kernel",         OMP_TGT_KERNEL        },

  { "ompt_region_unresolved",  OMP_REGION_UNR        },

  { "cuda_memcpy",             CUDA_MEMCPY           },
  { "cuda_sync",               CUDA_SYNC             },
  { "cuda_mem_alloc",          CUDA_MEM_ALLOC        },
  { "cuda_kernel",             CUDA_KERNEL           },

  { "NO_THREAD",               NO_THREAD_ROOT        }
};

static const char *fakeProcedures[] = {
  PROGRAM_ROOT, THREAD_ROOT, GUARD_NAME, "<partial call paths>"
};

static NameMappings_t renamingMap;
static NameMappings_t fakeProcedureMap;


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
  for (unsigned int i=0; i < sizeof(fakeProcedures)/sizeof(const char*); i++) {
    fakeProcedureMap[fakeProcedures[i]] = "f";
  }
}


static const char *
normalize_name_rename(const char *in, bool &fake_procedure)
{
  // check if the name has to be changed or not
  const char *name_new = in;
  NameMappings_t::iterator it = renamingMap.find(in);
    
  if (it != renamingMap.end()) {
    // it has to be renamed.
    name_new = it->second;
  }
  // here we want to mark fake procedures if they match with the list from fakeProcedureMap
  it = fakeProcedureMap.find(name_new);
  fake_procedure = (it != fakeProcedureMap.end());

  return name_new;
}



//******************************************************************************
// interface operations
//******************************************************************************

const char *
normalize_name(const char *in, bool &fake_procedure)
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
