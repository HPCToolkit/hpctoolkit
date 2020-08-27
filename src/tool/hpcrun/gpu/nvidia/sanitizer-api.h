#ifndef _HPCTOOLKIT_GPU_NVIDIA_SANITIZER_API_H_
#define _HPCTOOLKIT_GPU_NVIDIA_SANITIZER_API_H_

int
sanitizer_bind();

void
sanitizer_analysis_enable();

void
sanitizer_callbacks_subscribe();

void
sanitizer_callbacks_unsubscribe();

void
sanitizer_stop_flag_set();

void
sanitizer_stop_flag_unset();

void
sanitizer_device_flush(void *args);

void
sanitizer_device_shutdown(void *args);

void
sanitizer_process_init();

void
sanitizer_process_signal();

void
sanitizer_buffer_config(int gpu_patch_record_num, int buffer_pool_size);

void
sanitizer_approx_level_config(int approx_level);

void
sanitizer_views_config(int pc_views, int mem_views);

void
sanitizer_data_type_config(char *data_type);

int
sanitizer_buffer_pool_size_get();

#endif
