#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>

#include "general.h"
#include "interface.h"
#include "epoch.h"
#include "mem.h"
#include "name.h"

#ifdef STATIC_ONLY
extern void *static_epoch_offset;
extern long static_epoch_size;
#endif

static int segment_is_executable(char *);
static char *xname(char *);
static unsigned long htoll(char *);
static unsigned long segment_size(char *);
static void segment_start_end(char *line, unsigned long long *start, unsigned long long *end);

/* MWF added utility f dumping w.o. modifying epoch stuff */
void
csprof_dump_loaded_modules(void){
#ifndef STATIC_ONLY
  char filename[PATH_MAX];
  char mapline[PATH_MAX+80];
  char *p;
  FILE *fs; 
  pid_t pid = getpid();

  sprintf(filename, "/proc/%d/maps", pid);
  fs = fopen(filename, "r");

  do {
    p = fgets(mapline, PATH_MAX+80, fs);

    if(p) {
      if(segment_is_executable(mapline)) {
	char *name = strdup(xname(mapline));
	unsigned long offset = htoll(mapline);

	MSG(CSPROF_MSG_EPOCH, "Load module %s loaded at %p",
	    name, (void *) offset);
      }
    }
  } while(p);
#else
  MSG(1,"STATIC ONLY: dump loaded modules = NOOP");
#endif
}


int
find_load_module(unsigned long long addr, 
		 char *module_name,
		 unsigned long long *start, 
		 unsigned long long *end)
{
#ifndef STATIC_ONLY
    char filename[PATH_MAX];
    char mapline[PATH_MAX+80];
    char *p;
    FILE *fs;
    pid_t pid = getpid();
    unsigned long long module_start, module_end;

    sprintf(filename, "/proc/%d/maps", pid);
    fs = fopen(filename, "r");

    
    module_name[0] = 0;
    module_start   = ~(0ULL);
    module_end     = 0;
    do {
        p = fgets(mapline, PATH_MAX+80, fs);

        if(p) {
            if(segment_is_executable(mapline)) {
	      unsigned long long seg_start, seg_end;
	      char *seg_name;

	      seg_name = xname(mapline);
	      segment_start_end(mapline, &seg_start, &seg_end);

	      if (strcmp(module_name, seg_name) != 0) {
		/*--------------------------------------
		 * at end of one module and beginning a
		 * new module. test addr for inclusion in 
		 * old module before beginning new one.
		 *------------------------------------*/
		if (module_start <= addr  && addr < module_end) {
		  *start = module_start;
		  *end = module_end;
		  fclose(fs);
		  return 1;
		}
		strcpy(module_name, xname(mapline));
		module_start = seg_start;
		module_end   = seg_end;
	      } else {
		if (seg_start < module_start) module_start = seg_start;
		if (seg_end   > module_end)   module_end   = seg_end;
	      }
            }
        }
    } while(p);
    /*--------------------------------------
     * test for inclusion in final module
     *------------------------------------*/
    if (module_start <= addr  && addr < module_end) {
      *start = module_start;
      *end = module_end;
      fclose(fs);
      return 1;
    }
    return 0;
#endif
}

void
csprof_epoch_get_loaded_modules(csprof_epoch_t *epoch,
                                csprof_epoch_t *previous_epoch)
{
#ifndef STATIC_ONLY
    char filename[PATH_MAX];
    char mapline[PATH_MAX+80];
    char *p;
    FILE *fs;
    pid_t pid = getpid();

    sprintf(filename, "/proc/%d/maps", pid);
    fs = fopen(filename, "r");

    do {
        p = fgets(mapline, PATH_MAX+80, fs);

        if(p) {
            if(segment_is_executable(mapline)) {
                char *segname = xname(mapline);
		int segname_len = strlen(segname);
                unsigned long offset = htoll(mapline);
                /** HACK: /proc is different, no size, but end address **/
		unsigned long thesize = segment_size(mapline);
                csprof_epoch_module_t *newmod;

                newmod = csprof_malloc(sizeof(csprof_epoch_module_t));
                newmod->module_name = 
		  strcpy(csprof_malloc(segname_len + 1), segname);
                newmod->mapaddr = (void *)offset;
                newmod->vaddr = NULL;
		newmod->size = thesize;

		MSG(CSPROF_MSG_EPOCH, "Load module %s loaded at %p",
		    newmod->module_name, (void *) offset);
                newmod->next = epoch->loaded_modules;
                epoch->loaded_modules = newmod;
                epoch->num_modules++;
            }
        }
    } while(p);
#else
    csprof_epoch_module_t *newmod;

    MSG(1,"STATIC_ONLY: epoch loads no modules");
    MSG(1,"STATIC_ONLY: build fake initial epoch");


    newmod = csprof_malloc(sizeof(csprof_epoch_module_t));
    newmod->module_name = csprof_get_executable_name();
    newmod->mapaddr = static_epoch_offset;
    newmod->vaddr = NULL;
    newmod->size = static_epoch_size;

    newmod->next = epoch->loaded_modules;
    epoch->loaded_modules = newmod;
    epoch->num_modules++;

    MSG(1,"newmod thing: name = %s\n"
          "         mapaddr = %p\n"
        "         size    = %lx", csprof_get_executable_name(),
        static_epoch_offset,
        static_epoch_size);

#endif
}

/* support procedures */

static int
segment_is_executable(char *line)
{
    char *blank = index(line, (int) ' ');
    char *slash = index(line, (int) '/');

    return (blank && *(blank + 3) == 'x' && slash);
}


static void 
segment_start_end(char *line, unsigned long long *start, unsigned long long *end)
{
  char *dash;

  *start = htoll(line);

  dash = index(line, (int) '-');

  *end = htoll(dash+1);
}


static unsigned long
segment_size(char *line)
{
  unsigned long long start, end;

  segment_start_end(line, &start, &end);

  return end - start;
}


static char *
xname(char *line)
{
    char *name = index(line, (int) '/');
    char *newline = index(line, (int) '\n');

    if(newline) {
        *newline = 0;
    }

    return name;
}

static unsigned long
htoll(char *s)
{
    unsigned long res = 0;
    sscanf(s, "%lx", &res);
    return res;
}
