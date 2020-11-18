#ifndef _HPCTOOLKIT_GPU_NVIDIA_SANITIZER_FUNCTION_LIST_H_
#define _HPCTOOLKIT_GPU_NVIDIA_SANITIZER_FUNCTION_LIST_H_

#include <stdbool.h>

typedef struct sanitizer_function_list_entry_s sanitizer_function_list_entry_t;

void sanitizer_function_list_init(sanitizer_function_list_entry_t **head);

bool sanitizer_function_list_register(sanitizer_function_list_entry_t *head, const char *function);

sanitizer_function_list_entry_t *sanitizer_function_list_lookup(sanitizer_function_list_entry_t *head, const char *function);

const char *sanitizer_function_list_entry_function_get(sanitizer_function_list_entry_t *entry);

#endif
