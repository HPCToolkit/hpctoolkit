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

#ifndef _JMT_H_
#define _JMT_H_

#include <jvmti.h>

// jmt: java method tools
//      an interface for handling, storing, retrieving and managing java compiled
//      methods 

// jmt_get_address: get the code address from java method ID based on the database
void * jmt_get_address(jmethodID method);

// jmt_add_java_method: storing the pair java-method-ID and its address to the database
void jmt_add_java_method(jmethodID method, const void *address);

// jmt_add_method_db: add a method into the database of methods
// this function has to be called to store method ID of java callstack
int jmt_add_method_db(jmethodID method);

// jmt_get_all_methods_db : get the list of methods in the database
jmethodID* jmt_get_all_methods_db(jvmtiEnv * jvmti);

#endif //ifndef _JMT_H_
