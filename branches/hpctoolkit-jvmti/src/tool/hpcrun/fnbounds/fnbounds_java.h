// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$ 
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2012, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

/*
 * The interface to the unwind interval tree.
 *
 */

#ifndef _FNBOUNDS_JAVA_H_
#define _FNBOUNDS_JAVA_H_

#include <sys/types.h>
#include <jni.h> 
#include <jvmti.h> 
#include "splay-interval.h"
#include "splay.h"

splay_interval_t * hpcjava_get_interval(void *addr);
void hpcjava_interval_tree_init(const char *jo_filename);
void hpcjava_release_splay_lock(void);
void *hpcjava_ui_malloc(size_t ui_size);

splay_interval_t *hpcjava_add_address_interval(const void *addr_start, const void *addr_end);
splay_interval_t *hpcjava_addr_to_interval_locked(const void *addr_start, const void *addr_end);

void hpcjava_delete_ui();
void free_ui_node_locked(interval_tree_node *node);

void hpcjava_print_interval_tree(void);
void hpcjava_set_jvmti(JavaVM *jvm, jvmtiEnv *jvmti);

void hpcjava_attach_thread(JavaVM *vm, jvmtiEnv *jvmti, JNIEnv *jenv);
void hpcjava_detach_thread();

#endif  /* !_FNBOUNDS_JAVA_H_ */
