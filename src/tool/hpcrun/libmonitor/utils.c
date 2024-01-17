/*
 *  Libmonitor utility functions.
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
 */

#include <signal.h>
#include <stdio.h>

#include "config.h"
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
