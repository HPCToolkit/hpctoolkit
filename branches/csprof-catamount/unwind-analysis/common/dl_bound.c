/******************************************************************************
 * system includes
 *****************************************************************************/

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <dlfcn.h>



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



/******************************************************************************
 * local types
 *****************************************************************************/

typedef struct dl_record_s {
  char *name;
  void **table;
  int length;

  void *start_addr;
  void *end_addr;
  int relocate;

  struct dl_record_s *next;
} dl_record_t;

#define PERFORM_RELOCATION(addr, base) \
	((void *) (((unsigned long) addr) + ((unsigned long) base)))

#define MAPPING_END(addr, length) \
	((void *) (((unsigned long) addr) + ((unsigned long) length)))


/******************************************************************************
 * forward declarations
 *****************************************************************************/

static dl_record_t *
dl_compute(const char *filename, void *start, void *end);

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

void 
dl_add_module_base(const char *module_name, void *start, void *end)
{
  char real_module_name[PATH_MAX];
  realpath(module_name, real_module_name);
  dl_compute(real_module_name, start, end);
}


void 
dl_add_module(const char *module)
{
  struct csprof_epoch *e = csprof_get_epoch();
  struct csprof_epoch_module *m = e->loaded_modules;
  //--------------------------------------------------------------
  // post condition: filename is a canonical absolute path
  //--------------------------------------------------------------
  char real_module[PATH_MAX];
  realpath(module, real_module);

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



void dump_dl_record(dl_record_t *r)
{
  printf("record addr = %p: name = '%s', table = %p, length = 0x%x, "
	 "start = %p, end = %p, relocate = %d\n",
         r, r->name, r->table, r->length, r->start_addr, 
	 r->end_addr, r->relocate);
}

void dump_dl_list()
{
  dl_record_t *r = dl_list;
  for (; r; r = r->next) dump_dl_record(r);
}
	

void 
dl_fini()
{
#ifndef STATIC_ONLY
  unlink_tree(mytmpdir);
#if 0
  char command[PATH_MAX+1024];
  sprintf(command,"/bin/rm -rf %s\n", mytmpdir); 
  monitor_real_system(command); 
#endif
#endif
}


int 
find_dl_bound(const char *filename, void *baseaddr, void *pc, 
              void **start, void **end)
{
#ifndef STATIC_ONLY
  dl_record_t *r = dl_list;
  int ret = 1;

  //--------------------------------------------------------------
  // post condition: filename is a canonical absolute path
  //--------------------------------------------------------------
  char real_filename[PATH_MAX];
  realpath(filename, real_filename);

  while (r && (strcmp(r->name, real_filename) != 0 
	       || pc < r->start_addr || pc > r->end_addr)) r = r->next; 
#if 0
  if (!r) r = dl_compute(real_filename, baseaddr);
#endif
  
  if (r && r->length > 0) { 
    unsigned long rpc = (unsigned long) pc;
    if (r->relocate) rpc -=  (unsigned long) r->start_addr; 
    ret =  nm_tab_bound(r->table, r->length, rpc, start, end);
    if (ret == 0 && r->relocate) {
	*start = PERFORM_RELOCATION(*start, r->start_addr);
	*end   = PERFORM_RELOCATION(*end  , r->start_addr);
    }
  }
#endif
  return ret;
}



/******************************************************************************
 * private operations
 *****************************************************************************/

static void 
relocate_symbols(unsigned long *table, int nsymbols, unsigned long baseaddr)
{
  int i;
  for (i = 0; i < nsymbols; i++) {
    table[i] += baseaddr;
  }
}


static dl_record_t *
new_dl_record(const char *name, void **table, int length, int relocate, void *startaddr, void *endaddr) 
{
  dl_record_t *r = (dl_record_t *) malloc(sizeof(dl_record_t));
  r->next = dl_list;
  r->name = strdup(name);
  r->table = table;
  r->length = length;

  r->relocate = relocate;
  r->start_addr = startaddr;
  r->end_addr = endaddr;

  dl_list = r;
  return r;
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
    sprintf(dlname, "%s/%s.nm.so", mytmpdir, mybasename(filename));
    sprintf(command, "%s %s %s\n", nm_command, filename, mytmpdir);
    {
      monitor_real_system(command); 
      int nsymbols = 0; // default is that there are no symbols
      int relocate = 0; // default is no relocation
      int addmapping = 0; // default is omit
      void *dlhandle = monitor_real_dlopen(dlname, RTLD_LAZY);
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
#if 0
	if (*relocatable == 1) {
	  relocate_symbols(nm_table, nsymbols, (unsigned long) start);

	  //------------------------------------------------------------
	  // unset relocatable to prevent us from relocating the symbols 
	  // twice in case we unknowingly map them again.
	  //------------------------------------------------------------
	  *relocatable = 0; 
	}
#else
	if (nsymbols >= 1) {
	  if (nm_table[0] >= start && nm_table[0] <= end)
	    relocate = 0; // segment loaded at its preferred address

#if 0
	  // the mapping only makes sense if the length of the mapping
	  // is at least as long as the code size
	  if (nm_table[nsymbols-1] - nm_table[0] <= length) 
#endif
          addmapping = 1;
	}
#endif
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
