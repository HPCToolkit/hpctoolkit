#ifndef _FNBOUNDS_SERVER_H_
#define _FNBOUNDS_SERVER_H_

#include <stdint.h>
#include "code-ranges.h"

void dump_file_info(const char *filename, DiscoverFnTy fn_discovery);

void system_server(DiscoverFnTy, int, int);

void syserv_add_addr(void *, long);

void syserv_add_header(int is_relocatable, uintptr_t ref_offset);

#endif  // _FNBOUNDS_SERVER_H_
