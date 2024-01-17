/*
 *  Shared thread declarations.
 *
 *  Copyright (c) 2007-2023, Rice University.
 *  All rights reserved.
 *  
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *  
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  
 *  * Neither the name of Rice University (RICE) nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *  
 *  This software is provided by RICE and contributors "as is" and any
 *  express or implied warranties, including, but not limited to, the
 *  implied warranties of merchantability and fitness for a particular
 *  purpose are disclaimed. In no event shall RICE or contributors be
 *  liable for any direct, indirect, incidental, special, exemplary, or
 *  consequential damages (including, but not limited to, procurement of
 *  substitute goods or services; loss of use, data, or profits; or
 *  business interruption) however caused and on any theory of liability,
 *  whether in contract, strict liability, or tort (including negligence
 *  or otherwise) arising in any way out of the use of this software, even
 *  if advised of the possibility of such damage.
 *
 *  $Id$
 *
 *  Note: other files need to share the monitor_thread_node struct,
 *  so this file should not assume that it's always configured with
 *  pthreads.
 */

#ifndef _MONITOR_THREAD_H_
#define _MONITOR_THREAD_H_

#include "config.h"
#ifdef MONITOR_USE_PTHREADS
#include <pthread.h>
#endif
#include "queue.h"

#define MONITOR_TN_MAGIC  0x6d746e00

typedef void *pthread_start_fcn_t(void *);

struct monitor_thread_node {
    LIST_ENTRY(monitor_thread_node) tn_links;
#ifdef MONITOR_USE_PTHREADS
    pthread_t  tn_self;
    pthread_start_fcn_t  *tn_start_routine;
#endif
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
