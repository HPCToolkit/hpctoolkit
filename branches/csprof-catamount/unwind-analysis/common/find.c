#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#ifndef STATIC_ONLY
#include <dlfcn.h>
#endif

#include "pmsg.h"

extern int nm_bound(unsigned long pc, unsigned long *st, unsigned long *e);

#include "find.h"

#ifndef STATIC_ONLY

#define USE_FDE 1
#include <unwind-dw2-fde.h>

static char *find_dwarf_end_addr(char *addr, struct dwarf_fde const *start_fde,
				 struct dwarf_eh_bases *start_bases) 
{
  struct dwarf_eh_bases end_bases;
  struct dwarf_eh_bases new_bases;
  struct dwarf_fde const *other_fde;
  unsigned int offset, mine, other;
  int mid;

  offset = 1;
  do {
    other_fde = _Unwind_Find_FDE(addr + offset, &end_bases);
    PMSG(FIND,"FIND:testing offset = %d, end = %p", offset, end_bases.func);
    offset <<= 1;
  } while (other_fde && (end_bases.func == start_bases->func));


  other = offset >> 1;
  mine = other >> 1;

  for (;;) {
    struct dwarf_fde const *new_fde;
    mid = (other - mine) >> 1;
    if (mid == 0) break;
    new_fde = _Unwind_Find_FDE(addr + mine + mid, &new_bases);
    PMSG(FIND,"testing offset = %d, end = %p", mine + mid, new_bases.func);
    if (new_bases.func == start_bases->func) mine += mid;
    else other -= mid;
  }
  PMSG(FIND,"finishing with offset = %d, addr + other = %p, end = %p", 
       other, addr + other, new_bases.func);
  return (char *) addr + other; // new_bases.func;
}


static char *find_dl_end_addr(char *addr, Dl_info *start)
{
  Dl_info end;
  Dl_info otheri;
  unsigned int offset, mine, other, res;


  offset = 1;
  do {
    res = dladdr(addr + offset, &end);
    offset <<= 1;
    PMSG(FIND,"testing offset = %d, end = %p", offset, (void *) end.dli_saddr);
  } while ((res == 0) && start->dli_saddr == end.dli_saddr);


  other = offset >> 1;
  mine = other >> 1;

  for (;;) {
    int mid = (other - mine) >> 1;
    if (mid == 0) break;
    res = dladdr(addr + mine + mid, &otheri);
    PMSG(FIND,"testing offset = %d, end = %p", 
	 mine + mid, (void *) otheri.dli_saddr);
    if (otheri.dli_saddr == start->dli_saddr) mine += mid;
    else other -= mid;
  }
  PMSG(FIND,"finishing offset = %d, end = %p", other, addr + other);
  return addr + other;
}
#endif

int 
find_enclosing_function_bounds_v(char *addr, char **start, char **end, 
				 int verbose) 
{
  // debug = 1;

  int failure = 0;
  struct dwarf_eh_bases base;
#ifndef STATIC_ONLY
  struct dwarf_fde const *fde = _Unwind_Find_FDE(addr, &base);
#endif
  int nmb = nm_bound((unsigned long)addr,(unsigned long *)start,
		     (unsigned long *)end);

  if (nmb){
    PMSG(FIND,"FIND:found in nm table for %p: start=%p,end=%p",addr, *start, *end);
  } else if (fde && base.func) {
    *start = (char *) base.func;
    *end = find_dwarf_end_addr(addr, fde, &base);
    PMSG(FIND,"FIND:looked up address %p, fde = %p, " 
	 "found func addr = %p, end addr = %p",
	 addr, fde, *start, *end);
#ifndef STATIC_ONLY
  } else {
    Dl_info info;
    if (dladdr(addr, &info) && info.dli_saddr) {
      *start = (char *) info.dli_saddr;
      *end = find_dl_end_addr(addr, &info);
      PMSG(FIND,"FIND:looked up address using dladdr %p, "
	   "found func addr = %p, end = %p",
	   addr, *start, *end);
    } else {
      if (verbose){
	PMSG(FIND, "FIND:look up failed @ %p!",addr);
      }
      failure = 1;
    }
#endif // STATIC_ONLY
  }
  return failure;
}


#if 0
void test_find_enclosing_function_bounds(char *addr) 
{
  char *start, *end;
  debug = 1;
  find_enclosing_function_bounds(addr, &start, &end);
  printf("found enclosing function: addr = %p, start = %p, end = %p\n", 
	 addr, start, end);
  debug = 0;
}
#endif
