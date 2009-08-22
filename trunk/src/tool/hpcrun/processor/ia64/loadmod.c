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
// Copyright ((c)) 2002-2009, Rice University 
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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>

#include "general.h"
#include "interface.h"
#include "epoch.h"
#include "mem.h"

static int segment_is_executable(char *);
static char *xname(char *);
static unsigned long htoll(char *);

void
csprof_epoch_get_loaded_modules(csprof_epoch_t *epoch,
                                csprof_epoch_t *previous_epoch)
{
    char filename[PATH_MAX];
    char mapline[PATH_MAX+80];
    char *p;
    FILE *fs;
    pid_t pid = getpid();

    sprintf(filename, "/proc/%d/maps", pid);
    fs = fopen(filename, "r");

    if (fs) {
        do {
            p = fgets(mapline, PATH_MAX+80, fs);

            if(p) {
                if(segment_is_executable(mapline)) {
                    char *name = strdup(xname(mapline));
                    unsigned long offset = htoll(mapline);
                    csprof_epoch_module_t *newmod;

                    newmod = csprof_malloc(sizeof(csprof_epoch_module_t));
                    newmod->module_name = name;
                    newmod->mapaddr = (void *)offset;
                    newmod->vaddr = NULL;

                    MSG(CSPROF_MSG_EPOCH, "Load module %s loaded at %p",
                        name, (void *) offset);
                    newmod->next = epoch->loaded_modules;
                    epoch->loaded_modules = newmod;
                    epoch->num_modules++;
                }
            }
        } while(p);

        fclose(fs);
    }
    else {
        perror("could not open file");
    }
}


/* support procedures */

static int
segment_is_executable(char *line)
{
    char *blank = index(line, (int) ' ');
    char *slash = index(line, (int) '/');

    return (blank && *(blank + 3) == 'x' && slash);
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
