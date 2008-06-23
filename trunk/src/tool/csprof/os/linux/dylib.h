#ifndef dylib_h
#define dylib_h

void dylib_map_open_dsos();

void dylib_map_executable();

int dylib_addr_is_mapped(unsigned long long addr) ;


// tallent: FIXME(double check): why not use a void* or uintptr_t instead of a 'long long'?  The latter tends to only bring trouble during ports.
int dylib_find_module_containing_addr(unsigned long long addr, 
				      // output parameters
				      char *module_name,
				      unsigned long long *start, 
				      unsigned long long *end);

#endif
