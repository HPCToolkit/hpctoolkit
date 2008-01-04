/******************************************************************************
 * system includes
 *****************************************************************************/

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
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



/******************************************************************************
 * local types
 *****************************************************************************/

typedef struct dl_record_s {
  char *name;
  void *table;
  int length;
  struct dl_record_s *next;
} dl_record_t;



/******************************************************************************
 * forward declarations
 *****************************************************************************/

static dl_record_t *
dl_compute(const char *filename, void *baseaddr);

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
dl_add_module(const char *module_name, void *baseaddr)
{
  char real_module_name[PATH_MAX];
  realpath(module_name, real_module_name);
  dl_compute(real_module_name, baseaddr);
}


void 
dl_init()
{
#ifndef STATIC_ONLY
  struct csprof_epoch *e = csprof_get_epoch();
  struct csprof_epoch_module *m = e->loaded_modules;
  sprintf(mytmpdir,"%s/%d", tmproot, (int) getpid());
  if (!mkdir(mytmpdir, 0777)) {
    for(;m; m = m->next) dl_add_module(m->module_name, m->mapaddr);
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
  char command[PATH_MAX+1024];
  sprintf(command,"/bin/rm -rf %s\n", mytmpdir); 
  monitor_real_system(command); 
#endif
}


int 
find_dl_bound(const char *filename, void *baseaddr, void *pc, 
              void **start, void **end)
{
#ifndef STATIC_ONLY
  dl_record_t *r = dl_list;

  //--------------------------------------------------------------
  // post condition: filename is a canonical absolute path
  //--------------------------------------------------------------
  char real_filename[PATH_MAX];
  realpath(filename, real_filename);

  while (r && strcmp(r->name, real_filename) != 0) r = r->next; 
  if (!r) r = dl_compute(real_filename, baseaddr);
  
  if (r && r->length > 0) 
    return nm_tab_bound(r->table, r->length, (unsigned long) pc, start, end);
#endif
  return 1;
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
new_dlentry(const char *name, void * table, int length) 
{
  dl_record_t *r = (dl_record_t *) malloc(sizeof(dl_record_t));
  r->next = dl_list;
  r->name = strdup(name);
  r->table = table;
  r->length = length;
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
dl_compute(const char *filename, void *baseaddr)
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
      void *dlhandle = monitor_real_dlopen(dlname, RTLD_LAZY);
      void *nm_table = dlsym(dlhandle, "csprof_nm_addrs");
      int *nm_table_len = (int *) dlsym(dlhandle, "csprof_nm_addrs_len");
      int *relocatable = (int *) dlsym(dlhandle, "csprof_relocatable");
      int *stripped = (int *) dlsym(dlhandle, "csprof_stripped");
      if (stripped && *stripped == 0 && nm_table_len && relocatable) {
	// file is not stripped. the number of symbols and relocation 
	// information are available. in this case, we can consider using
	// symbols values available in the table.
	nsymbols = *nm_table_len;
	if (*relocatable == 1) {
	  relocate_symbols(nm_table, nsymbols, (unsigned long) baseaddr);

	  //------------------------------------------------------------
	  // unset relocatable to prevent us from relocating the symbols 
	  // twice in case we unknowingly map them again.
	  //------------------------------------------------------------
	  *relocatable = 0; 
	}
      }
      dl_record_t *r = new_dlentry(filename, nm_table, nsymbols);
      return r;
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
