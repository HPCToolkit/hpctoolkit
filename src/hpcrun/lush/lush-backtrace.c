// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//***************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   LUSH: Logical Unwind Support for HPCToolkit
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
// Author:
//   Nathan Tallent, Rice University.
//
//***************************************************************************

//************************* System Include Files ****************************

#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>

//************************ libmonitor Include Files *************************

#include "../libmonitor/monitor.h"

//*************************** User Include Files ****************************

#include "../../common/lean/min-max.h"

#include "lush.h"
#include "lush-backtrace.h"
#include "../cct_insert_backtrace.h"

#include "../epoch.h"
#include "../sample_event.h" // hpcrun_drop_sample()
#include "../unwind/common/backtrace.h" // dump_backtrace()


//*************************** Forward Declarations **************************

#define MYDBG 0

static frame_t*
canonicalize_chord(frame_t* chord_beg, lush_assoc_t as,
                   unsigned int pchord_len, unsigned int lchord_len);


//***************************************************************************
// LUSH Agents
//***************************************************************************

lush_agent_pool_t* lush_agents = NULL;
bool is_lush_agent = false;


//***************************************************************************
// LUSH backtrace
//***************************************************************************

cct_node_t*
lush_backtrace2cct(cct_bundle_t* cct, ucontext_t* context,
                   int metricId,
                   hpcrun_metricVal_t metricIncr,
                   int skipInner, int isSync)
{
  // ---------------------------------------------------------
  // Record backtrace if:
  //   1) a lush agent indicates we should
  //   2) a non-lush-relevant metric
  //   3) a synchronous unwind (may have a lush relevant metric)
  // ---------------------------------------------------------
  bool     doMetric = false;
  uint64_t incrMetric = metricIncr.i;

  bool     doMetricIdleness = false;
  double   incrMetricIdleness = 0.0;
  // lush_agentid_t aidMetricIdleness; // = lush_agentid_NULL; // list of agents

  if (metricId == lush_agents->metric_time) {
    lush_agentid_t aid = 1; // TODO: multiple agents
    if (lush_agents->LUSHI_do_metric[aid](metricIncr.i,
                                          &doMetric, &doMetricIdleness,
                                          &incrMetric, &incrMetricIdleness)) {
      //aidMetricIdleness = aid; // case 1
    }
  }

  if (isSync || metricId != lush_agents->metric_time) {
    doMetric = true; // case 2 and 3
  }

  if (!doMetric) {
    return NULL;
  }


  // ---------------------------------------------------------
  // Perform the backtrace
  // ---------------------------------------------------------

  lush_cursor_t cursor;
  lush_init_unw(&cursor, lush_agents, context);

  // FIXME: unwind/common/backtrace.c
  thread_data_t* td = hpcrun_get_thread_data();
  td->btbuf_cur   = td->btbuf_beg;  // innermost

  td->btbuf_sav   = td->btbuf_end;

  // ---------------------------------------------------------
  // Step through bichords
  // ---------------------------------------------------------
  lush_step_t ty = LUSH_STEP_NULL;

  while ( (ty = lush_step_bichord(&cursor)) != LUSH_STEP_END_PROJ
          && ty != LUSH_STEP_ERROR ) {
    lush_agentid_t aid = lush_cursor_get_aid(&cursor);
    lush_assoc_t as = lush_cursor_get_assoc(&cursor);

    TMSG(LUNW, "Chord: aid:%d assoc:%d", aid, as);

    // FIXME: short circuit unwind if we hit the 'active return'

    hpcrun_ensure_btbuf_avail();
    frame_t* chord_beg = td->btbuf_cur; // innermost note
    unsigned int pchord_len = 0, lchord_len = 0;

    // ---------------------------------------------------------
    // Step through p-notes of p-chord
    // ---------------------------------------------------------
    while (lush_step_pnote(&cursor) != LUSH_STEP_END_CHORD) {
      hpcrun_ensure_btbuf_avail();

      //unw_word_t ip = lush_cursor_get_ip(&cursor);
      ip_normalized_t ip_norm = lush_cursor_get_ip_norm(&cursor);
      TMSG(LUNW, "IP:  lm-id = %d and lm-ip = %p", ip_norm.lm_id,
           ip_norm.lm_ip);
      td->btbuf_cur->ip_norm = ip_norm;

      pchord_len++;
      td->btbuf_cur++;
    }

    td->btbuf_cur = chord_beg;

    // ---------------------------------------------------------
    // Step through l-notes of l-chord
    // ---------------------------------------------------------
    lush_lip_t* lip_persistent = NULL;
    while (lush_step_lnote(&cursor) != LUSH_STEP_END_CHORD) {
      hpcrun_ensure_btbuf_avail();

      lush_lip_t* lip = lush_cursor_get_lip(&cursor); // ephemeral
      TMSG(LUNW, "LIP: %p", *((void**)lip));

      if (lush_assoc_is_a_to_1(as)) {
        if (!lip_persistent) {
          lip_persistent = lush_lip_clone(lip);
        }
        // else: lip_persistent is already set
      }
      else {
        // INVARIANT: as must be 1-to-M
        lip_persistent = lush_lip_clone(lip);
      }
      td->btbuf_cur->lip = lip_persistent;

      lchord_len++;
      td->btbuf_cur++;
    }

    // ---------------------------------------------------------
    // canonicalize frames to form a chord
    // ---------------------------------------------------------
    frame_t* chord_end;
    chord_end = canonicalize_chord(chord_beg, as, pchord_len, lchord_len);

    td->btbuf_cur = chord_end;
  }

  if (ty == LUSH_STEP_ERROR) {
    hpcrun_drop_sample(); // NULL
  }

  // ---------------------------------------------------------
  // insert backtrace into calling context tree (if sensible)
  // ---------------------------------------------------------
  if (MYDBG) {
    hpcrun_bt_dump(td->btbuf_cur, "LUSH");
  }

  frame_t* bt_beg = td->btbuf_beg;      // innermost, inclusive
  frame_t* bt_end = td->btbuf_cur - 1; // outermost, inclusive
  cct_node_t* cct_cursor = cct->tree_root;

  if (skipInner) {
    bt_beg = hpcrun_skip_chords(bt_end, bt_beg, skipInner);
  }

  cct_node_t* node = NULL;
  node = hpcrun_cct_insert_backtrace_w_metric(cct_cursor, metricId,
                                              bt_end, bt_beg,
                                              metricIncr, NULL);

  if (doMetricIdleness) {
    // lush_agentid_t aid = aidMetricIdleness;
    int mid = lush_agents->metric_idleness;
    cct_metric_data_increment(mid, node,
                              (cct_metric_data_t){.r = incrMetricIdleness});
  }

  // FIXME: register active return

  return node;
}


// returns the end of the chord (exclusive)
static frame_t*
canonicalize_chord(frame_t* chord_beg, lush_assoc_t as,
                   unsigned int pchord_len, unsigned int lchord_len)
{
  // Set assoc and fill empty p-notes/l-notes

  // INVARIANT: chord_len >= 1
  //   [chord_beg = innermost ... outermost, chord_end)

  unsigned int chord_len = MAX(pchord_len, lchord_len);
  frame_t* chord_end  = chord_beg + chord_len; // N.B.: exclusive
  frame_t* pchord_end = chord_beg + pchord_len;
  frame_t* lchord_end = chord_beg + lchord_len;

  ip_normalized_t ip_norm = {0, 0};
  lush_lip_t* lip = NULL;

  if (as == LUSH_ASSOC_1_to_M) {
    ip_norm = chord_beg->ip_norm;
  }
  else if (as == LUSH_ASSOC_M_to_1) {
    lip = chord_beg->lip;
  }
  // else: default is fine for a-to-0 and 1-to-1

  unsigned int path_len = chord_len;
  for (frame_t* x = chord_beg; x < chord_end; ++x, --path_len) {
    lush_assoc_info__set_assoc(x->as_info, as);
    lush_assoc_info__set_path_len(x->as_info, path_len);

    if (x >= pchord_end) {
      // INVARIANT: as must be 1-to-M
      x->ip_norm = ip_norm;
    }
    if (x >= lchord_end) {
      // INVARIANT: as is one of: M-to-1, a-to-0
      x->lip = lip;
    }
  }

  return chord_end;
}


//***************************************************************************
