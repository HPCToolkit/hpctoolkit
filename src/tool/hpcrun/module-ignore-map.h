#ifndef _HPCTOOLKIT_MODULE_IGNORE_MAP_H_
#define _HPCTOOLKIT_MODULE_IGNORE_MAP_H_

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdint.h>


/******************************************************************************
 * local includes
 *****************************************************************************/

#include <hpcrun/loadmap.h>

/******************************************************************************
 * type definitions 
 *****************************************************************************/

typedef bool (*module_ignore_fn_t)(load_module_t* module);

typedef struct module_ignore_fn_entry {
  struct module_ignore_fn_entry* next;
  module_ignore_fn_t fn;
  load_module_t* module;
} module_ignore_fn_entry_t;

typedef struct module_ignore_map_entry_s module_ignore_map_entry_t;


/******************************************************************************
 * interface operations
 *****************************************************************************/

module_ignore_map_entry_t *module_ignore_map_lookup(load_module_t *module);

void module_ignore_map_insert(load_module_t *module);

bool module_ignore_map_ignore(load_module_t *module);

void module_ignore_map_register(module_ignore_fn_entry_t *fn);

bool module_ignore_map_refcnt_update(load_module_t *module, int val);

uint64_t module_ignore_map_entry_refcnt_get(module_ignore_map_entry_t *entry);

#endif

