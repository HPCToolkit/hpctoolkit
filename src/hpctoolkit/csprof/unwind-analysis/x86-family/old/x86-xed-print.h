#ifndef x86_xed_print_h
#define x86_xed_print_h

#include "xed-interface.h"

void print_flags(xed_decoded_inst_t* xptr); 
void print_memops(xed_decoded_inst_t* xptr); 
void print_operands(xed_decoded_inst_t* xptr); 

#endif
