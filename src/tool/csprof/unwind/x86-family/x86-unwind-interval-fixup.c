#include <stdlib.h>
#include "x86-unwind-interval-fixup.h"

typedef struct x86_ui_fixup_list_item_s {
  x86_ui_fixup_fn_t fn;
  struct x86_ui_fixup_list_item_s *next;
} x86_ui_fixup_list_item_t; 

static x86_ui_fixup_list_item_t *x86_fixup_list = 0;


void 
add_x86_unwind_interval_fixup_function(x86_ui_fixup_fn_t fn)
{
   x86_ui_fixup_list_item_t *i = (x86_ui_fixup_list_item_t *) malloc(sizeof(x86_ui_fixup_list_item_t));
   i->next = x86_fixup_list;
   i->fn = fn;
   x86_fixup_list = i;
}

int 
x86_fix_unwind_intervals(char *ins, int len, interval_status *stat)
{
   x86_ui_fixup_list_item_t *fentry = x86_fixup_list;
   while (fentry) {
    if ((*fentry->fn)(ins,len,stat)) return 1;
    fentry = fentry->next;
   }
   return 0;
}
