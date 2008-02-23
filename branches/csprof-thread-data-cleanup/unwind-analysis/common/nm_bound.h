#ifndef nm_bound_h
#define nm_bound_h

int 
nm_tab_bound(void **table, int length, unsigned long pc, 
	     void **start, void **end);

int 
nm_bound(unsigned long pc, void **start, void **end);

#endif
