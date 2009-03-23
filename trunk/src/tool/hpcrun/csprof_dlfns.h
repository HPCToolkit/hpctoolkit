// $Id$

#ifndef _CSPROF_DLFNS_H_
#define _CSPROF_DLFNS_H_

int  csprof_dlopen_read_lock(void);
void csprof_dlopen_read_unlock(void);

void csprof_pre_dlopen(const char *path, int flags);
void csprof_dlopen(const char *module_name, int flags, void *handle);
void csprof_dlclose(void *handle);
void csprof_post_dlclose(void *handle, int ret);

#endif
