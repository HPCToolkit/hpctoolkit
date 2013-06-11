#ifndef _FNBOUNDS_CLIENT_H_
#define _FNBOUNDS_CLIENT_H_

#include "fnbounds_file_header.h"

int  hpcrun_syserv_init(void);

void hpcrun_syserv_fini(void);

void *hpcrun_syserv_query(const char *fname, struct fnbounds_file_header *fh);

#endif  // _FNBOUNDS_CLIENT_H_
