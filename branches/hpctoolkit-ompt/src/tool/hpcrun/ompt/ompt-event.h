/* -------------- OMPT events -------------- */

  /*--- Mandatory Events ---*/
ompt_event(ompt_event_parallel_create, ompt_new_parallel_callback_t, 1, ompt_event_parallel_create_implemented) /* parallel create */
ompt_event(ompt_event_parallel_exit, ompt_new_parallel_callback_t, 2, ompt_event_parallel_exit_implemented) /* parallel exit */

ompt_event(ompt_event_task_create, ompt_new_task_callback_t, 3, ompt_event_task_create_implemented) /* task create */
ompt_event(ompt_event_task_exit, ompt_task_callback_t, 4, ompt_event_task_exit_implemented) /* task destroy */

ompt_event(ompt_event_thread_create, ompt_thread_callback_t, 5, ompt_event_thread_create_implemented) /* thread create */
ompt_event(ompt_event_thread_exit, ompt_thread_callback_t, 6, ompt_event_thread_exit_implemented) /* thread exit */

ompt_event(ompt_event_control, ompt_control_callback_t, 7, ompt_event_control_implemented) /* support control calls */

ompt_event(ompt_event_runtime_shutdown, ompt_callback_t, 8, ompt_event_runtime_shutdown_implemented) /* runtime shutdown */

  /*--- Optional Events (blame shifting, ompt_event_unimplemented) ---*/
ompt_event(ompt_event_idle_begin, ompt_thread_callback_t, 9, ompt_event_idle_begin_implemented) /* begin idle state */
ompt_event(ompt_event_idle_end, ompt_thread_callback_t, 10, ompt_event_idle_end_implemented) /* end idle state */

ompt_event(ompt_event_wait_barrier_begin, ompt_parallel_callback_t, 11, ompt_event_wait_barrier_begin_implemented) /* begin wait at barrier */
ompt_event(ompt_event_wait_barrier_end, ompt_parallel_callback_t, 12, ompt_event_wait_barrier_end_implemented) /* end wait at barrier */

ompt_event(ompt_event_wait_taskwait_begin, ompt_parallel_callback_t, 13, ompt_event_wait_taskwait_begin_implemented) /* begin wait at taskwait */
ompt_event(ompt_event_wait_taskwait_end, ompt_parallel_callback_t, 14, ompt_event_wait_taskwait_end_implemented) /* end wait at taskwait */

ompt_event(ompt_event_wait_taskgroup_begin, ompt_parallel_callback_t, 15, ompt_event_wait_taskgroup_begin_implemented) /* begin wait at taskgroup */
ompt_event(ompt_event_wait_taskgroup_end, ompt_parallel_callback_t, 16, ompt_event_wait_taskgroup_end_implemented) /* end wait at taskgroup */

ompt_event(ompt_event_release_lock, ompt_wait_callback_t, 17, ompt_event_release_lock_implemented) /* lock release */
ompt_event(ompt_event_release_nest_lock_last, ompt_wait_callback_t, 18, ompt_event_release_nest_lock_implemented) /* last nest lock release */
ompt_event(ompt_event_release_critical, ompt_wait_callback_t, 19, ompt_event_release_critical_implemented) /* critical release */

ompt_event(ompt_event_release_atomic, ompt_wait_callback_t, 20, ompt_event_release_atomic_implemented) /* atomic release */

ompt_event(ompt_event_release_ordered, ompt_wait_callback_t, 21, ompt_event_release_ordered_implemented) /* ordered release */

  /*--- Optional Events (synchronous events, ompt_event_unimplemented) --- */
ompt_event(ompt_event_implicit_task_create, ompt_parallel_callback_t, 22, ompt_event_implicit_task_create_implemented) /* implicit task create   */
ompt_event(ompt_event_implicit_task_exit, ompt_parallel_callback_t, 23, ompt_event_implicit_task_exit_implemented) /* implicit task destroy  */

ompt_event(ompt_event_task_switch, ompt_task_switch_callback_t, 24, ompt_event_task_switch_implemented) /* task switch */

ompt_event(ompt_event_loop_begin, ompt_parallel_callback_t, 25, ompt_event_loop_begin_implemented) /* task at loop begin */
ompt_event(ompt_event_loop_end, ompt_parallel_callback_t, 26, ompt_event_loop_end_implemented) /* task at loop end */

ompt_event(ompt_event_section_begin, ompt_parallel_callback_t, 27, ompt_event_section_begin_implemented) /* task at section begin  */
ompt_event(ompt_event_section_end, ompt_parallel_callback_t, 28, ompt_event_section_end_implemented) /* task at section end */

ompt_event(ompt_event_single_in_block_begin, ompt_parallel_callback_t, 29, ompt_event_single_in_block_begin_implemented) /* task at single begin*/
ompt_event(ompt_event_single_in_block_end, ompt_parallel_callback_t, 30, ompt_event_single_in_block_end_implemented) /* task at single end */

ompt_event(ompt_event_single_others_begin, ompt_parallel_callback_t, 31, ompt_event_single_others_begin_implemented) /* task at single begin */
ompt_event(ompt_event_single_others_end, ompt_parallel_callback_t, 32, ompt_event_single_others_end_implemented) /* task at single end */

ompt_event(ompt_event_master_begin, ompt_parallel_callback_t, 33, ompt_event_master_begin_implemented) /* task at master begin */
ompt_event(ompt_event_master_end, ompt_parallel_callback_t, 34, ompt_event_master_end_implemented) /* task at master end */

ompt_event(ompt_event_barrier_begin, ompt_parallel_callback_t, 35, ompt_event_barrier_begin_implemented) /* task at barrier begin  */
ompt_event(ompt_event_barrier_end, ompt_parallel_callback_t, 36, ompt_event_barrier_end_implemented) /* task at barrier end */

ompt_event(ompt_event_taskwait_begin, ompt_parallel_callback_t, 37, ompt_event_taskwait_begin_implemented) /* task at taskwait begin */
ompt_event(ompt_event_taskwait_end, ompt_parallel_callback_t, 38, ompt_event_taskwait_end_implemented) /* task at task wait end */

ompt_event(ompt_event_taskgroup_begin, ompt_parallel_callback_t, 39, ompt_event_taskgroup_begin_implemented) /* task at taskgroup begin */
ompt_event(ompt_event_taskgroup_end, ompt_parallel_callback_t, 40, ompt_event_taskgroup_end_implemented) /* task at taskgroup end */

ompt_event(ompt_event_release_nest_lock_prev, ompt_parallel_callback_t, 41, ompt_event_release_nest_lock_prev_implemented) /* prev nest lock release */

ompt_event(ompt_event_wait_lock, ompt_wait_callback_t, 42, ompt_event_wait_lock_implemented) /* lock wait */
ompt_event(ompt_event_wait_nest_lock, ompt_wait_callback_t, 43, ompt_event_wait_nest_lock_implemented) /* nest lock wait */
ompt_event(ompt_event_wait_critical, ompt_wait_callback_t, 44, ompt_event_wait_critical_implemented) /* critical wait */
ompt_event(ompt_event_wait_atomic, ompt_wait_callback_t, 45, ompt_event_wait_atomic_implemented) /* atomic wait */
ompt_event(ompt_event_wait_ordered, ompt_wait_callback_t, 46, ompt_event_wait_ordered_implemented) /* ordered wait */

ompt_event(ompt_event_acquired_lock, ompt_wait_callback_t, 47, ompt_event_acquired_lock_implemented) /* lock acquired */
ompt_event(ompt_event_acquired_nest_lock_first, ompt_wait_callback_t, 48, ompt_event_acquired_nest_lock_first_implemented) /* 1st nest lock acquired */
ompt_event(ompt_event_acquired_nest_lock_next, ompt_parallel_callback_t, 49, ompt_event_acquired_nest_lock_next_implemented) /* next nest lock acquired*/
ompt_event(ompt_event_acquired_critical, ompt_wait_callback_t, 50, ompt_event_acquired_critical_implemented) /* critical acquired */
ompt_event(ompt_event_acquired_atomic, ompt_wait_callback_t, 51, ompt_event_acquired_atomic_implemented) /* atomic acquired */
ompt_event(ompt_event_acquired_ordered, ompt_wait_callback_t, 52, ompt_event_acquired_ordered_implemented) /* ordered acquired */

ompt_event(ompt_event_init_lock, ompt_wait_callback_t, 53, ompt_event_init_lock_implemented) /* lock init */
ompt_event(ompt_event_init_nest_lock, ompt_wait_callback_t, 54, ompt_event_init_nest_lock_implemented) /* nest lock init */
ompt_event(ompt_event_destroy_lock, ompt_wait_callback_t, 55, ompt_event_destroy_lock_implemented) /* lock destruction */
ompt_event(ompt_event_destroy_nest_lock, ompt_wait_callback_t, 56, ompt_event_destroy_nest_lock_implemented) /* nest lock destruction */

ompt_event(ompt_event_flush, ompt_thread_callback_t, 57, ompt_event_flush_implemented) /* after executing flush */

#undef ompt_event
