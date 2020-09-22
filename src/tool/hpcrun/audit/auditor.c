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
// Copyright ((c)) 2002-2020, Rice University
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "audit-api.h"

#include <sys/stat.h>
#include <sys/auxv.h>
#include <sys/wait.h>
#include <link.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sched.h>

static bool verbose = false;
static char* mainlib = NULL;

enum hpcrun_state {
  state_awaiting, state_found, state_attached,
  state_connecting, state_connected, state_disconnected,
};
static enum hpcrun_state state = state_awaiting;

static auditor_hooks_t hooks;
static auditor_attach_pfn_t pfn_init = NULL;
static uintptr_t* mainlib_cookie = NULL;

static void mainlib_connected();
static auditor_exports_t exports = {
  .mainlib_connected = mainlib_connected,
  .pipe = pipe, .close = close, .waitpid = waitpid,
  .clone = clone, .execve = execve,
};

struct buffered_entry_t {
  struct link_map* map;
  Lmid_t lmid;
  uintptr_t* cookie;

  struct buffered_entry_t* next;
} *buffer = NULL;

unsigned int la_version(unsigned int version) {
  if(version < 1) return 0;

  // Read in our arguments
  verbose = getenv("HPCRUN_AUDIT_DEBUG");

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

// Helper to call the open hook, when you only have the link_map.
static void hook_open(uintptr_t* cookie, struct link_map* map) {
  // Allocate some space for our extra bits, and fill it.
  auditor_map_entry_t* entry = malloc(sizeof *entry);
  entry->map = map;

  // Normally the path is map->l_name, but sometimes that string is empty
  // which indicates the main executable. So we get it the other way.
  entry->path = NULL;
  if(map->l_name[0] == '\0')
    entry->path = realpath((const char*)getauxval(AT_EXECFN), NULL);
  else
    entry->path = realpath(map->l_name, NULL);

  // Find the phdrs for this here binary. Depending on bits it can be tricky.
  Elf32_Phdr* phdr32 = NULL;
  Elf64_Phdr* phdr64 = NULL;
  size_t phnum = 0;
  if(map->l_addr == 0) {
    phnum = getauxval(AT_PHNUM);
    switch(getauxval(AT_PHENT)) {
    case sizeof(Elf32_Phdr):
      phdr32 = (void*)getauxval(AT_PHDR);
      break;
    case sizeof(Elf64_Phdr):
      phdr64 = (void*)getauxval(AT_PHDR);
      break;
    default: abort();
    }
    entry->ehdr = NULL;
  } else {
    switch(((char*)map->l_addr)[EI_CLASS]) {
    case ELFCLASS32: {
      Elf32_Ehdr* hdr = (void*)(uintptr_t)map->l_addr;
      phdr32 = (void*)(uintptr_t)map->l_addr + hdr->e_phoff;
      phnum = hdr->e_phnum;
      break;
    }
    case ELFCLASS64: {
      Elf64_Ehdr* hdr = (void*)(uintptr_t)map->l_addr;
      phdr64 = (void*)(uintptr_t)map->l_addr + hdr->e_phoff;
      phnum = hdr->e_phnum;
      break;
    }
    default: abort();
    }
    entry->ehdr = (void*)map->l_addr;
  }

  // Use the phdrs to calculate the range of executable bits as well as the
  // real base address.
  uintptr_t start = UINTPTR_MAX;
  uintptr_t end = 0;
  for(size_t i = 0; i < phnum; i++) {
    if(phdr32 != NULL) {
      if(phdr32[i].p_type == PT_LOAD) {
        if(!entry->ehdr)
          entry->ehdr = (void*)(uintptr_t)(phdr32[i].p_vaddr - phdr32[i].p_offset);
        if((phdr32[i].p_flags & PF_X) != 0 && phdr32[i].p_memsz > 0) {
          if(phdr32[i].p_vaddr < start) start = phdr32[i].p_vaddr;
          if(phdr32[i].p_vaddr + phdr32[i].p_memsz > end)
            end = phdr32[i].p_vaddr + phdr32[i].p_memsz;
        }
      } else if(phdr64[i].p_type == PT_DYNAMIC && map->l_ld == NULL) {
        map->l_ld = (void*)(map->l_addr + phdr32[i].p_vaddr);
      }
    } else if(phdr64 != NULL) {
      if(phdr64[i].p_type == PT_LOAD) {
        if(!entry->ehdr)
          entry->ehdr = (void*)(uintptr_t)(phdr64[i].p_vaddr - phdr64[i].p_offset);
        if((phdr64[i].p_flags & PF_X) != 0 && phdr64[i].p_memsz > 0) {
          if(phdr64[i].p_vaddr < start) start = phdr64[i].p_vaddr;
          if(phdr64[i].p_vaddr + phdr64[i].p_memsz > end)
            end = phdr64[i].p_vaddr + phdr64[i].p_memsz;
        }
      } else if(phdr64[i].p_type == PT_DYNAMIC && map->l_ld == NULL) {
        map->l_ld = (void*)(map->l_addr + phdr64[i].p_vaddr);
      }
    } else abort();
  }
  entry->start = (void*)map->l_addr + start;
  entry->end = (void*)map->l_addr + end;

  // Since we don't use dl_iterate_phdr, we have to reconsitute its data.
  entry->dl_info.dlpi_addr = (ElfW(Addr))map->l_addr;
  entry->dl_info.dlpi_name = entry->path;
  entry->dl_info.dlpi_phdr = phdr32 ? (void*)phdr32 : (void*)phdr64;
  entry->dl_info.dlpi_phnum = phnum;
  entry->dl_info.dlpi_adds = 0;
  entry->dl_info.dlpi_subs = 0;
  entry->dl_info.dlpi_tls_modid = 0;
  entry->dl_info.dlpi_tls_data = NULL;
  entry->dl_info_sz = offsetof(struct dl_phdr_info, dlpi_phnum)
                      + sizeof entry->dl_info.dlpi_phnum;

  if(verbose)
    fprintf(stderr, "[audit] Delivering objopen for `%s'\n", entry->path);
  hooks.open(entry);
  if(cookie)
    *cookie = (uintptr_t)entry;
  else free(entry);
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

unsigned int la_objopen(struct link_map* map, Lmid_t lmid, uintptr_t* cookie) {
  switch(state) {
  case state_awaiting: {
    // If this is libhpcrun.so, nab the initialization bits and transition.
    char* path = realpath(map->l_name, NULL);
    if(path) {
      if(strcmp(path, mainlib) == 0) {
        if(verbose)
          fprintf(stderr, "[audit] Located tracker library.\n");
        free(mainlib);
        mainlib_cookie = cookie;
        pfn_init = get_symbol(map, "hpcrun_auditor_attach");
        if(verbose)
          fprintf(stderr, "[audit] Found init hook: %p\n", pfn_init);
        state = state_found;
      }
      free(path);
    }
    // fallthrough
  }
  case state_found:
  case state_attached:
  case state_connecting: {
    // Buffer operations that happen before the connection
    struct buffered_entry_t* entry = malloc(sizeof *entry);
    if(verbose)
      fprintf(stderr, "[audit] Buffering objopen before connection: `%s'\n", map->l_name);
    *entry = (struct buffered_entry_t){
      .map = map, .lmid = lmid, .cookie = cookie, .next = buffer,
    };
    buffer = entry;
    return 0;
  }
  case state_connected:
    // If we're already connected, just call the hook.
    hook_open(cookie, map);
    return 0;
  case state_disconnected:
    // We just ignore things that happen after disconnection.
    if(verbose)
      fprintf(stderr, "[audit] objopen after disconnection: `%s'\n", map->l_name);
    return 0;
  }
  abort();  // unreachable
}

// Transition from connecting to connected, once the mainlib is ready.
static void mainlib_connected(const char* vdso_path) {
  if(state != state_connecting) return;

  // Reverse the stack of buffered notifications, so they get reported in order.
  struct buffered_entry_t* queue = NULL;
  while(buffer != NULL) {
   struct buffered_entry_t* next = buffer->next;
   buffer->next = queue;
   queue = buffer;
   buffer = next;
  }

  // Drain the buffer and deliver everything that happened before time begins.
  if(verbose)
    fprintf(stderr, "[audit] Draining buffered objopens\n");
  while(queue != NULL) {
    hook_open(queue->cookie, queue->map);
    struct buffered_entry_t* next = queue->next;
    free(queue);
    queue = next;
  }

  // Try to get our own linkmap, and let the mainlib know about us.
  // Obviously there is no cookie, we never notify a close from these.
  struct link_map* map;
  Dl_info info;
  dladdr1(la_version, &info, (void**)&map, RTLD_DL_LINKMAP);
  hook_open(NULL, map);

  // Add an entry for vDSO, because we don't get it otherwise.
  uintptr_t vdso = getauxval(AT_SYSINFO_EHDR);
  if(vdso != 0) {
    struct link_map* mvdso = malloc(sizeof *mvdso);
    mvdso->l_addr = vdso;
    mvdso->l_name = vdso_path ? (char*)vdso_path : "[vdso]";
    mvdso->l_ld = NULL;  // NOTE: Filled by hook_open
    mvdso->l_next = mvdso->l_prev = NULL;
    hook_open(NULL, mvdso);
  }

  if(verbose)
    fprintf(stderr, "[audit] Auditor is now connected\n");
  state = state_connected;
}

static unsigned int previous = LA_ACT_CONSISTENT;
void la_activity(uintptr_t* cookie, unsigned int flag) {
  // If we've hit consistency and know where libhpcrun is, initialize it.
  if(flag == LA_ACT_CONSISTENT) {
    switch(state) {
    case state_awaiting:
      break;
    case state_found:
      if(verbose)
        fprintf(stderr, "[audit] Attaching to mainlib\n");
      pfn_init(&exports, &hooks);
      state = state_attached;
      break;
    case state_attached: {
      if(verbose)
        fprintf(stderr, "[audit] Beginning early initialization\n");
      state = state_connecting;
      hooks.initialize();
      mainlib_connected(NULL);
      break;
    }
    case state_connecting:
      // The mainlib is still initializing, we can skip this notification.
      break;
    case state_connected:
      if(verbose)
        fprintf(stderr, "[audit] Notifying stability (additive: %d)\n",
                previous == LA_ACT_ADD);
      hooks.stable(previous == LA_ACT_ADD);
      break;
    case state_disconnected:
      break;
    }
  }
  previous = flag;
}

void la_preinit(uintptr_t* cookie) {
  if(state == state_attached) {
    if(verbose)
      fprintf(stderr, "[audit] Beginning late initialization\n");
    state = state_connecting;
    hooks.initialize();
    mainlib_connected(NULL);
  }
}

unsigned int la_objclose(uintptr_t* cookie) {
  switch(state) {
  case state_awaiting:
  case state_found:
  case state_attached:
  case state_connecting:
    // Scan through the buffer for the matching entry, and remove it.
    for(struct buffered_entry_t *e = buffer, *p = NULL; e != NULL;
        p = e, e = e->next) {
      if(e->cookie == cookie) {
        if(verbose)
          fprintf(stderr, "[audit] Buffering objclose before connection: `%s'\n",
                  e->map->l_name);
        if(p != NULL) p->next = e->next;
        else buffer = e->next;
        free(e);
        break;
      }
    }
    break;
  case state_connected:
    if(mainlib_cookie == cookie) state = state_disconnected;
    else if(*cookie != 0) {
      auditor_map_entry_t* entry = *(void**)cookie;
      if(verbose)
        fprintf(stderr, "[audit] Delivering objclose for `%s'\n", entry->path);
      hooks.close(entry);
      free(entry);
    }
    break;
  case state_disconnected:
    // We just ignore things that happen after disconnection.
    break;
  }
  return 0;
}
