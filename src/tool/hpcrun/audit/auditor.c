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
// Copyright ((c)) 2002-2022, Rice University
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


//******************************************************************************
// global includes
//******************************************************************************

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <link.h>
#include <sched.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


//******************************************************************************
// local includes
//******************************************************************************

#include "audit-api.h"



//******************************************************************************
// macros
//******************************************************************************

// define the architecture-specific GOT index where the address of loader's 
// resolver function can be found

#if defined(HOST_CPU_x86_64) || defined(HOST_CPU_x86)
#define GOT_resolver_index 2
#elif defined(HOST_CPU_PPC)
#define GOT_resolver_index 0
#elif defined(HOST_CPU_ARM64)
#define GOT_resolver_index 2
#else
#error "GOT resolver index for the host architecture is unknown"
#endif



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct object_t {
  // Doubly-linked list for buffered entries
  struct object_t* next;
  struct object_t* prev;

  // Storage for program headers, if they need to be read manually
  ElfW(Phdr)* phdrs_storage;

  // If true, this is object is the main executable, and the aux vector should
  // be used to get most values.
  bool isMainExecutable : 1;

  // If true, this object is the VDSO segment.
  bool isVDSO : 1;

  // If true, the link_map referred to in this object is not a "real" link_map
  // structure from Glibc, and so can be edited at will.
  bool isFakeLinkMap : 1;

  // Publicly available data
  auditor_map_entry_t entry;
} object_t;

//******************************************************************************
// local data
//******************************************************************************

static bool verbose = false;
static char* mainlib = NULL;
static bool disable_plt_call_opt = false;
static ElfW(Addr) dl_runtime_resolver_ptr = 0;

static object_t* buffer_head = NULL;
static object_t* buffer_tail = NULL;

static uintptr_t* mainlib_cookie = NULL;
static auditor_attach_pfn_t pfn_attach = NULL;
static bool connected = false;
static auditor_hooks_t hooks;

static void mainlib_connected(const char*);
static void mainlib_disconnect();

static auditor_exports_t exports = {
  .mainlib_connected = mainlib_connected,
  .mainlib_disconnect = mainlib_disconnect,
  .pipe = pipe, .close = close, .waitpid = waitpid,
  .clone = clone, .execve = execve
};



//******************************************************************************
// private operations
//******************************************************************************

// Wrapper for pread that makes multiple calls until everything requested is
// read. Returns true on success.
static bool pread_full(int fd, void* buf, size_t count, off_t offset) {
  while(count > 0) {
    ssize_t got = pread(fd, buf, count, offset);
    if(got < 1) return false;  // error or EOF
    buf += got;
    count -= got;
    offset += got;
  }
  return true;
}


// Open the file for a binary. Returns an owned file descriptor.
static int open_object(const object_t* obj) {
  const char* path = obj->entry.path;
  if(path == NULL && !obj->isFakeLinkMap)
    path = obj->entry.map->l_name;

  // For the main executable, get information out of the aux vector. It's faster
  // and more reliable when it gives us answers.
  if(obj->isMainExecutable) {
    // First try using the executable's file descriptor itself
    int fd = getauxval(AT_EXECFD);
    if(fd != 0) {
      fd = dup(fd);  // This makes sure the fd is still valid
      if(fd != -1) return fd;
    }

    // If that fails, try /proc/self/exe next
    fd = open("/proc/self/exe", O_RDONLY);
    if(fd != -1) return fd;

    // If that fails, fall back on the filename-based approach
    unsigned long auxv = getauxval(AT_EXECFN);
    if(auxv != 0) path = (void*)auxv;
  }

  return path == NULL ? -1 : open(path, O_RDONLY);
}

static bool fetch_hdrs(object_t* obj) {
  assert(obj->entry.dl_info.dlpi_phdr == NULL);
  obj->isVDSO = false;

  // For the main executable, use the aux vector if we can. It's faster and
  // more reliable for non-PIE cases.
  if(obj->isMainExecutable) {
    unsigned long phdrs = getauxval(AT_PHDR);
    unsigned long phnum = getauxval(AT_PHNUM);
    if(phdrs != 0 && phnum != 0) {
      obj->entry.dl_info.dlpi_phdr = (void*)phdrs;
      obj->entry.dl_info.dlpi_phnum = phnum;
      return true;
    }
    // getauxval failed, maybe the kernel has the auxv disabled?
  }

  int fd = -1;
  ElfW(Ehdr) ehdr_storage;

  // l_addr is the base of the binary, which is the ELF header...
  const ElfW(Ehdr)* ehdr = (void*)obj->entry.map->l_addr;
  if(ehdr == 0) {
    // ...except in position-dependent binaries. In that case pop open the file
    // and read the header for ourselves.
    if(fd == -1) {
      fd = open_object(obj);
      if(fd == -1) {
        if(verbose)
          fprintf(stderr, "[audit] Failed to load phdrs for `%s': failed to open file\n",
                  obj->entry.map->l_name);
        return false;
      }
    }
    if(!pread_full(fd, &ehdr_storage, sizeof ehdr_storage, 0)) {
      if(verbose)
        fprintf(stderr, "[audit] Failed to load phdrs for `%s': I/O error or EOF while reading ELF header\n",
                obj->entry.map->l_name);
      close(fd);
      return false;
    }
    ehdr = &ehdr_storage;
  } else {
    // If we do have the mapped ELF header, check if this is the VDSO using the
    // aux vector, if it's available.
    unsigned long vdso = getauxval(AT_SYSINFO_EHDR);
    if(vdso != 0)
      obj->isVDSO = vdso == obj->entry.map->l_addr;
  }
  // Triple-check that whatever we ended up with in the previous mess is indeed
  // an ELF file. Messing this up would be bad for our health.
  if(ehdr->e_ident[EI_MAG0] != ELFMAG0 || ehdr->e_ident[EI_MAG1] != ELFMAG1
     || ehdr->e_ident[EI_MAG2] != ELFMAG2 || ehdr->e_ident[EI_MAG3] != ELFMAG3) {
    if(verbose)
      fprintf(stderr, "[audit] Failed to load phdrs for `%s': not an ELF binary\n",
              obj->entry.map->l_name);
    if(fd != -1) close(fd);
    return false;
  }

  // Ignore binaries without program headers. We can't do a thing with 'em.
  if(ehdr->e_phnum == 0) {
    if(verbose)
      fprintf(stderr, "[audit] Failed to load phdrs for `%s': phnum is 0\n",
              obj->entry.map->l_name);
    return false;
  }

  obj->entry.dl_info.dlpi_phnum = ehdr->e_phnum;

  // If the program headers are right after the ELF header, we assume they will
  // land in the same PT_LOAD segment. If we didn't have to open the file by
  // this point we can access them directly.
  // Elf64 header is 64 bytes long.
  if(ehdr->e_phoff <= 64 && fd == -1) {
    obj->entry.dl_info.dlpi_phdr = (void*)(obj->entry.map->l_addr + ehdr->e_phoff);
    return true;
  }

  // Open the file and read out the program headers.
  if(fd == -1) {
    fd = open_object(obj);
    if(fd == -1) {
      if(verbose)
        fprintf(stderr, "[audit] Failed to load phdrs for `%s': failed to open file\n",
                obj->entry.map->l_name);
      return false;
    }
  }
  obj->phdrs_storage = malloc(sizeof(ElfW(Phdr)) * ehdr->e_phnum);
  for(size_t i = 0; i < ehdr->e_phnum; i++) {
    if(!pread_full(fd, &obj->phdrs_storage[i], sizeof(ElfW(Phdr)),
                   ehdr->e_phoff + ehdr->e_phentsize * i)) {
      if(verbose)
        fprintf(stderr, "[audit] Failed to load phdrs for `%s': I/O error or EOF while reading Phdrs\n",
                obj->entry.map->l_name);
      free(obj->phdrs_storage);
      close(fd);
      return false;
    }
  }
  obj->entry.dl_info.dlpi_phdr = obj->phdrs_storage;
  close(fd);
  return true;
}

// Finalize an object structure before notifying libhpcrun.so about it. Unlike
// fetch_hdrs above, this one handles failures more gracefully.
static void complete_object(object_t* obj) {
  // Fetch the path to the object, if it hasn't been gotten already.
  if(obj->entry.path == NULL) {
    const char* path = obj->entry.map->l_name;

    // Use /proc or the aux vector for the main executable. They're more
    // reliable when available.
    if(obj->isMainExecutable) {
      obj->entry.path = realpath("/proc/self/exe", NULL);
      if(obj->entry.path != NULL && strcmp(obj->entry.path, "/proc/self/exe") != 0) {
        unsigned long v = getauxval(AT_EXECFN);
        if(v != 0) path = (void*)v;
      } else if(obj->entry.path != NULL) {
        free(obj->entry.path);
        obj->entry.path = NULL;
      }
    }

    // Resolve to a full path. This may be NULL if the file no longer exists at
    // a path where we can access it.
    if(obj->entry.path == NULL)
      obj->entry.path = realpath(path, NULL);
  }

  // Calculate the executable code range from the program headers, relative to
  // the l_addr base address.
  assert(obj->entry.dl_info.dlpi_phnum > 0);
  uintptr_t start = UINTPTR_MAX;
  uintptr_t end = 0;
  for(size_t i = 0; i < obj->entry.dl_info.dlpi_phnum; i++) {
    const ElfW(Phdr)* phdr = &obj->entry.dl_info.dlpi_phdr[i];
    if(phdr->p_type == PT_LOAD && (phdr->p_flags & PF_X) != 0 && phdr->p_memsz > 0) {
      if(phdr->p_vaddr < start)
        start = phdr->p_vaddr;
      if(phdr->p_vaddr + phdr->p_memsz > end)
        end = phdr->p_vaddr + phdr->p_memsz;
    } else if(phdr->p_type == PT_DYNAMIC && obj->isFakeLinkMap) {
      // Fill l_ld if the link_map is fake
      obj->entry.map->l_ld = (void*)(obj->entry.map->l_addr + phdr->p_vaddr);
    }
  }

  if(end > 0) {
    obj->entry.start = (void*)(obj->entry.map->l_addr + start);
    obj->entry.end = (void*)(obj->entry.map->l_addr + end);
  }
}


// Search a link_map's (dynamic) symbol table until we find the given symbol.
static void* get_symbol(struct link_map* map, const char* name) {
  // Nab the STRTAB and SYMTAB
  const char* strtab = NULL;
  ElfW(Sym)* symtab = NULL;
  for(const ElfW(Dyn)* d = map->l_ld; d->d_tag != DT_NULL; d++) {
    if(d->d_tag == DT_STRTAB) strtab = (const char*)d->d_un.d_ptr;
    else if(d->d_tag == DT_SYMTAB) symtab = (ElfW(Sym)*)d->d_un.d_ptr;
    if(strtab && symtab) break;
  }
  if(!strtab || !symtab) abort();  // This should absolutely never happen

  // Hunt down the given symbol. It must exist, or we run off the end.
  for(ElfW(Sym)* s = symtab; ; s++) {
    if(ELF64_ST_TYPE(s->st_info) != STT_FUNC) continue;
    if(strcmp(&strtab[s->st_name], name) == 0)
      return (void*)(map->l_addr + s->st_value);
  }
}


static ElfW(Addr)* get_plt_got_start(ElfW(Dyn)* dyn) {
  for(ElfW(Dyn)* d = dyn; d->d_tag != DT_NULL; d++) {
    if(d->d_tag == DT_PLTGOT) {
      return (ElfW(Addr)*)d->d_un.d_ptr;
    }
  }
  return NULL;
}

static void optimize_object_plt(const object_t* obj) {
  // Scan for the DT_PLTGOT entry, so we know where the GOT is
  ElfW(Addr)* plt_got = get_plt_got_start(obj->entry.map->l_ld);
  if(plt_got == NULL) {
    fprintf(stderr, "[audit] Failed to find GOTPLT section in `%s'!\n",
            obj->entry.map->l_name);
    return;
  }

  // If the original entry is already optimized, silently skip
  if(plt_got[GOT_resolver_index] == dl_runtime_resolver_ptr)
    return;

  // If the original entry is NULL, we skip it (obviously something is wrong)
  if(plt_got[GOT_resolver_index] == 0) {
    if(verbose)
      fprintf(stderr, "[audit] Skipping optimization of `%s', original entry %p is NULL\n",
              obj->entry.map->l_name, &plt_got[GOT_resolver_index]);
    return;
  }

  // Print out some debugging information
  if(verbose) {
    Dl_info info;
    Dl_info info2;
    if(!dladdr((void*)plt_got[GOT_resolver_index], &info))
      info = (Dl_info){NULL, NULL, NULL, NULL};
    if(!dladdr((void*)dl_runtime_resolver_ptr, &info2))
      info = (Dl_info){NULL, NULL, NULL, NULL};
    if(info.dli_fname != NULL && info2.dli_fname != NULL
       && strcmp(info.dli_fname, info2.dli_fname) == 0)
      info2.dli_fname = "...";
    fprintf(stderr, "[audit] Optimizing `%s': %p (%s+%p) -> %p (%s+%p)\n",
            obj->entry.map->l_name,
            (void*)plt_got[GOT_resolver_index], info.dli_fname,
            (void*)plt_got[GOT_resolver_index]-(ptrdiff_t)info.dli_fbase,
            (void*)dl_runtime_resolver_ptr, info2.dli_fname,
            (void*)dl_runtime_resolver_ptr-(ptrdiff_t)info2.dli_fbase);
  }

  // Figure out the mask for doing page alignments
  static uintptr_t pagemask = 0;
  if(pagemask == 0) pagemask = ~(getpagesize()-1);

  // Enable write permissions for the GOT
  const struct dl_phdr_info* phdrs = &obj->entry.dl_info;
  for(size_t i = 0; i < phdrs->dlpi_phnum; i++) {
    // The GOT (and some other stuff) is listed in "a segment which may be made
    // read-only after relocations have been processed."
    // If we don't see this we assume the linker won't make the GOT unwritable.
    if(phdrs->dlpi_phdr[i].p_type == PT_GNU_RELRO) {
      // We include PROT_EXEC since the loader may share these pages with
      // other unrelated sections, such as code regions.
      //
      // We also have to open up the entire GOT table since we can't trust
      // the linker to leave the it open for the resolver in the LD_AUDIT case.
      void* start = (void*)(((uintptr_t)phdrs->dlpi_addr + phdrs->dlpi_phdr[i].p_vaddr) & pagemask);
      void* end = (void*)(phdrs->dlpi_addr + phdrs->dlpi_phdr[i].p_vaddr + phdrs->dlpi_phdr[i].p_memsz);
      mprotect(start, end - start, PROT_READ | PROT_WRITE | PROT_EXEC);
    }
  }

  // Overwrite the resolver entry
  plt_got[GOT_resolver_index] = dl_runtime_resolver_ptr;
}


//******************************************************************************
// interface operations
//******************************************************************************

unsigned int la_version(unsigned int version) {
  if(version < 1) return 0;

  // Read in our arguments
  verbose = getenv("HPCRUN_AUDIT_DEBUG");

  // Check if we need to optimize PLT calls
  disable_plt_call_opt = getenv("HPCRUN_AUDIT_DISABLE_PLT_CALL_OPT");
  if (!disable_plt_call_opt) {
    ElfW(Addr)* plt_got = get_plt_got_start(_DYNAMIC);
    if (plt_got != NULL) {
      dl_runtime_resolver_ptr = plt_got[GOT_resolver_index];
      if(verbose)
        fprintf(stderr, "[audit] PLT optimized resolver: %p\n", (void*)dl_runtime_resolver_ptr);
    } else if(verbose) {
      fprintf(stderr, "[audit] Failed to find PLTGOT section in auditor!\n");
    }
  }

  mainlib = realpath(getenv("HPCRUN_AUDIT_MAIN_LIB"), NULL);
  if(verbose)
    fprintf(stderr, "[audit] Awaiting mainlib `%s'\n", mainlib);

  // Generate the purified environment before the app changes it
  // NOTE: Consider removing only ourselves to allow for Spindle-like optimizations.
  {
    size_t envsz = 0;
    for(char** e = environ; *e != NULL; e++) envsz++;
    exports.pure_environ = malloc((envsz+1)*sizeof exports.pure_environ[0]);
    size_t idx = 0;
    for(char** e = environ; *e != NULL; e++) {
      if(strncmp(*e, "LD_PRELOAD=", 11) == 0) continue;
      if(strncmp(*e, "LD_AUDIT=", 9) == 0) continue;
      exports.pure_environ[idx++] = *e;
    }
    exports.pure_environ[idx] = NULL;
  }

  return 1;
}


unsigned int la_objopen(struct link_map* map, Lmid_t lmid, uintptr_t* cookie) {
  // Fill in the basics of what we know about this object
  object_t* obj = malloc(sizeof *obj);
  if(!obj) abort();
  *cookie = (uintptr_t)obj;

  *obj = (object_t){
    .next = NULL, .prev = NULL,
    .phdrs_storage = NULL,
    .isMainExecutable = lmid == LM_ID_BASE && map->l_prev == NULL,
    .isVDSO = false,  // Filled by fetch_hdrs
    .isFakeLinkMap = false,
    .entry = (auditor_map_entry_t){
      .path = NULL,
      .start = NULL, .end = NULL,
      .dl_info = (struct dl_phdr_info){
        .dlpi_addr = map->l_addr,
        .dlpi_name = map->l_name,
        .dlpi_phdr = NULL, .dlpi_phnum = 0,
      },
      .dl_info_sz = offsetof(struct dl_phdr_info, dlpi_phnum)
                    + sizeof obj->entry.dl_info.dlpi_phnum,
      .load_module = NULL,
      .map = map,
    },
  };

  // If we haven't found it already, check if this is libhpcrun.so.
  if(mainlib_cookie == NULL && mainlib != NULL) {
    // NOTE: libhpcrun.so is always PIC, so we don't need special cases here
    obj->entry.path = realpath(map->l_name, NULL);
    if(obj->entry.path != NULL) {
      if(strcmp(obj->entry.path, mainlib) == 0) {
        free(mainlib);
        mainlib = NULL;
        mainlib_cookie = cookie;
        pfn_attach = get_symbol(map, "hpcrun_auditor_attach");
        if(verbose)
          fprintf(stderr, "[audit] Located libhpcrun.so, attach hook is: %p\n", pfn_attach);
      }
    }
  }

  // Fetch the program headers, we *always* need them. If we can't get them
  // drop the binary on the floor and ignore it completely.
  if(!fetch_hdrs(obj)) {
    free(obj);
    *cookie = 0;
    return LA_FLG_BINDFROM | LA_FLG_BINDTO;
  }

  // If we're connected finalize and notify the new object, otherwise buffer it
  // and wait until we're connected.
  if(connected) {
    complete_object(obj);
    if(verbose)
      fprintf(stderr, "[audit] Delivering objopen for `%s' [%p, %p)\n",
              map->l_name, obj->entry.start, obj->entry.end);
    hooks.open(&obj->entry);
  } else {
    if(verbose)
      fprintf(stderr, "[audit] la_objopen: buffering while unconnected: `%s'\n",
              map->l_name);

    if(buffer_head == NULL) buffer_head = obj;
    obj->prev = buffer_tail;
    if(buffer_tail != NULL) buffer_tail->next = obj;
    buffer_tail = obj;
  }
  return LA_FLG_BINDFROM | LA_FLG_BINDTO;
}


static void mainlib_connected(const char* vdso_path) {
  assert(!connected && "Attempt to connect more than once?");
  connected = true;

  // Finalize and deliver notifications for all the objects we've buffered
  if(verbose)
    fprintf(stderr, "[audit] libhpcrun.so is connected, draining buffered objopens...\n");
  bool foundVDSO = false;
  while(buffer_head != NULL) {
    // Unlink the front object from the buffer
    object_t* obj = buffer_head;
    buffer_head = obj->next;
    obj->prev = obj->next = NULL;

    if(obj->isVDSO) foundVDSO = true;

    // Finalize and notify
    complete_object(obj);
    if(verbose)
      fprintf(stderr, "[audit] Delivering buffered objopen for `%s'\n",
              obj->entry.map->l_name);
    hooks.open(&obj->entry);
  }
  buffer_tail = NULL;

  // Make sure there's always a vDSO object, even if we have to make one up
  if(!foundVDSO) {
    unsigned long vdso_addr = getauxval(AT_SYSINFO_EHDR);
    if(vdso_addr != 0) {
      // Synthesize a link_map entry for the vDSO
      struct link_map* vdso_map = malloc(sizeof *vdso_map);
      if(!vdso_map) abort();
      vdso_map->l_addr = vdso_addr;
      vdso_map->l_name = "[vdso]";
      vdso_map->l_ld = NULL;  // Filled by fetch_hdrs
      vdso_map->l_next = vdso_map->l_prev = NULL;

      // Fill an appropriate object_t
      object_t* obj = malloc(sizeof *obj);
      if(!obj) abort();
      *obj = (object_t){
        .next = NULL, .prev = NULL,
        .phdrs_storage = NULL,
        .isMainExecutable = false,
        .isVDSO = true,
        .isFakeLinkMap = true,
        .entry = (auditor_map_entry_t){
          .path = (char*)vdso_path,
          .start = NULL, .end = NULL,
          .dl_info = (struct dl_phdr_info){
            .dlpi_addr = vdso_addr,
            .dlpi_name = vdso_map->l_name,
            .dlpi_phdr = NULL, .dlpi_phnum = 0,
          },
          .load_module = NULL,
          .map = vdso_map,
        },
      };

      if(!fetch_hdrs(obj)) {
        // Clean up, we'll just give up.
        if(verbose)
          fprintf(stderr, "[audit] Error synthesizing object for vDSO...\n");
        free(vdso_map);
        free(obj);
      } else {
        // All clear, fill it in and notify
        complete_object(obj);
        if(verbose)
          fprintf(stderr, "[audit] Delivering synthetic objopen for vDSO...\n");
        hooks.open(&obj->entry);
      }
    }
  }

  // We ourselves won't appear in the la_objopen list, since auditors don't
  // audit themselves. Since we can appear on callstacks sometimes make sure
  // libhpcrun.so is aware of our existence.
  {
    struct link_map* self_map;
    Dl_info self_info;
    if(dladdr1(la_version, &self_info, (void**)&self_map, RTLD_DL_LINKMAP) != 0) {
      object_t* obj = malloc(sizeof *obj);
      if(!obj) abort();
      *obj = (object_t){
        .next = NULL, .prev = NULL,
        .phdrs_storage = NULL,
        .isMainExecutable = false,
        .isVDSO = false,
        .isFakeLinkMap = false,
        .entry = (auditor_map_entry_t){
          .path = NULL,
          .start = NULL, .end = NULL,
          .dl_info = (struct dl_phdr_info){
            .dlpi_addr = self_map->l_addr,
            .dlpi_name = self_map->l_name,
            .dlpi_phdr = NULL, .dlpi_phnum = 0,
          },
          .load_module = NULL,
          .map = self_map,
        },
      };

      if(!fetch_hdrs(obj)) {
        if(verbose)
          fprintf(stderr, "[audit] Error completing object for auditor `%s'...\n",
                  obj->entry.map->l_name);
        free(obj);
      } else {
        complete_object(obj);
        if(verbose)
          fprintf(stderr, "[audit] Delivering objopen for auditor `%s'...\n",
                  obj->entry.map->l_name);
        hooks.open(&obj->entry);
      }
    }
  }

  if(verbose)
    fprintf(stderr, "[audit] Auditor is now connected\n");
}



uintptr_t la_symbind32(Elf32_Sym *sym, unsigned int ndx,
                       uintptr_t *refcook, uintptr_t *defcook,
                       unsigned int *flags, const char *symname) {
  if(*refcook != 0 && dl_runtime_resolver_ptr != 0)
    optimize_object_plt((object_t*)*refcook);
  return sym->st_value;
}
uintptr_t la_symbind64(Elf64_Sym *sym, unsigned int ndx,
                       uintptr_t *refcook, uintptr_t *defcook,
                       unsigned int *flags, const char *symname) {
  if(*refcook != 0 && dl_runtime_resolver_ptr != 0)
    optimize_object_plt((object_t*)*refcook);
  return sym->st_value;
}



void la_activity(uintptr_t* cookie, unsigned int flag) {
  static unsigned int previous = LA_ACT_CONSISTENT;

  if(flag == LA_ACT_CONSISTENT) {
    if(verbose)
      fprintf(stderr, "[audit] la_activity: LA_CONSISTENT\n");

    // If we're not attached to the mainlib yet, do so.
    if(pfn_attach != NULL) {
      pfn_attach(&exports, &hooks);
      pfn_attach = NULL;
    }

    // If we're connected, let the mainlib know about the current stability
    if(connected) {
      if(verbose)
        fprintf(stderr, "[audit] Notifying stability (additive: %s)\n",
                previous == LA_ACT_ADD ? "yes" : "no");
      hooks.stable(previous == LA_ACT_ADD);
    }
  } else if(verbose) {
    if(flag == LA_ACT_ADD)
      fprintf(stderr, "[audit] la_activity: LA_ADD\n");
    else if(flag == LA_ACT_DELETE)
      fprintf(stderr, "[audit] la_activity: LA_DELETE\n");
    else
      fprintf(stderr, "[audit] la_activity: %d\n", flag);
  }

  previous = flag;
}

static void mainlib_disconnect() {
  mainlib_cookie = NULL;
  pfn_attach = NULL;
  connected = false;
  if(verbose)
    fprintf(stderr, "[audit] Auditor has now disconnected\n");
}

unsigned int la_objclose(uintptr_t* cookie) {
  if(*cookie == 0) {
    // Ignored binary, apparently things went wrong on the other side.
    return 0;
  }
  object_t* obj = (void*)*cookie;

  // If the mainlib is disappearing, forcibly disconnect.
  if(cookie == mainlib_cookie) {
    if(verbose)
      fprintf(stderr, "[audit] libhpcrun.so has closed, forcibly disconnecting.\n");
    mainlib_disconnect();
  }

  // Notify the mainlib about this disappearing binary
  if(connected) {
    if(verbose)
      fprintf(stderr, "[audit] Delivering objclose for `%s'\n",
              obj->entry.map->l_name);
    hooks.close(&obj->entry);
  }

  // If the object was in the buffer, unlink it before cleaning it up
  bool unlinked = false;
  if(obj->prev != NULL) obj->prev->next = obj->next, unlinked = true;
  else if(buffer_head == obj) buffer_head = obj->next, unlinked = true;
  if(obj->next != NULL) obj->next->prev = obj->prev, unlinked = true;
  else if(buffer_tail == obj) buffer_tail = obj->prev, unlinked = true;
  if(unlinked && verbose) {
    fprintf(stderr, "[audit] Removed from buffer: `%s'\n",
            obj->entry.map->l_name);
  }

  // Free up the memory for the entry
  free(obj);
  *cookie = 0;
  return 0;
}

