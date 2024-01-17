/*
 *  Libmonitor dlopen functions.
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
 *  Override functions:
 *
 *    dlopen
 *    dlclose
 *
 *  Support functions:
 *
 *    monitor_real_dlopen
 *    monitor_real_dlclose
 */

#include "config.h"
#include <dlfcn.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>

#include "common.h"
#include "monitor.h"

/*
 *----------------------------------------------------------------------
 *  GLOBAL VARIABLES and EXTERNAL SYMBOLS
 *----------------------------------------------------------------------
 */

typedef void *dlopen_fcn_t(const char *, int);
typedef int dlclose_fcn_t(void *);

#ifdef MONITOR_STATIC
extern dlopen_fcn_t   __real_dlopen;
extern dlclose_fcn_t  __real_dlclose;
#endif

static dlopen_fcn_t   *real_dlopen = NULL;
static dlclose_fcn_t  *real_dlclose = NULL;

/*
 *----------------------------------------------------------------------
 *  INTERNAL HELPER FUNCTIONS
 *----------------------------------------------------------------------
 */

static void
monitor_dlopen_init(void)
{
    static int init_done = 0;

    if (init_done)
	return;

    MONITOR_GET_REAL_NAME_WRAP(real_dlopen, dlopen);
    MONITOR_GET_REAL_NAME_WRAP(real_dlclose, dlclose);
    init_done = 1;
}

/*
 *----------------------------------------------------------------------
 *  OVERRIDE and EXTERNAL FUNCTIONS
 *----------------------------------------------------------------------
 */

/*
 *  Client access to the real dlopen() and dlclose().
 */
void *
monitor_real_dlopen(const char *path, int flags)
{
    monitor_dlopen_init();
    return (*real_dlopen)(path, flags);
}

int
monitor_real_dlclose(void *handle)
{
    monitor_dlopen_init();
    return (*real_dlclose)(handle);
}

/*
 *  Override dlopen() and dlclose().
 */
void *
MONITOR_WRAP_NAME(dlopen)(const char *path, int flags)
{
    void *handle;

    monitor_dlopen_init();
    MONITOR_DEBUG("(pre) path: %s, flags: %d\n", path, flags);
    monitor_pre_dlopen(path, flags);
    handle = (*real_dlopen)(path, flags);
    monitor_dlopen(path, flags, handle);
    MONITOR_DEBUG("(post) path: %s, handle: %p\n", path, handle);

    return (handle);
}

int
MONITOR_WRAP_NAME(dlclose)(void *handle)
{
    int ret;

    monitor_dlopen_init();
    MONITOR_DEBUG("(pre) handle: %p\n", handle);
    monitor_dlclose(handle);
    ret = (*real_dlclose)(handle);
    monitor_post_dlclose(handle, ret);
    MONITOR_DEBUG("(post) handle: %p, ret: %d\n", handle, ret);

    return (ret);
}
