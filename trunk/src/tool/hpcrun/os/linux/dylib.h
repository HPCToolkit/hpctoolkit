#ifndef dylib_h
#define dylib_h

void dylib_map_open_dsos();

void dylib_map_executable();

int dylib_addr_is_mapped(void *addr);


// tallent: FIXME(double check): why not use a void* or uintptr_t instead of a 'long long'?  The latter tends to only bring trouble during ports.
int dylib_find_module_containing_addr(void *addr, 
				      // output parameters
				      char *module_name,
				      void **start, 
				      void **end);

#endif
