#include <sys/types.h>
#include <unistd.h>

#include <bfd.h>

#include "general.h"
#include "interface.h"
#include "epoch.h"

static bfd_vma plt_begin_address;
static bfd_vma plt_end_address;

static void
bfdsect_callback(bfd *thebfd, asection *sect, void *obj)
{
  if(strcmp(sect->name, ".plt") == 0) {
    plt_begin_address = sect->vma;
    plt_end_address = sect->vma + sect->size;
  }
}

static void
discover_plt_addresses()
{
    char exe_file[PATH_MAX+1];
    pid_t mypid = getpid();
    bfd *thebfd;
    char **matching;
    asection *thesect;

    sprintf(exe_file, "/proc/%d/exe", mypid);

    bfd_init();

    thebfd = bfd_openr(exe_file, "default");

    if(!bfd_check_format_matches(thebfd, bfd_object, &matching)) {
      DIE("Could not determine start of .plt section",
	  __FILE__, __LINE__);
    }

    bfd_map_over_sections(thebfd, bfdsect_callback, NULL);

    bfd_close(thebfd);
}

static bfd_vma memcpy_begin_address;
static bfd_vma memcpy_end_address;

static int
go_find_memcpy(csprof_epoch_module_t *m)
{
  while(m) {
    if(strstr(m->module_name, "libc-2.3.6.so")) {
      unsigned long base = (unsigned long)m->mapaddr;
      memcpy_begin_address = base + 0x71850;
      memcpy_end_address = base + 0x72160;
      return 1;
    }
    m = m->next;
  }

  return 0;
}

static bfd_vma csproflib_begin_address;
static bfd_vma csproflib_end_address;

static int
go_find_csproflib(csprof_epoch_module_t *m)
{
  while(m) {
    if(strstr(m->module_name, "libcsprof.so.1")) {
      unsigned long base = (unsigned long)m->mapaddr;
      csproflib_begin_address = base;
      csproflib_end_address = base + m->size;
      return 1;
    }
    m = m->next;
  }

  return 0;
}

int
csprof_context_is_unsafe(void *context)
{
  static int plt_found = 0;
  static int memcpy_found = 0;
  static int csproflib_found = 0;

  struct ucontext *ctx = (struct ucontext *) context;
  greg_t ip = ctx->uc_mcontext.gregs[REG_RIP];

  if(!plt_found) {
    discover_plt_addresses();
    plt_found = 1;
  }
  if(!memcpy_found) {
    csprof_epoch_t *e = csprof_get_epoch();

    if(e == NULL) {
      return 1;
    }
    else {
      csprof_epoch_module_t *m = e->loaded_modules;

      memcpy_found = go_find_memcpy(m);

      if(!memcpy_found) {
	return 1;
      }
    }
  }
  if(!csproflib_found) {
    csprof_epoch_t *e = csprof_get_epoch();

    if(e == NULL) {
      return 1;
    }
    else {
      csprof_epoch_module_t *m = e->loaded_modules;

      csproflib_found = go_find_csproflib(m);

      if(!csproflib_found) {
	return 1;
      }
    }
  }

  /* memcpy plays games with the stack and doesn't tell anybody */
  {
    int verdict = ((csproflib_begin_address <= ip) && (ip < csproflib_end_address))
      || ((memcpy_begin_address <= ip) && (ip < memcpy_end_address))
      || ((plt_begin_address <= ip) && (ip < plt_end_address));

    return verdict;
  }
}
