// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2016, Rice University
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

/****************************************************************************
//
// File: 
//    $HeadURL$
//
// Purpose:
//    Finds a list of loaded modules (e.g. DSOs) for the current process.
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
// Author:
//    Written by John Mellor-Crummey and Nathan Tallent, Rice University.
//
*****************************************************************************/

/************************** System Include Files ****************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <limits.h> /* for 'PATH_MAX' */
#include <libgen.h> /* for dirname/basename */
#include <sys/types.h>

/**************************** User Include Files ****************************/

#include "rtmap.h"

/**************************** Forward Declarations **************************/

#define TESTING 0

static rtloadmap_t rtloadmap;

static void finalizelines(void);
static int iscodeline(char *line);
static char* get_line_slot(void);

static char* get_name(char *line);
static char* get_beg_addr(char *line);
static char* get_end_addr(char *line);

static unsigned long hex2ul(char *s);
static uint64_t      hex2u64(char *s);

static void dumprtloadmap(void);
static void dumplines(void);

static void reset_slots();

/****************************************************************************/

/* number of bytes to print out for a long in hex with a 0x prefix */ 
#define bhl ((int)(sizeof(long)*2+2))

#define MAXLINELEN (PATH_MAX + 80)

/****************************************************************************/

/*
 *  Read lines from the load maps in /proc to find runtime libraries
 *  and load addresses.  Cf. "man proc".  E.g.:
 *
 *      address           perms offset  dev   inode      pathname
 *      08048000-08056000 r-xp 00000000 03:0c 64593      /usr/sbin/gpm
 *      08056000-08058000 rw-p 0000d000 03:0c 64593      /usr/sbin/gpm
 *  
 */
rtloadmap_t* 
hpcrun_get_rtloadmap(int dbglvl)
{
  char filename[PATH_MAX];
  FILE *fs;
  char *line;
  pid_t pid = getpid();

  reset_slots();
  line = get_line_slot(); 

  sprintf(filename, "/proc/%d/maps", pid);
  fs = fopen(filename, "r");

  /* read lines from the maps file in /proc for this
   * process. discard the line if it does not correspond
   * to an executable segment
   */
  do {
    if (iscodeline(line)) {
      line = get_line_slot(); 
    }
  } while (fgets(line, MAXLINELEN, fs) != NULL);
  fclose(fs);

  finalizelines();

  if (dbglvl >= 3) { 
    dumprtloadmap(); 
  }
  if (dbglvl >= 4) {
    dumplines();
  }

  return &rtloadmap;
}


/* 
 * Return the name of the command invoked to create this process.  
 *
 */
const char*
hpcrun_get_cmd(int dbglvl)
{
  static char* cmd = NULL; /* */
  
  char filenm[PATH_MAX];
  char cmdline[PATH_MAX];
  FILE *fs;
  pid_t pid = getpid();
  char* space, *basenm;
  
  free(cmd);
  
  sprintf(filenm, "/proc/%d/cmdline", pid);
  fs = fopen(filenm, "r");
  fgets(cmdline, PATH_MAX, fs);
  fclose(fs);

  /* Separate the command's path from its arguments.  Assume for now
     that there are no spaces in path name. */
  space = strchr(cmdline, ' '); /* assume there is no  */
  if (space) {
    *space = '\0';
  }
  
  /* Find the basename of the path*/
  basenm = basename(cmdline);
  cmd = malloc(sizeof(const char*) * (strlen(basenm) + 1));
  strcpy(cmd, basenm);
  
  return cmd;
}

/****************************************************************************/

static char **mappings = 0;
static int slots_avail = 0;
static int slots_in_use = 0;

static void reset_slots()
{
  slots_avail = 0;
  slots_in_use = 0;
  if (mappings) free(mappings);
  mappings = 0;
}

/* allocate a slot for a new line in an 
 * extensible array of load map lines.
 */
static char*
get_line_slot(void) 
{
  if (slots_in_use == slots_avail) {
    if (mappings == 0) {
      slots_avail = 512;
      mappings = malloc(slots_avail * sizeof(char*));
    } 
    else {
      slots_avail += 256;
      mappings = realloc(mappings, slots_avail * sizeof(char*));
    }
  }
  mappings[slots_in_use] = (char *) malloc(MAXLINELEN);
  *mappings[slots_in_use] = '\0';
  return mappings[slots_in_use++];
}

/* check if a line in a loadmap entry represents 
 * code by testing to see if execute permission
 * is set and file name has a path.
 */
static int 
iscodeline(char *line)
{
  char *blank = strchr(line, (int) ' ');
  char *slash = strchr(line, (int) '/');
  return (blank && *(blank + 3) == 'x' && slash);
}

/* discard the last line in the array of 
 * load map lines if it does not represent
 * executable code.
 */
static void 
finalizelines(void)
{
  int i;
  if (slots_in_use > 0) {
    int last = slots_in_use - 1;
    if (!iscodeline(mappings[last])) {
      free(mappings[last]);
      slots_in_use = last;
    }
  }
  
  rtloadmap.module = 
    (rtloadmod_desc_t *) malloc(sizeof(rtloadmod_desc_t) * slots_in_use);
  
  for(i = 0; i < slots_in_use; i++) {
    char* line = mappings[i];
    rtloadmap.module[i].name = strdup(get_name(line));
    rtloadmap.module[i].offset = hex2u64(get_beg_addr(line));
    rtloadmap.module[i].length = hex2ul(get_end_addr(line)) -
      rtloadmap.module[i].offset;
  }
  rtloadmap.count = slots_in_use;
#if TESTING
  dumprtloadmap();
#endif
  
  mappings = realloc(mappings, (slots_in_use +1) * sizeof(char*));
  slots_avail = slots_in_use + 1;
}

static char*
get_name(char *line)
{
  char *name = strchr(line, (int) '/');
  char *newline = strchr(line, (int) '\n');
  if (newline) *newline = 0;
  return name;
}

static char*
get_beg_addr(char *line)
{
  return line;
}

static char*
get_end_addr(char *line)
{
  char *minus = strchr(line, (int) '-');
  return minus + 1;
}

static unsigned long 
hex2ul(char *s)
{
  unsigned long res = 0;
  sscanf(s, "%lx", &res);
  return res;
}

static uint64_t 
hex2u64(char *s)
{
  uint64_t res = 0;
  sscanf(s, "%"SCNx64"", &res);
  return res;
}


/*  Dump a processed form of the load map
 */
static void 
dumprtloadmap(void)
{
  int i;
  fprintf(stderr, "Dumping currently mapped load modules (%d):\n", 
	  rtloadmap.count);
  for (i = 0; i < rtloadmap.count; i++) {
    fprintf(stderr, "  [%d] offset=%#0*"PRIx64" ", 
	    i, bhl, rtloadmap.module[i].offset);
    fprintf(stderr, "length=%#0*lx ",    bhl, rtloadmap.module[i].length);
    fprintf(stderr, "name=%s\n", rtloadmap.module[i].name);
  }
}

/*  Simply regurgitate the code lines of the load map
 */
static void 
dumplines(void)
{
  int i = 0;
  for (; i < slots_in_use; i++) {
    printf("%s\n",mappings[i]);
  }
}

#if TESTING

int 
main(void)
{
  hpcrun_get_rtloadmap(3);
#if 0
  dumplines();
#endif
  return 0;
}

#endif

