/******************************************************************************
 * system includes
 *****************************************************************************/

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <dlfcn.h>
#include <unistd.h>

/******************************************************************************
 * local includes
 *****************************************************************************/

/*---------------------------
 * monitor
 *--------------------------*/
#include "monitor.h"

/*---------------------------
 * csprof
 *--------------------------*/
#include "nm_bound.h"
#include "epoch.h"
#include "structs.h"
#include "pmsg.h"
#include "dl_bound.h"
#include "unlink.h"
#include "executable-path.h"
#include "loadmod.h"

/******************************************************************************
 * local types
 *****************************************************************************/

typedef struct dl_record_s {
  char *name;
  void *start_addr;
  void *end_addr;
  int relocate;

  void **table;
  int nsymbols;

  struct dl_record_s *next;
} dl_record_t;

#define PERFORM_RELOCATION(addr, base) \
	((void *) (((unsigned long) addr) + ((unsigned long) base)))

#define MAPPING_END(addr, length) \
	((void *) (((unsigned long) addr) + ((unsigned long) length)))


/******************************************************************************
 * forward declarations
 *****************************************************************************/

static dl_record_t *dl_compute(const char *filename, void *start, void *end);

void dl_add_module_base(const char *module_name, void *start, void *end);

// FIXME: this should come from monitor
static int
monitor_real_system(const char *command);



/******************************************************************************
 * local variables
 *****************************************************************************/

// FIXME: tmproot should be overridable with an option.
static char *tmproot = "/tmp";

static char mytmpdir[PATH_MAX];
static dl_record_t *dl_list;



/******************************************************************************
 * interface operations
 *****************************************************************************/

#if 0
void 
dl_add_module(const char *module)
{
  struct csprof_epoch *e = csprof_get_epoch();
  struct csprof_epoch_module *m = e->loaded_modules;
  //--------------------------------------------------------------
  // post condition: filename is a canonical absolute path
  //--------------------------------------------------------------
  char real_module[PATH_MAX];
  const char *ld_library_path = getenv("LD_LIBRARY_PATH");
  if (executable_path(module, ld_library_path, real_module) == NULL) {
    EMSG("unable to locate object file '%s' loaded with dlopen", module);
    return;
  }

  for(;m; m = m->next) {
    if (strcmp(m->module_name, real_module) == 0) {
      const char *module_name = m->module_name;
      void *start = m->mapaddr;
      void *end = MAPPING_END(start, m->size);
      while (m->next && strcmp(m->next->module_name, module_name) == 0) {
	m = m->next;
	if (m->mapaddr < start) {
	  start = m->mapaddr;
	}
	if (m->mapaddr > start) {
	  void *nend = MAPPING_END(m->mapaddr, m->size);
	  if (nend > end) end = nend;
	}
      }
      dl_add_module_base(module_name, start, end);
      break;
    }
  }
}
#endif

void 
dl_init()
{
#ifndef STATIC_ONLY
  struct csprof_epoch *e = csprof_get_epoch();
  struct csprof_epoch_module *m = e->loaded_modules;
  sprintf(mytmpdir,"%s/%d", tmproot, (int) getpid());
  if (!mkdir(mytmpdir, 0777)) {
    for(;m; m = m->next) {
      const char *module_name = m->module_name;
      void *start = m->mapaddr;
      void *end = MAPPING_END(start, m->size);
      while (m->next && strcmp(m->next->module_name, module_name) == 0) {
	m = m->next;
	if (m->mapaddr < start) {
	  start = m->mapaddr;
	}
	if (m->mapaddr > start) {
	  void *nend = MAPPING_END(m->mapaddr, m->size);
	  if (nend > end) end = nend;
	}
      }
      dl_add_module_base(module_name, start, end);
    }
  } else {
    EMSG("fatal error: unable to make temporary directory %s\n", mytmpdir);
    exit(-1);
  }
#endif
}


void 
dl_fini()
{
#ifndef STATIC_ONLY
  IF_NOT_DISABLED(DL_BOUND_UNLINK){
    unlink_tree(mytmpdir);
  }
#endif
}


int 
find_dl_bound(void *pc, void **start, void **end)
{
#ifndef STATIC_ONLY
  dl_record_t *r = dl_list;
  int ret = 1;

  while (r && (pc < r->start_addr || pc > r->end_addr)) r = r->next; 
  
  if (!r) {
    char module_name[PATH_MAX];
    unsigned long long addr, mstart, mend;
    addr = (unsigned long long) pc;
    if (find_load_module(addr, module_name, &mstart, &mend)) {
      r = dl_compute(module_name, (void *) mstart, (void *) mend);
    }
  }
  if (r && r->nsymbols > 0) { 
    unsigned long rpc = (unsigned long) pc;
    if (r->relocate) rpc -=  (unsigned long) r->start_addr; 
    ret =  nm_tab_bound(r->table, r->nsymbols, rpc, start, end);
    if (ret == 0 && r->relocate) {
      *start = PERFORM_RELOCATION(*start, r->start_addr);
      *end   = PERFORM_RELOCATION(*end  , r->start_addr);
    }
  }
#if 0
  /* debugging output */
  printf("find_dl_bound: pc = %p found=%d start = %p end = %p\n", pc, !ret, (!ret) ? *start : 0, 
	(!ret) ? *end : 0);
#endif
#endif
  return ret;
}


char *csprof_strdup(char *str)
{
	int n = strlen(str);
	void *mem = csprof_malloc(n+1);
        char *result = strcpy(mem, str);
	return result;
}

/******************************************************************************
 * private operations
 *****************************************************************************/

static dl_record_t *
new_dl_record(const char *name, void **table, int nsymbols, int relocate, void *startaddr, void *endaddr) 
{
  dl_record_t *r = (dl_record_t *) csprof_malloc(sizeof(dl_record_t));
  r->next = dl_list;
  r->name = csprof_strdup(name);
  r->table = table;
  r->nsymbols = nsymbols;

  r->relocate = relocate;
  r->start_addr = startaddr;
  r->end_addr = endaddr;

  dl_list = r;
  return r;
}


void 
dl_add_module_base(const char *module_name, void *start, void *end)
{
  dl_compute(module_name, start, end);
}

 
static const char *
mybasename(const char *string)
{
  char *suffix = rindex(string, '/');
  if (suffix) return suffix + 1;
  else return string;
}


static dl_record_t *
dl_compute(const char *filename, void *start, void *end)
{
#ifndef STATIC_ONLY
  char *nm_command = getenv("CSPROF_NM_COMMAND"); 
  if (nm_command) {
    char command[MAXPATHLEN+1024];
    char dlname[MAXPATHLEN];
    char redir[64] = {'\0'};

    IF_NOT_DISABLED(DL_BOUND_REDIR) {
      int logfile_fd = csprof_logfile_fd();
      sprintf(redir,"1>&%d 2>&%d",logfile_fd,logfile_fd);
    }
    sprintf(dlname, "%s/%s.nm.so", mytmpdir, mybasename(filename));
    char *script_debug = "";
    IF_ENABLED(DL_BOUND_SCRIPT_DEBUG) {
      script_debug = "DBG";
    }
    sprintf(command, "%s %s %s %s %s\n", nm_command, filename, mytmpdir, script_debug, redir);
    TMSG(DL_BOUND,"system command = %s",command);
    {
      monitor_real_system(command); 
      int nsymbols = 0; // default is that there are no symbols
      int relocate = 0; // default is no relocation
      int addmapping = 0; // default is omit
      void *dlhandle = monitor_real_dlopen(dlname, RTLD_LAZY);
      int *bogus = dlsym(dlhandle,"BOGUS");
      if (bogus){
	EMSG("dl_compute for file %s produced bogus symbol table",filename);
	abort();
      }
      void **nm_table = dlsym(dlhandle, "csprof_nm_addrs");
      int *nm_table_len = (int *) dlsym(dlhandle, "csprof_nm_addrs_len");
      int *relocatable = (int *) dlsym(dlhandle, "csprof_relocatable");
      int *stripped = (int *) dlsym(dlhandle, "csprof_stripped");
      if (stripped && *stripped == 0 && nm_table_len && relocatable) {
	// file is not stripped. the number of symbols and relocation 
	// information are available. in this case, we can consider using
	// symbols values available in the table.
	nsymbols = *nm_table_len;
        relocate = *relocatable;
	if (nsymbols >= 1) {
	  if (nm_table[0] >= start && nm_table[0] <= end)
	    relocate = 0; // segment loaded at its preferred address
          addmapping = 1;
	}
      }
      if (addmapping) {
         dl_record_t *r = new_dl_record(filename, nm_table, nsymbols, relocate, start, end);
         return r;
      }
    }
  }
#endif
  return 0;
}


// FIXME: this should be in monitor and it should not have the side
// effect of unsetting LD_PRELOAD
static int
monitor_real_system(const char *command)
{
  unsetenv("LD_PRELOAD");
  return system(command);
}

/******************************************************************************
 * debugging support
 *****************************************************************************/

void 
dump_dl_record(dl_record_t *r)
{
  printf("record addr = %p: name = '%s', table = %p, nsymbols = 0x%x, "
	 "start = %p, end = %p, relocate = %d\n",
         r, r->name, r->table, r->nsymbols, r->start_addr, 
	 r->end_addr, r->relocate);
}

void 
dump_dl_list()
{
  dl_record_t *r = dl_list;
  for (; r; r = r->next) dump_dl_record(r);
}
	

