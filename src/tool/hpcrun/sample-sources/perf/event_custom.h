// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2020, Rice University
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

#ifndef __CUSTOM_EVENT_H__
#define __CUSTOM_EVENT_H__

#include "sample_event.h"
#include <lib/prof-lean/hpcrun-fmt.h>
#include "sample-sources/perf/perf-util.h"


// --------------------------------------------------------------
// data type
// --------------------------------------------------------------


typedef enum event_handle_type_e {EXCLUSIVE, INCLUSIVE} event_handle_type_t;

typedef enum event_accept_type_e {NOT_MY_EVENT, ACCEPT_EVENT, REJECT_EVENT} event_accept_type_t;

typedef struct event_custom_s event_custom_t;

typedef struct event_handler_arg_s {
  int    metric;                      /* metric id */
  double metric_value;                /* the value of the metric by perf handler */
  void   *context;                    /* current context */

  sample_val_t *sample;               /* the result of sampling by hpcrun callpath */

  struct event_info_s     *current;   /* info of the event */
  struct perf_mmap_data_s *data;      /* additional data from Linux kernel */
} event_handler_arg_t;


// --------------------------------------------------------------
// callback functions

typedef int  register_event_t(sample_source_t *self,
                              kind_info_t     *kb_kind,
                              event_custom_t  *event,
                              struct event_threshold_s *period);

typedef event_accept_type_t event_handler_t(event_handler_arg_t *args);

// --------------------------------------------------------------
// Main data structure to register a customized event
// this type should be used only within perf plug-in.
// --------------------------------------------------------------
typedef struct event_custom_s {
  const char *name;                  // unique name of the event
  const char *desc;                  // brief description of the event

  register_event_t *register_fn;     // function to register the event

  event_handler_t  *pre_handler_fn;  // callback to be used during the sampling
  event_handler_t  *post_handler_fn; // callback to be used during the sampling

  event_handle_type_t handle_type;   // whether the handler will be called exclusively or inclusively (all events)

} event_custom_t;


// --------------------------------------------------------------
// Function interface
// --------------------------------------------------------------

/***
 * create a custom event
 */ 
int event_custom_create_event(sample_source_t *self, kind_info_t *kb_kind, char *name);
/**
 * find an event custom based on its event name.
 * @return event_custom_t if exists, NULL otherwise.
 **/ 
event_custom_t *event_custom_find(const char *name);

/**
 * register a new event. 
 * For the sake of simplicity, if an event already exists, it still adds it.
 * The caller has to make sure the name of the event is unique
 * @param event_custom_t event
 **/
int event_custom_register(event_custom_t *event);

/**
 * list all registered custom events
 **/ 
void event_custom_display(FILE *std);

/**
 * to be called during the signal handler, BEFORE unwinding
 * the custom event can decide not to continue handling the sample, hence
 *  save the cpu time to avoid unwinding undesired sample
 */
event_accept_type_t
event_custom_pre_handler(event_handler_arg_t *args);

/**
 * method to be called after signal delivery. If an event is recognized,
 * it will delivered to the custom handler.
 **/ 
event_accept_type_t event_custom_post_handler(event_handler_arg_t *args);


#endif
