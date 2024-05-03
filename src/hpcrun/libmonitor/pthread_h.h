// SPDX-FileCopyrightText: 2007-2023 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

/*
 *  Shared thread declarations.
 *
 *  Note: other files need to share the monitor_thread_node struct,
 *  so this file should not assume that it's always configured with
 *  pthreads.
 */

#ifndef _MONITOR_THREAD_H_
#define _MONITOR_THREAD_H_

#include <pthread.h>
#include "queue.h"

#define MONITOR_TN_MAGIC  0x6d746e00

typedef void *pthread_start_fcn_t(void *);

struct monitor_thread_node {
    LIST_ENTRY(monitor_thread_node) tn_links;
    pthread_t  tn_self;
    pthread_start_fcn_t  *tn_start_routine;
    int    tn_magic;
    int    tn_tid;
    void  *tn_arg;
    void  *tn_user_data;
    void  *tn_stack_bottom;
    void  *tn_thread_info;
    char   tn_is_main;
    char   tn_ignore_threads;
    volatile char  tn_appl_started;
    volatile char  tn_fini_started;
    volatile char  tn_fini_done;
    volatile char  tn_exit_win;
    volatile char  tn_block_shootdown;
};

struct monitor_thread_node *monitor_get_tn(void);
struct monitor_thread_node *monitor_get_main_tn(void);
void monitor_reset_thread_list(struct monitor_thread_node *);

#endif  /* ! _MONITOR_THREAD_H_ */
