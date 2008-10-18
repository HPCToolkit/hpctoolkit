/* unsafe.h -- determining if a signal context is safe to unwind */
#ifndef CSPROF_UNSAFE_H
#define CSPROF_UNSAFE_H

int csprof_context_is_unsafe(void *);
void csprof_add_unsafe_addresses_from_file(char *);

#endif
