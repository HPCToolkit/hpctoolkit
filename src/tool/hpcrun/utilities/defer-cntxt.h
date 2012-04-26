#ifndef defer_cntxt_h
#define defer_cntxt_h

/*
 * this interface is used for lazy callpath resolution 
*/

/* register the functions in runtime (OMP) to support lazy resolution */
void register_callback();

/* check whether the lazy resolution is needed */
int  need_defer_cntxt();

/* resolve the contexts */
void resolve_cntxt();
void resolve_cntxt_fini();
void resolve_other_cntxt(bool fini_flag);

#endif
