// SPDX-FileCopyrightText: 2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

/*
 *  Libmonitor default callback functions.
 *
 *  Define the defaults as weak symbols and allow the client to
 *  override a subset of them.
 *
 *  This file is in the public domain.
 *
 *  NB: For inclusion in hpcrun, only those functions which hpcrun does not
 *  define are given default implementations here. To ensure there is only ever
 *  one definition (hpcrun or default) they are also made strong symbols.
 *
 *  $Id$
 */

#define _GNU_SOURCE

#include <sys/types.h>
#include <unistd.h>
#include "common.h"
#include "monitor.h"

void
monitor_init_library(void)
{
    MONITOR_DEBUG1("(default callback)\n");
}

void
monitor_fini_library(void)
{
    MONITOR_DEBUG1("(default callback)\n");
}

void
monitor_init_thread_support(void)
{
    MONITOR_DEBUG1("(default callback)\n");
}

void
monitor_pre_dlopen(const char *path, int flags)
{
    MONITOR_DEBUG1("(default callback)\n");
}

void
monitor_dlopen(const char *path, int flags, void *handle)
{
    MONITOR_DEBUG("(default callback) path = %s, flags = %d, handle = %p\n",
                  path, flags, handle);
}

void
monitor_dlclose(void *handle)
{
    MONITOR_DEBUG("(default callback) handle = %p\n", handle);
}

void
monitor_post_dlclose(void *handle, int ret)
{
    MONITOR_DEBUG1("(default callback)\n");
}

void
monitor_fini_mpi(void)
{
    MONITOR_DEBUG("(default callback) size = %d, rank = %d\n",
                  monitor_mpi_comm_size(), monitor_mpi_comm_rank());
}

void
monitor_mpi_post_fini(void)
{
    MONITOR_DEBUG1("(default callback)\n");
}

int
monitor_wrap_main(int argc, char **argv, char **envp)
{
    MONITOR_DEBUG1("(default callback)\n");
    return 0;
}
