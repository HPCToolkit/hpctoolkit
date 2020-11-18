#include "sanitizer-function-list.h"

#include <string.h>
#include <stdbool.h>
#include <hpcrun/memory/hpcrun-malloc.h>

struct sanitizer_function_list_entry_s {
  struct sanitizer_function_list_entry_s *next;
  char *function;
};


void sanitizer_function_list_init(sanitizer_function_list_entry_t **head) {
  if (*head == NULL) {
    *head = hpcrun_malloc_safe(sizeof(sanitizer_function_list_entry_t));
  }
}


bool sanitizer_function_list_register(sanitizer_function_list_entry_t *head, const char *function) {
  sanitizer_function_list_entry_t *cur = head;
  sanitizer_function_list_entry_t *prev = NULL;
  while (cur != NULL) {
    if (cur->function != NULL && strcmp(cur->function, function) == 0) {
      return false;
    }
    prev = cur;
    cur = cur->next;
  }

  if (prev != NULL) {
    prev->next = hpcrun_malloc_safe(sizeof(sanitizer_function_list_entry_t));
    prev->next->function = hpcrun_malloc_safe(sizeof(char) * (strlen(function) + 1));
    prev->next->next = NULL;
    strcpy(prev->next->function, function);
    return true;
  }
  return false;
}


sanitizer_function_list_entry_t *sanitizer_function_list_lookup(sanitizer_function_list_entry_t *head, const char *function) {
  sanitizer_function_list_entry_t *cur = head;

  while (cur != NULL) {
    if (cur->function != NULL && strcmp(cur->function, function) == 0) {
      return cur;
    }
    cur = cur->next;
  }
  return NULL;
}


const char *sanitizer_function_list_entry_function_get(sanitizer_function_list_entry_t *entry) {
  if (entry != NULL && entry->function != NULL) {
    return entry->function;
  }
  return NULL;
}
