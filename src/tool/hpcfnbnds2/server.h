#ifndef _FNBOUNDS_SERVER_H_
#define _FNBOUNDS_SERVER_H_

#include <stdint.h>
#include "code-ranges.h"
#include "function-entries.h"
#include "syserv-mesg.h"

void	init_server(DiscoverFnTy, int, int);
static	void	do_query(DiscoverFnTy , struct syserv_mesg *);
static	void	signal_handler_init();
static	int	read_all(int, void*, size_t);
static	int	write_all(int, const void*, size_t);
static	int	read_mesg(struct syserv_mesg *mesg);
static	int	write_mesg(int32_t type, int64_t len);
static	void	signal_handler(int);

#if 0
void syserv_add_addr(void *, long);
void syserv_add_header(int is_relocatable, uintptr_t ref_offset);
#endif

#endif  // _FNBOUNDS_SERVER_H_
