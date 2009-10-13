#include <assert.h>
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define PATH_MAX 2048

static unsigned long
htoll(char *s)
{
    unsigned long res = 0;
    sscanf(s, "%lx", &res);
    return res;
}

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

static void
dump_loaded_modules(void){
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
      printf("%s",p);
      if(segment_is_executable(mapline)) {
	unsigned long long seg_start, seg_end;
	char *seg_name;

	// seg_name = xname(mapline);
	segment_start_end(mapline, &seg_start, &seg_end);
	char *name = strdup(xname(mapline));
	unsigned long offset = htoll(mapline);

	printf("--Load module %s: start = %p, end = %p\n",name, (char *)seg_start, (char *)seg_end);
      }
    }
  } while(p);
}

int main(int argc, char *argv[])
{
  void *handle = dlopen("libm.so", RTLD_NOW);
  dump_loaded_modules();
  dlclose(handle);
  
  return 0;
}
