// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//******************************************************************************
// global includes
//******************************************************************************

#define _GNU_SOURCE

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
#include <signal.h>


//******************************************************************************
// local includes
//******************************************************************************

#include "audit-api.h"

#include "binding.h"


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
static bool disable_plt_call_opt = false;
static ElfW(Addr) dl_runtime_resolver_ptr = 0;

static object_t* buffer_head = NULL;
static object_t* buffer_tail = NULL;

static const auditor_hooks_t* hooks;

static void mainlib_connected(const char*, const auditor_hooks_t*);
static void mainlib_disconnect();

typedef int (*pfn_iterate_phdr_t)(int (*callback)(struct dl_phdr_info*, size_t, void*), void* data);
extern pfn_iterate_phdr_t hpcrun_iterate_phdr;


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
  if (obj->entry.dl_info.dlpi_phdr != NULL)
    abort();
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
  if (obj->entry.dl_info.dlpi_phnum == 0)
    abort();
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

__attribute__((visibility("default")))
unsigned int la_version(unsigned int version) {
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

  return LAV_CURRENT;
}


__attribute__((visibility("default")))
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

  // Fetch the program headers, we *always* need them. If we can't get them
  // drop the binary on the floor and ignore it completely.
  if(!fetch_hdrs(obj)) {
    free(obj);
    *cookie = 0;
    return LA_FLG_BINDFROM | LA_FLG_BINDTO;
  }

  // If we're connected finalize and notify the new object, otherwise buffer it
  // and wait until we're connected.
  if(hooks != NULL) {
    complete_object(obj);
    if(verbose)
      fprintf(stderr, "[audit] Delivering objopen for `%s' [%p, %p)\n",
              map->l_name, obj->entry.start, obj->entry.end);
    hooks->open(&obj->entry);
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


static void mainlib_connected(const char* vdso_path, const auditor_hooks_t* new_hooks) {
  // No need to execute this code in a forked child without exec
  if (hooks != NULL) return;

  hooks = new_hooks;
  hpcrun_iterate_phdr = hooks->dl_iterate_phdr;

  // Finalize and deliver notifications for all the objects we've buffered
  if(verbose)
    fprintf(stderr, "[audit] libhpcrun.so is connected, draining buffered objopens...\n");
  bool foundVDSO = false;
  while(buffer_head != NULL) {
    // Unlink the front object from the buffer
    object_t* obj = buffer_head;
    buffer_head = obj->next;
    obj->prev = obj->next = NULL;

    if(obj->isVDSO) {
      foundVDSO = true;
      if(obj->entry.path == NULL)
        obj->entry.path = (char*)vdso_path;
    }

    // Finalize and notify
    complete_object(obj);
    if(verbose) {
      fprintf(stderr, "[audit] Delivering buffered objopen for `%s'\n",
              obj->entry.path);
    }
    hooks->open(&obj->entry);
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
        hooks->open(&obj->entry);
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
        hooks->open(&obj->entry);
      }
    }
  }

  if(verbose)
    fprintf(stderr, "[audit] Auditor is now connected\n");
}

static const auditor_exports_t* hpcrun_connect_to_auditor_p() {
  static const auditor_exports_t exports = {
    .mainlib_connected = mainlib_connected,
    .mainlib_disconnect = mainlib_disconnect,
    .hpcrun_bind_v = hpcrun_bind_v,
    .exit = exit,
    .sigprocmask = sigprocmask, .pthread_sigmask = pthread_sigmask,
    .sigaction = sigaction,
  };
  return &exports;
}

__attribute__((visibility("default")))
uintptr_t la_symbind32(Elf32_Sym *sym, unsigned int ndx,
                       uintptr_t *refcook, uintptr_t *defcook,
                       unsigned int *flags, const char *symname) {
  if(*refcook != 0 && dl_runtime_resolver_ptr != 0)
    optimize_object_plt((object_t*)*refcook);
  if(strcmp(symname, "hpcrun_connect_to_auditor") == 0)
    return (uintptr_t)&hpcrun_connect_to_auditor_p;
  return sym->st_value;
}
__attribute__((visibility("default")))
uintptr_t la_symbind64(Elf64_Sym *sym, unsigned int ndx,
                       uintptr_t *refcook, uintptr_t *defcook,
                       unsigned int *flags, const char *symname) {
  if(*refcook != 0 && dl_runtime_resolver_ptr != 0)
    optimize_object_plt((object_t*)*refcook);
  if(strcmp(symname, "hpcrun_connect_to_auditor") == 0)
    return (uintptr_t)&hpcrun_connect_to_auditor_p;
  return sym->st_value;
}



__attribute__((visibility("default")))
void la_activity(uintptr_t* cookie, unsigned int flag) {
  static unsigned int previous = LA_ACT_CONSISTENT;

  if(flag == LA_ACT_CONSISTENT) {
    if(verbose)
      fprintf(stderr, "[audit] la_activity: LA_CONSISTENT\n");

    // If we're connected, let the mainlib know about the current stability
    if(hooks != NULL) {
      if(verbose)
        fprintf(stderr, "[audit] Notifying stability (additive: %s)\n",
                previous == LA_ACT_ADD ? "yes" : "no");
      hooks->stable(previous == LA_ACT_ADD);
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
  hooks = NULL;
  if(verbose)
    fprintf(stderr, "[audit] Auditor has now disconnected\n");
}

__attribute__((visibility("default")))
unsigned int la_objclose(uintptr_t* cookie) {
  if(*cookie == 0) {
    // Ignored binary, apparently things went wrong on the other side.
    return 0;
  }
  object_t* obj = (void*)*cookie;

  // Notify the mainlib about this disappearing binary
  if(hooks != NULL) {
    if(verbose)
      fprintf(stderr, "[audit] Delivering objclose for `%s'\n",
              obj->entry.map->l_name);
    hooks->close(&obj->entry);
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
