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
  { "monitor_main",           "<program root>"     },
  { "monitor_main_fence1",    "<program root>"     },
  { "monitor_main_fence2",    "<program root>"     },
  { "monitor_main_fence3",    "<program root>"     },
  { "monitor_main_fence4",    "<program root>"     },

  { "monitor_begin_thread",   "<thread root>"      },
  { "monitor_thread_fence1",  "<thread root>"      },
  { "monitor_thread_fence2",  "<thread root>"      },
  { "monitor_thread_fence3",  "<thread root>"      },
  { "monitor_thread_fence4",  "<thread root>"      },

  { "ompt_idle_state",         "<omp idle>"         },
  { "ompt_idle",               "<omp idle>"         },

  { "ompt_overhead_state",     "<omp overhead>"     },
  { "omp_overhead",           "<omp overhead>"     },

  { "ompt_barrier_wait_state", "<omp barrier wait>" },
  { "ompt_barrier_wait",       "<omp barrier wait>" },

  { "ompt_task_wait_state",    "<omp task wait>"    },
  { "ompt_task_wait",          "<omp task wait>"    },

  { "ompt_mutex_wait_state",   "<omp mutex wait>"   },
  { "ompt_mutex_wait",         "<omp mutex wait>"   }

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

  for (unsigned int i = 0; i < sizeof(renamingTable) / sizeof(NameMapping); i++) {
    renamingMap[renamingTable[i].in] =  renamingTable[i].out;
  }
}


static const char *
normalize_name_rename(const char *in)
{
  NameMappings_t::iterator it = renamingMap.find(in);
  if (it == renamingMap.end()) return in;
  else return it->second;
}



//******************************************************************************
// interface operations
//******************************************************************************

const char *
normalize_name(const char *in)
{
  normalize_name_load_renamings();
  return normalize_name_rename(in);
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
