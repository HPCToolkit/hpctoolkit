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

/* MWF added utility f dumping w.o. modifying epoch stuff */
void
csprof_dump_loaded_modules(void){
    char filename[PATH_MAX];
    char mapline[PATH_MAX+80];
    char *p;
    FILE *fs;
    pid_t pid = getpid();

#ifndef STATIC_ONLY
    sprintf(filename, "/proc/%d/maps", pid);
    fs = fopen(filename, "r");

    do {
        p = fgets(mapline, PATH_MAX+80, fs);

        if(p) {
            if(segment_is_executable(mapline)) {
                char *name = strdup(xname(mapline));
                unsigned long offset = htoll(mapline);
		unsigned long thesize = segment_size(mapline);

		MSG(CSPROF_MSG_EPOCH, "Load module %s loaded at %p",
		    name, (void *) offset);
            }
        }
    } while(p);
#else
    MSG(1,"STATIC ONLY: dump loaded modules = NOOP");
#endif
}

void
csprof_epoch_get_loaded_modules(csprof_epoch_t *epoch,
                                csprof_epoch_t *previous_epoch)
{
    char filename[PATH_MAX];
    char mapline[PATH_MAX+80];
    char *p;
    FILE *fs;
    pid_t pid = getpid();

#ifndef STATIC_ONLY
    sprintf(filename, "/proc/%d/maps", pid);
    fs = fopen(filename, "r");

    do {
        p = fgets(mapline, PATH_MAX+80, fs);

        if(p) {
            if(segment_is_executable(mapline)) {
                char *name = strdup(xname(mapline));
                unsigned long offset = htoll(mapline);
                /** HACK: /proc is different, no size, but end address **/
		unsigned long thesize = segment_size(mapline);
                csprof_epoch_module_t *newmod;

                newmod = csprof_malloc(sizeof(csprof_epoch_module_t));
                newmod->module_name = name;
                newmod->mapaddr = (void *)offset;
                newmod->vaddr = NULL;
		newmod->size = thesize;

		MSG(CSPROF_MSG_EPOCH, "Load module %s loaded at %p",
		    name, (void *) offset);
                newmod->next = epoch->loaded_modules;
                epoch->loaded_modules = newmod;
                epoch->num_modules++;
            }
        }
    } while(p);
#else
    MSG(1,"STATIC_ONLY: epoch loads no modules");
    MSG(1,"STATIC_ONLY: build fake initial epoch");

    csprof_epoch_module_t *newmod;

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

static unsigned long
segment_size(char *line)
{
  unsigned long long start = htoll(line);
  char *dash = index(line, (int) '-');
  unsigned long long end = htoll(dash+1);

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
