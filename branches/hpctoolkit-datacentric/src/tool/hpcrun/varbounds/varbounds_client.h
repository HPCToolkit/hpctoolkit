#ifndef _VARBOUNDS_CLIENT_H_
#define _VARBOUNDS_CLIENT_H_

#include "varbounds_file_header.h"

int  hpcrun_syserv_var_init(void);

void hpcrun_syserv_var_fini(void);

void *hpcrun_syserv_var_query(const char *fname, struct varbounds_file_header *fh);

#endif  // _VARBOUNDS_CLIENT_H_
