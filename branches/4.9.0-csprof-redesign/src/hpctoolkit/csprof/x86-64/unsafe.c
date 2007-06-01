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
    /* KLUDGE: somewhere along the line, binutils changed the name
       of the 'size/_cooked_size' structure field and didn't bother
       to provide any way to differentiate between the two versions.
       No abstract interface, nothing.  So if you're having compilation
       problems with this file, you might change '_cooked_size' to
       'size'.

       The reason it's '_cooked_size' is because that's what recent
       versions of binutils use and therefore it's more likely to work
       out of the box for people.  */
    /* MWF hack -- change cooked_size as per suggestion */
    plt_end_address = sect->vma + sect->_cooked_size;
#ifdef NO
    plt_end_address = sect->vma + sect->size;
#endif
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
  /* KLUDGE: Recent versions of libc include a highly optimized memcpy
     that doesn't contain any unwind information.  We need to work
     around that.  */
  memcpy_begin_address = (bfd_vma) &memcpy;
  memcpy_end_address = memcpy_begin_address + 0x900;

  return 1;
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

  MSG(1,"In x86 unsafe!");
  if(!plt_found) {
    discover_plt_addresses();
    plt_found = 1;
  }
  if(!memcpy_found) {
    csprof_epoch_t *e = csprof_get_epoch();

    if(e == NULL) {
      MSG(1,"csprof_get_epoch fails");
      return 1;
    }
    else {
      csprof_epoch_module_t *m = e->loaded_modules;

      memcpy_found = go_find_memcpy(m);

      if(!memcpy_found) {
        MSG(1,"go_find_memcpy fails");
	return 1;
      }
    }
  }
  if(!csproflib_found) {
    csprof_epoch_t *e = csprof_get_epoch();

    if(e == NULL) {
      MSG(1,"csprof_get_epoch #2 fails");
      return 1;
    }
    else {
      csprof_epoch_module_t *m = e->loaded_modules;

      csproflib_found = go_find_csproflib(m);

      if(!csproflib_found) {
        MSG(1,"go_find_csproflib fails");
	return 1;
      }
    }
  }

  /* memcpy plays games with the stack and doesn't tell anybody */
  {
    int verdict = ((csproflib_begin_address <= ip) && (ip < csproflib_end_address))
      || ((memcpy_begin_address <= ip) && (ip < memcpy_end_address))
      || ((plt_begin_address <= ip) && (ip < plt_end_address));

    MSG(1,"ip = %lx\n"
          "csproflib_b = %lx, csproflib_e = %lx\n"
          "memcpy_b = %lx, memcpy_e = %lx\n"
          "plt_b = %lx, plt_e = %lx",
                       ip,
                       csproflib_begin_address,
                       csproflib_end_address,
                       memcpy_begin_address,
                       memcpy_end_address,
                       plt_begin_address,
                       plt_end_address
        );
    MSG(1,"verdict = %d",verdict);
    return verdict;
  }
}
