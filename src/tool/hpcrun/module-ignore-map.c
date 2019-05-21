#include "module-ignore-map.h"
#include <fcntl.h>   // open
#include <dlfcn.h>  // dlopen
#include <limits.h>  // PATH_MAX

#include <monitor.h>  // PATH_MAX

#include <hpcrun/loadmap.h>
#include <lib/prof-lean/pfq-rwlock.h>

#define NUM_FNS 3
static const char *NVIDIA_FNS[NUM_FNS] = {
  "cuLaunchKernel", "cudaLaunchKernel", "cuptiActivityEnable"
};

#define MODULE_IGNORE_DEBUG 0

#if MODULE_IGNORE_DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif


typedef struct module_ignore_entry {
  bool empty;
  load_module_t *module;
} module_ignore_entry_t;

static module_ignore_entry_t modules[NUM_FNS];
static pfq_rwlock_t modules_lock;


static bool
lm_contains_fn(const char *lm, const char *fn) {
  char resolved_path[PATH_MAX];
  bool load_handle = false;

  char *lm_real = realpath(lm, resolved_path);
  void *handle = monitor_real_dlopen(lm_real, RTLD_NOLOAD);
  // At the start of a program, libs are not loaded by dlopen
  if (handle == NULL) {
    handle = monitor_real_dlopen(lm_real, RTLD_LAZY);
    load_handle = handle ? true : false;
  }
  PRINT("query path = %s\n", lm_real);
  PRINT("query fn = %s\n", fn);
  PRINT("handle = %p\n", handle);
  bool result = false;
  if (handle) {
    void *fp = dlsym(handle, fn);
    PRINT("fp = %p\n", fp);
    if (fp) {
      Dl_info dlinfo;
      int res = dladdr(fp, &dlinfo);
      if (res) {
        char dli_fname_buf[PATH_MAX];
        char *dli_fname = realpath(dlinfo.dli_fname, dli_fname_buf);
        result = (strcmp(lm_real, dli_fname) == 0);
        PRINT("original path = %s\n", dlinfo.dli_fname);
        PRINT("found path = %s\n", dli_fname);
        PRINT("symbol = %s\n", dlinfo.dli_sname);
      }
    }
  }
  // Close the handle it is opened here
  if (load_handle) {
    monitor_real_dlclose(handle);
  }
  return result;
}


void
module_ignore_map_init()
{
  size_t i;
  for (i = 0; i < NUM_FNS; ++i) {
    modules[i].empty = true;
    modules[i].module = NULL;
  }
  pfq_rwlock_init(&modules_lock);
}


bool
module_ignore_map_module_id_lookup(uint16_t module_id)
{
  // Read path
  size_t i;
  bool result = false;
  pfq_rwlock_read_lock(&modules_lock);
  for (i = 0; i < NUM_FNS; ++i) {
    if (modules[i].empty == false && modules[i].module->id == module_id) {
      /* current module should be ignored */
      result = true;
      break;
    }
  }
  pfq_rwlock_read_unlock(&modules_lock);
  return result;
}


bool
module_ignore_map_module_lookup(load_module_t *module)
{
  return module_ignore_map_lookup(module->dso_info->start_addr, module->dso_info->end_addr);
}


bool
module_ignore_map_inrange_lookup(void *addr)
{
  return module_ignore_map_lookup(addr, addr);
}


bool
module_ignore_map_lookup(void *start, void *end)
{
  // Read path
  size_t i;
  bool result = false;
  pfq_rwlock_read_lock(&modules_lock);
  for (i = 0; i < NUM_FNS; ++i) {
    if (modules[i].empty == false &&
      modules[i].module->dso_info->start_addr <= start &&
      modules[i].module->dso_info->end_addr >= end) {
      /* current module should be ignored */
      result = true;
      break;
    }
  }
  pfq_rwlock_read_unlock(&modules_lock);
  return result;
}


bool
module_ignore_map_ignore(void *start, void *end)
{
  // Update path
  // Only one thread could update the flag,
  // Guarantee dlopen modules before notification are updated.
  size_t i;
  bool result = false;
  pfq_rwlock_node_t me;
  pfq_rwlock_write_lock(&modules_lock, &me);
  for (i = 0; i < NUM_FNS; ++i) {
    if (modules[i].empty == true) {
      load_module_t *module = hpcrun_loadmap_findByAddr(start, end);
      if (lm_contains_fn(module->name, NVIDIA_FNS[i])) {
        modules[i].module = module;
        modules[i].empty = false;
        result = true;
        break;
      }
    }
  }
  pfq_rwlock_write_unlock(&modules_lock, &me);
  return result;
}


bool
module_ignore_map_delete(void *start, void *end)
{
  size_t i;
  bool result = false;
  pfq_rwlock_node_t me;
  pfq_rwlock_write_lock(&modules_lock, &me);
  for (i = 0; i < NUM_FNS; ++i) {
    if (modules[i].empty == false &&
      modules[i].module->dso_info->start_addr <= start &&
      modules[i].module->dso_info->end_addr >= end) {
      modules[i].empty = true;
      result = true;
      break;
    }
  }
  pfq_rwlock_write_unlock(&modules_lock, &me);
  return result;
}
