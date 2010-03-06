// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2010, Rice University 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// 
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// 
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
// 
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage. 
// 
// ******************************************************* EndRiceCopyright *

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
