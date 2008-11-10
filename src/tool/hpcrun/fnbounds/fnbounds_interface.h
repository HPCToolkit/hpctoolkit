//
// $Id$
//

int fnbounds_init();

int fnbounds_enclosing_addr(void *addr, void **start, void **end); 

int fnbounds_note_module(const char *module_name, void *start, void *end); 
int fnbounds_module_domap(const char *module_name, void *start, void *end); 

void fnbounds_map_open_dsos();
void fnbounds_unmap_closed_dsos();

void fnbounds_epoch_finalize();
void fnbounds_fini();

void fnbounds_release_lock(void);

// support routine
int fnbounds_table_lookup(void **table, int length, void *pc, void **start, void **end);
