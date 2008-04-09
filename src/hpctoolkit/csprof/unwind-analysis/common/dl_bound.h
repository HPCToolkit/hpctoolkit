int 
find_dl_bound(void *pc, void **start, void **end);


void 
dl_fini();

void 
dl_init();

void
dl_add_module(const char *module);

void dl_add_module_base(const char *module_name, void *start, void *end);
