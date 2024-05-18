// SPDX-FileCopyrightText: 2007-2023 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

/*
 *  Libmonitor utility functions.
 */

#define _GNU_SOURCE

#include <signal.h>
#include <stdio.h>

#include "common.h"

/*
 *  Print the list of signals in the set into the buffer, up to a size
 *  limit.  For example: 10, 12, 20-30, etc.
 *
 *  Returns: the number of signals in the set if they all fit within
 *  the buffer, else -1 if the buffer is too small.
 */
#define SIG_ITEM_SIZE  15
int
monitor_sigset_string(char *buf, int size, const sigset_t *set)
{
    int sig, first, pos, prev, num_sigs;

    /* Unusual special cases, should never happen. */
    if (buf == NULL) {
        return -1;
    }
    buf[0] = 0;
    if (size < SIG_ITEM_SIZE) {
        return -1;
    }
    if (set == NULL) {
        sprintf(buf, " (null)");
        return 0;
    }

    pos = 0;
    prev = 0;
    num_sigs = 0;
    for (sig = 1; sig < MONITOR_NSIG; sig++) {
        if (sigismember(set, sig) == 1) {
            if (pos + SIG_ITEM_SIZE > size) {
                sprintf(&buf[prev], " and more");
                return -1;
            }
            prev = pos;
            first = sig;
            while (sig + 1 < MONITOR_NSIG && sigismember(set, sig + 1) == 1) {
                sig++;
            }
            if (sig > first) {
                pos += sprintf(&buf[pos], " %d-%d,", first, sig);
                num_sigs += sig - first + 1;
            } else {
                pos += sprintf(&buf[pos], " %d,", sig);
                num_sigs++;
            }
        }
    }
    buf[(pos > 0) ? pos - 1 : 0] = 0;

    return num_sigs;
}

/*
 *  Print the list of signals in the list (terminated by -1), same as
 *  monitor_sigset_string().  This is the format used in signal.c.
 *
 *  Returns: the number of signals in the set if they all fit within
 *  the buffer, else -1 if the buffer is too small.
 */
int
monitor_signal_list_string(char *buf, int size, int *list)
{
    int k, sig, first, pos, prev, num_sigs;

    /* Unusual special cases, should never happen. */
    if (buf == NULL) {
        return -1;
    }
    buf[0] = 0;
    if (size < SIG_ITEM_SIZE) {
        return -1;
    }
    if (list == NULL) {
        sprintf(buf, " (null)");
        return 0;
    }

    pos = 0;
    prev = 0;
    num_sigs = 0;
    for (k = 0; list[k] > 0; k++) {
        sig = list[k];
        if (pos + SIG_ITEM_SIZE > size) {
            sprintf(&buf[prev], " and more");
            return -1;
        }
        prev = pos;
        first = sig;
        while (list[k + 1] == sig + 1) {
            k++;
            sig++;
        }
        if (sig > first) {
            pos += sprintf(&buf[pos], " %d-%d,", first, sig);
            num_sigs += sig - first + 1;
        } else {
            pos += sprintf(&buf[pos], " %d,", sig);
            num_sigs++;
        }
    }
    buf[(pos > 0) ? pos - 1 : 0] = 0;

    return num_sigs;
}
