int 
find_dl_bound(const char *filename, void *base, void *pc, 
	      void **start, void **end);


void 
dl_fini();

void 
dl_init();

void
dl_add_module(const char *module);

