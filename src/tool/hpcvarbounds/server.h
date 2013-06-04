#ifndef _VARBOUNDS_SERVER_H_
#define _VARBOUNDS_SERVER_H_

#include <stdint.h>

void dump_file_info(const char *filename);

void system_server(int, int);

void syserv_add_addr(void *, long);

void syserv_add_header(int is_relocatable, uintptr_t ref_offset);

#endif  // _VARBOUNDS_SERVER_H_
