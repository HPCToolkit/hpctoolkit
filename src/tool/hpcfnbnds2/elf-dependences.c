// -*-Mode: C++;-*-

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
// Copyright ((c)) 2002-2019, Rice University
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

//*****************************************************************************
// system includes
//*****************************************************************************

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include <lib/prof-lean/file-permissions.h>


//*****************************************************************************
// macros
//*****************************************************************************

#define UNIT_TEST 1

#define DYNAMIC_SECTION_NAME ".dynamic"



//*****************************************************************************
// type declarations
//*****************************************************************************

typedef struct list_item_t {
  struct list_item_t *next;
  char *name; 
} list_item_t;



//*****************************************************************************
// global operations
//*****************************************************************************

static list_item_t *library_list = 0;

static list_item_t *library_runpath_list = 0;

static list_item_t *library_rpath_list = 0;

static list_item_t *library_soname_list = 0;

static list_item_t *default_path_list = 0;

static list_item_t *default_path64_list = 0;

static list_item_t *ld_library_path_list = 0;

static list_item_t *resolved_library_list = 0;



//*****************************************************************************
// private operations
//*****************************************************************************
static void
register_list_item
(
 const char *name,
 list_item_t **head
)
{
  list_item_t *entry = (list_item_t *) malloc(sizeof(list_item_t));
  entry->name = strdup(name);
  entry->next = *head;
  *head = entry;
}


static void
register_pathlist
(
 const char *pathlist,
 list_item_t **list
)
{
  char *pl = strdup(pathlist);
  // register paths (by pushing them on the front of a list) in
  // reverse order to preserve list order
  for(;;) {
    char *p = strrchr(pl,':'); // pointer before last path in the list
    if (!p) break; // only first item is left
    // register next path before null
    register_list_item(p+1, list); 
    *p = 0; // remove the last path off the list
  }
  register_list_item(pl, list); // first item in the list
  free(pl);
}


static list_item_t *
default_path_list_get
(
 void
)
{
  if (!default_path_list) {
    register_pathlist("/lib:/usr/lib", &default_path_list);
  }
  return default_path_list;
}


static list_item_t *
default_path64_list_get
(
 void
)
{
  if (!default_path64_list) {
    register_pathlist("/lib64:/usr/lib64", &default_path64_list);
  }
  return default_path64_list;
}


static list_item_t *
ld_library_path_list_get
(
 void
)
{
  if (!ld_library_path_list) {
    const char *ld_library_path = getenv("LD_LIBRARY_PATH");
    if (ld_library_path) {
      register_pathlist(ld_library_path, &ld_library_path_list);
    }
  }
  return ld_library_path_list;
}


static void
register_library
(
 const char *libname
)
{
  register_list_item(libname, &library_list);
}


static void
register_library_rpath
(
 const char *pathlist
)
{
  register_pathlist(pathlist, &library_rpath_list); // first item in the list
}


static void
register_library_runpath
(
 const char *pathlist
)
{
  register_pathlist(pathlist, &library_runpath_list); // first item in the list
}


static void
register_library_soname
(
 const char *libname
)
{
  register_list_item(libname, &library_soname_list);
}


static void
register_resolved_library
(
 const char *libname
)
{
  register_list_item(libname, &resolved_library_list);
}


void
dump_list
(
 list_item_t *head,
 const char *list_name
)
{
  printf("dumping list %s\n", list_name);
  while (head) {
    printf("\t%s\n", head->name);
    head = head->next;
  }
}


static void
dump_lists
(
 void
)
{
  dump_list(library_list,              "library list");
  dump_list(library_soname_list,       "library soname list");
  dump_list(library_runpath_list,      "library runpath list");
  dump_list(library_rpath_list,        "library rpath list");
  dump_list(default_path_list_get(),   "library default path list");
  dump_list(default_path64_list_get(), "library default path64 list");
  dump_list(resolved_library_list,     "resolved library list");
}


static char *
try_path
(
 const char *libname,
 const char *path
)
{
  char *result = NULL;

  char filepath[PATH_MAX];
  if (strlen(libname) + strlen(path) > PATH_MAX) return 0;

  sprintf(filepath, "%s/%s", path, libname);
  
  if (file_permissions_contain(filepath, R_X)) {
    result = strdup(filepath);
  } 

  return result;
}


static char *
try_pathlist
(
 const char *libname,
 list_item_t *pathlist
)
{
  
  char *result = 0; 
  for (; pathlist; pathlist = pathlist->next) {
    result = try_path(libname, pathlist->name);
    if (result) return result;
  }
  return 0;
}


//--------------------------------------------------------------------
// We resolve shared library dependencies per Linux ld.so
// documentation:
//
// If a shared object dependency does not contain a slash, then it is
// searched for in the following order:
//
//   Using the directories specified in the DT_RPATH dynamic section
//   attribute of the binary if present and DT_RUNPATH attribute does
//   not exist.  Use of DT_RPATH is deprecated.
//
//   Using the environment variable LD_LIBRARY_PATH, unless the
//   executable is being run in secure-execution mode (see below), in
//   which case this variable is ignored.
//
//   Using the directories specified in the DT_RUNPATH dynamic section
//   attribute of the binary if present.  Such directories are
//   searched only to find those objects required by DT_NEEDED (direct
//   dependencies) entries and do not apply to those objects'
//   children, which must themselves have their own DT_RUNPATH
//   entries.  This is unlike DT_RPATH, which is applied to searches
//   for all children in the dependency tree.
//
//   From the cache file /etc/ld.so.cache, which contains a compiled
//   list of candidate shared objects previously found in the
//   augmented library path.  If, however, the binary was linked with
//   the -z nodeflib linker option, shared objects in the default
//   paths are skipped.  Shared objects installed in hardware
//   capability directories (see below) are preferred to other shared
//   objects.
//
//   In the default path /lib, and then /usr/lib.  (On some 64-bit
//   architectures, the default paths for 64-bit shared objects are
//   /lib64, and then /usr/lib64.)  If the binary was linked with the
//   -z nodeflib linker option, this step is skipped.
//--------------------------------------------------------------------
static char *
resolve_path
(
 const char *libname,
 list_item_t *default_paths
)
{
  if (library_runpath_list) {
    char *result = try_pathlist(libname, library_runpath_list);
    if (result) return result;
  } else {
    char *result = try_pathlist(libname, library_rpath_list);
    if (result) return result;
  }

  char *result = try_pathlist(libname, ld_library_path_list);
  if (result) return result;

  char *result = try_pathlist(libname, default_paths);
  if (result) return result;
}


static void
resolve_libraries
(
 void
)
{
  list_item_t *i = library_list;
  for (; i; i = i->next) {
    const char *path = resolve_path(i->name, default_path64_list_get()); 
    if (path && path[0] == '/') {
      register_resolved_library(path);
    }
  }
}


static void
elf_process_dynamic
(
 Elf *elf, 
 Elf_Scn *scn,
 GElf_Shdr *shdr
)
{
  Elf_Data *data = elf_getdata(scn, NULL);
  if (data == NULL) return;

  size_t sh_entsize = gelf_fsize(elf, ELF_T_DYN, 1, EV_CURRENT);
  int nitems = shdr->sh_size/sh_entsize; 

  int i;
  for (i = 0; i < nitems; ++i) {
    GElf_Dyn dyn_v;
    GElf_Dyn *dyn = gelf_getdyn(data, i, &dyn_v);

    if (dyn == NULL) break;

    switch (dyn->d_tag) {
    case DT_NEEDED:
      register_library(elf_strptr(elf, shdr->sh_link, dyn->d_un.d_val));
      break;

    case DT_RPATH:
      register_library_rpath(elf_strptr(elf, shdr->sh_link, dyn->d_un.d_val));
      break;

    case DT_RUNPATH:
      register_library_runpath(elf_strptr(elf, shdr->sh_link, dyn->d_un.d_val));
      break;

    case DT_SONAME:
      register_library_soname(elf_strptr(elf, shdr->sh_link, dyn->d_un.d_val));
      break;

    default:
      break;
    }
  }
}



//*****************************************************************************
// interface operations
//*****************************************************************************

void
elf_dependents(Elf *elf)
{
  if (elf) {
    GElf_Ehdr ehdr_v;
    GElf_Ehdr *ehdr = gelf_getehdr(elf, &ehdr_v);
    if (ehdr) {
      Elf_Scn *scn = NULL;
      while ((scn = elf_nextscn(elf, scn)) != NULL) {
	GElf_Shdr shdr;
	if (!gelf_getshdr(scn, &shdr)) continue;
	const char *name = elf_strptr(elf, ehdr->e_shstrndx, shdr.sh_name);
	if (strcmp(name, DYNAMIC_SECTION_NAME) == 0) {
	  elf_process_dynamic(elf, scn, &shdr);
	}
      }
    }
  }
}



//*****************************************************************************
// unit test
//*****************************************************************************


#if UNIT_TEST


int
main
(
 int argc, 
 char **argv
)
{
  if (argc != 2) {
    printf("usage: %s <load module name>\n", argv[0]);
    exit(-1);
  }

  const char *name = argv[1];

  // open the file
  int fd = open(name, O_RDONLY);
  if (fd == -1) {
    printf("%s: open failed: %s", argv[0], strerror(errno));
    exit(-1);
  }

  if (elf_version(EV_CURRENT) == EV_NONE) {
    printf("%s: problem setting elf version: %s\n", argv[0], elf_errmsg(-1));
    close (fd);
    exit(-1);
  }


  Elf *elf = elf_begin(fd, ELF_C_READ, NULL);
  if (elf == NULL) {
    printf("%s: elf_begin failed: %s", argv[0], elf_errmsg(-1));
    close (fd);
    exit(-1);
  }

  elf_dependents(elf);

  resolve_libraries();

  dump_lists();

  return 0;
}


#endif
