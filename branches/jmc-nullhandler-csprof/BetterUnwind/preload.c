#include <stdio.h>

#define GNU_SOURCE
#define __USE_GNU
#include <dlfcn.h>

extern int strlen();

void _init()
{
	printf("strlen = %p\n", dlsym(NULL, "strlen"));
{
	Dl_info foo;
	printf("dladdr(%p) = %d\n", dladdr(strlen, &foo));
	printf("fname = %s, fbase = %p, sname = %s, saddr = %p\n", foo.dli_fname, foo.dli_fbase, 
		foo.dli_sname, foo.dli_saddr);
}
	
}
void _fini()
{
}
