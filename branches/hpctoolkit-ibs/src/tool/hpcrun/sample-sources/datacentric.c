// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/tool/hpcrun/sample-sources/sync.c $
// $Id: sync.c 2770 2010-03-06 02:54:44Z tallent $
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


/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>



/******************************************************************************
 * libmonitor
 *****************************************************************************/

#include <monitor.h>



/******************************************************************************
 * local includes
 *****************************************************************************/

#include <hpcrun/hpcrun_options.h>
#include <hpcrun/disabled.h>
#include <hpcrun/metrics.h>
#include <sample_event.h>
#include "sample_source_obj.h"
#include "common.h"
#include <main.h>
#include <hpcrun/sample_sources_registered.h>
#include "simple_oo.h"
#include <hpcrun/thread_data.h>

#include <messages/messages.h>
#include "files.h"
#include "ibs_init.h"
#include "libelf/libelf.h"
#include "libelf/gelf.h"

static int malloc_id = -1;
static int calloc_id = -1;
static int realloc_id = -1;

unsigned long long bss_start;
unsigned long long bss_end;
static int count = -2;//static data ids are even and less than 0

void bss_partition(int fd);
int bss_find(int fd);//return 0 means wrong, return 1 means true
void compute_var(int fd);
void create_static_data_table();
int insert_splay_tree(interval_tree_node*, void*, size_t, int32_t);


/******************************************************************************
 * method definitions
 *****************************************************************************/

static void
METHOD_FN(init)
{
  self->state = INIT; 

  // reset static variables to their virgin state
  malloc_id = -1;
  calloc_id = -1;
  realloc_id = -1;
  
  //create static data table
  create_static_data_table();
}


static void
METHOD_FN(start)
{
  TMSG(IBS_SAMPLE,"starting data-centric analysis");

  TD_GET(ss_state)[self->evset_idx] = START;
}


static void
METHOD_FN(stop)
{
  malloc_id = -1;
  calloc_id = -1;
  realloc_id = -1;

  TMSG(IBS_SAMPLE,"stopping data-centric analysis");
  TD_GET(ss_state)[self->evset_idx] = STOP;
}


static void
METHOD_FN(shutdown)
{
  METHOD_CALL(self,stop); // make sure stop has been called
  self->state = UNINIT;
}


static bool
METHOD_FN(supports_event,const char *ev_str)
{
  return (strstr(ev_str,"DATACENTRIC") != NULL);
}
 

// MEMLEAK creates two metrics: bytes allocated and Bytes Freed.

static void
METHOD_FN(process_event_list,int lush_metrics)
{
  malloc_id = hpcrun_new_metric();
  calloc_id = hpcrun_new_metric();
  realloc_id = hpcrun_new_metric();

  TMSG(IBS_SAMPLE, "Setting up metrics for data-centric analysis");

  hpcrun_set_metric_info_and_period(malloc_id, "malloc",
				    HPCRUN_MetricFlag_Async, 1);

  hpcrun_set_metric_info_and_period(calloc_id, "calloc",
				    HPCRUN_MetricFlag_Async, 1);

  hpcrun_set_metric_info_and_period(realloc_id, "realloc",
                                    HPCRUN_MetricFlag_Async, 1);

}


//
// Event sets not relevant for this sample source
// Events are generated by user code
//
static void
METHOD_FN(gen_event_set,int lush_metrics)
{
  thread_data_t *td = hpcrun_get_thread_data();
  td->eventSet[self->evset_idx] = 0xDEAD; 
}


//
//
//
static void
METHOD_FN(display_events)
{
  printf("===========================================================================\n");
  printf("Available Data-centric analysis events\n");
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
  printf("\t  DATACENTRIC: data-centric analysis (together with IBS_SAMPLE event)\n");
  printf("\n");
}

/***************************************************************************
 * object
 ***************************************************************************/

//
// sync class is "SS_SOFTWARE" so that both synchronous and asynchronous sampling is possible
// 

#define ss_name datacentric
#define ss_cls SS_SOFTWARE

#include "ss_obj.h"


// ***************************************************************************
//  Interface functions
// ***************************************************************************

// increment the return count
//
// N.B. : This function is necessary to avoid exposing the retcnt_obj.
//        For the case of the retcnt sample source, the handler (the trampoline)
//        is separate from the sample source code.
//        Consequently, the interaction with metrics must be done procedurally


int
hpcrun_datacentric_active()
{
  if (hpcrun_is_initialized()&&!hpcrun_is_in_init_thread()) {
    return (TD_GET(ss_state)[obj_name().evset_idx] == START);
  } else {
    return 0;
  }
}

int
hpcrun_dc_malloc_id() 
{
  return malloc_id;
}

int
hpcrun_dc_calloc_id()    
{
  return calloc_id;
}

int
hpcrun_dc_realloc_id()    
{
  return realloc_id;
}

void
create_static_data_table()
{
  int fd = -1;
  const char *exec = files_executable_pathname();
  
  fd = open(exec, O_RDONLY);
  bss_partition(fd);
  close(fd);
}

/*forward declaration*/
int bss_find(int fd);//return 0 means wrong, return 1 means true
void compute_var(int fd);
 
void bss_partition(int fd)
{
  if(bss_find(fd)==0)
    return;
  lseek(fd, 0, SEEK_SET);//move to the starting point of the file
  compute_var(fd);
}
 
int bss_find(int fd)
{
  Elf32_Ehdr *elf_header;         /* ELF header */
  Elf *elf;                       /* Our Elf pointer for libelf */
  Elf_Scn *scn = NULL;                   /* Section Descriptor */
  Elf_Data *edata = NULL;                /* Data Descriptor */
  GElf_Sym sym;                   /* Symbol */
  GElf_Shdr shdr;                 /* Section Header */
 
  char *base_ptr;         // ptr to our object in memory
  const char *bss = "__bss_start";
  const char *bss1 = "_end";
  struct stat elf_stats;  // fstat struct
  int i, symbol_count;
 
  if((fstat(fd, &elf_stats)))
  {
    EMSG("bss: could not fstat");
    close(fd);
    return 0;
  }
  if((base_ptr = (char *) malloc(elf_stats.st_size)) == NULL)
  {
    EMSG("could not malloc");
    close(fd);
    return 0;
  }
  if((read(fd, base_ptr, elf_stats.st_size)) < elf_stats.st_size)
  {
    EMSG("could not read");
    free(base_ptr);
    close(fd);
    return 0;
  }
 
  if(elf_version(EV_CURRENT) == EV_NONE)
  {
    EMSG("WARNING Elf Library is out of date!");
  }
  elf_header = (Elf32_Ehdr *) base_ptr;   // point elf_header at our object in memory
  elf = elf_begin(fd, ELF_C_READ, NULL);  // Initialize 'elf' pointer to our file descri
 
  /* find the beginning and ending point of bss section */
  while((scn = elf_nextscn(elf, scn)) != NULL)
  {
    gelf_getshdr(scn, &shdr);
    // When we find a section header marked SHT_SYMTAB stop and get symbols
        if(shdr.sh_type == SHT_SYMTAB)
    {
       // edata points to our symbol table
       edata = elf_getdata(scn, edata);
       symbol_count = shdr.sh_size/shdr.sh_entsize;
       // loop through to grab all symbols
       for(i = 0; i < symbol_count; i++)
       {
         // libelf grabs the symbol data using gelf_getsym()
         gelf_getsym(edata, i, &sym);
         char *symname = elf_strptr(elf, shdr.sh_link, sym.st_name);
         if(strcmp(bss, symname)==0)
         {
           bss_start = sym.st_value;
           int j = i+1;
           while(1)
           {
             gelf_getsym(edata, j, &sym);
             char *symname1 = elf_strptr(elf, shdr.sh_link, sym.st_name);
             if(strcmp(bss1, symname1)!=0)
             {
               j++;
               continue;
             }
             bss_end = sym.st_value;
             return 1;
           }
         }
       }
    }
  }
  return 1;
}
 
void compute_var(int fd)
{
  Elf32_Ehdr *elf_header;         /* ELF header */
  Elf *elf;                       /* Our Elf pointer for libelf */
  Elf_Scn *scn = NULL;                   /* Section Descriptor */
  Elf_Data *edata = NULL;                /* Data Descriptor */
  GElf_Sym sym;                   /* Symbol */
  GElf_Shdr shdr;                 /* Section Header */
 
  char *base_ptr;         // ptr to our object in memory
  const char *bss = "__bss_start";
  const char *bss1 = "_end";
  struct stat elf_stats;  // fstat struct
  int i, symbol_count;
 
  if((fstat(fd, &elf_stats)))
  {
    EMSG("bss: could not fstat");
    close(fd);
    exit(1);
  }
  if((base_ptr = (char *) malloc(elf_stats.st_size)) == NULL)
  {
    EMSG("could not malloc");
    close(fd);
    exit(1);
  }
  if((read(fd, base_ptr, elf_stats.st_size)) < elf_stats.st_size)
  {
    EMSG("could not read");
    free(base_ptr);
    close(fd);
    exit(1);
  }
  if(elf_version(EV_CURRENT) == EV_NONE)
  {
    EMSG("WARNING Elf Library is out of date!");
  }
  elf_header = (Elf32_Ehdr *) base_ptr;   // point elf_header at our object in memory
  elf = elf_begin(fd, ELF_C_READ, NULL);  // Initialize 'elf' pointer to our file descriptor

  while((scn = elf_nextscn(elf, scn)) != NULL)
  {
    gelf_getshdr(scn, &shdr);
    if(shdr.sh_type == SHT_SYMTAB)
    {
      edata = elf_getdata(scn, edata);
      symbol_count = shdr.sh_size / shdr.sh_entsize;
      for (i = 0; i < symbol_count; i++)
      {
        if(gelf_getsym(edata, i, &sym) == NULL)
        {
          EMSG("gelf_getsym return NULL");
          EMSG("%s", elf_errmsg(elf_errno()));
          exit(1);
        }
        /*dummey is of no use. But without it the code cannot run correctly*/
        unsigned long long dummey = sym.st_value;
        dummey = 0;
 
        if((sym.st_value < bss_start)||(sym.st_value > bss_end))//not in bss section
        {
          continue;
        }
        if(sym.st_size == 0)//not a variable
        {
          continue;
        }
        char *symname2 = elf_strptr(elf, shdr.sh_link, sym.st_name);
        interval_tree_node* node = hpcrun_malloc(sizeof(interval_tree_node));
        TMSG(IBS_SAMPLE, "static data %d", count);
        if(insert_splay_tree(node, (void*)sym.st_value, (size_t)sym.st_size, count)<0)
          EMSG("insert_splay_tree error");
        count-=2;
      }
    }
  }
}
