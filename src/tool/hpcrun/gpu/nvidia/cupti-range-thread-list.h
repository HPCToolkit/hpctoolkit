#ifndef cupti_range_thread_list_h
#define cupti_range_thread_list_h

#include <stdbool.h>

void cupti_range_thread_list_add(int thread_id);

void cupti_range_thread_list_advance_cur();

bool cupti_range_thread_list_is_cur(int thread_id);

int cupti_range_thread_list_num_threads();

void cupti_range_thread_list_clear();

#endif 
