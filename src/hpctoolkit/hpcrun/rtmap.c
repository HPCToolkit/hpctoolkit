/* -*-Mode: C;-*- */
/* $Id$ */

/****************************************************************************
//
// File: 
//    $Source$
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

static char* name(char *line);
static char* offset(char *line);
static char* length(char *line);
static long htoll(char *s);

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
 *  and load addresses.
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
    } else
      slots_avail += 256;
      mappings = realloc(mappings, slots_avail * sizeof(char));
  }
  mappings[slots_in_use] = (char *) malloc(MAXLINELEN);
  *mappings[slots_in_use] = (char) NULL;
  return mappings[slots_in_use++];
}

/* check if a line in a loadmap entry represents 
 * code by testing to see if execute permission
 * is set and file name has a path.
 */
static int 
iscodeline(char *line)
{
  char *blank = index(line, (int) ' ');
  char *slash = index(line, (int) '/');
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
    rtloadmap.module[i].name = strdup(name(mappings[i]));
    rtloadmap.module[i].offset = htoll(offset(mappings[i]));
    rtloadmap.module[i].length = htoll(length(mappings[i])) -
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
name(char *line)
{
  char *name = index(line, (int) '/');
  char *newline = index(line, (int) '\n');
  if (newline) *newline = 0;
  return name;
}

static char*
offset(char *line)
{
  return line;
}

static char*
length(char *line)
{
  char *minus = index(line, (int) '-');
  return minus + 1;
}

static long 
htoll(char *s)
{
  unsigned long res = 0;
  sscanf(s,"%lx",&res);
  return res;
}

/*  Dump a processed form of the load map
 */
static void 
dumprtloadmap(void)
{
  int i;
  fprintf(stderr, "Dumping currently mapped load modules:\n");
  for (i = 0; i < rtloadmap.count; i++) {
    fprintf(stderr, "  offset=%#0*"PRIx64" ", bhl, rtloadmap.module[i].offset);
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

