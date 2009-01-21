// $Id$

void csprof_pre_dlopen(const char *path, int flags);
void csprof_dlopen(const char *module_name, int flags, void *handle);
void csprof_dlclose(void *handle);
void csprof_post_dlclose(void *handle, int ret);
int  csprof_dlopen_pending();
