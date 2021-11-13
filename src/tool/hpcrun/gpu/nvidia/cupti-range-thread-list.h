#ifndef cupti_range_thread_list_h
#define cupti_range_thread_list_h

#include <stdbool.h>

#include "cupti-cct-trie.h"

void cupti_range_thread_list_add();

typedef void (*cupti_range_thread_list_fn_t)
(
 int thread_id,
 cupti_cct_trie_node_t *thread_trie_root,
 cupti_cct_trie_node_t **thread_trie_logic_root,
 cupti_cct_trie_node_t **thread_trie_cur,
 void *args
);

void cupti_range_thread_list_apply(cupti_range_thread_list_fn_t fn, void *args);

int cupti_range_thread_list_num_threads();

int cupti_range_thread_id_get();

void cupti_range_thread_list_clear();

#endif 
