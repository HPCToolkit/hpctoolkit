#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>

#include "utils.h"

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

    sprintf(filename, "/proc/%d/maps", pid);
    fs = fopen(filename, "r");

    do {
        p = fgets(mapline, PATH_MAX+80, fs);

        if(p) {
            if(segment_is_executable(mapline)) {
                char *name = strdup(xname(mapline));
                unsigned long offset = htoll(mapline);
		unsigned long thesize = segment_size(mapline);

		MSG(1, "Load module %s loaded at %p",
		    name, (void *) offset);
            }
        }
    } while(p);
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
/* ** MSG free version of csprof_dump_loaded_modules ** */
void dump_load_mod(void){
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
		unsigned long thesize = segment_size(mapline);

		MSG(1, "Load module %s loaded at %p",
		    name, (void *) offset);
            }
        }
    } while(p);
}
extern void f_load_mod_(void){
  csprof_dump_loaded_modules();
}
