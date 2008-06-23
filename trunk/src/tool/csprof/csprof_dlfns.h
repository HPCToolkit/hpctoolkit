void csprof_pre_dlopen();
int csprof_dlopen_pending();
void csprof_dlopen(const char *module_name, int flags, void *handle);
void csprof_dlclose(void *handle);
