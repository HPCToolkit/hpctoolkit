// -*-Mode: C++;-*-

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
// Copyright ((c)) 2002-2017, Rice University
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
 * File:   TCT-Time.cpp
 * Author: Lai Wei <lai.wei@rice.edu>
 *
 * Created on April 30, 2018, 12:02 AM
 */

#include "TCT-Time.hpp"
#include "TCT-Time-Internal.hpp"

namespace TraceAnalysis {  
  TCTTime::TCTTime() {
    ptr = new TCTTraceTime();
  }
  
  TCTTime::TCTTime(const TCTTime& orig) {
    ptr = ((TCTATime*)orig.ptr)->duplicate();
  }
  
  TCTTime::~TCTTime() {
    delete (TCTATime*)ptr;
  }
  
  void TCTTime::clear() {
    ((TCTATime*)ptr)->clear();
  }
  
  double TCTTime::getNumSamples() const {
    return ((TCTATime*)ptr)->numSamples;
  }
  
  void TCTTime::setNumSamples(double numSamples) {
    ((TCTATime*)ptr)->numSamples = numSamples;
  }
  
  Time TCTTime::getDuration() const {
    return ((TCTATime*)ptr)->getDuration();
  }
  
  Time TCTTime::getMinDuration() const {
    return ((TCTATime*)ptr)->getMinDuration();
  }
  
  Time TCTTime::getMaxDuration() const {
    return ((TCTATime*)ptr)->getMaxDuration();
  }
  
  void TCTTime::setDuration(Time min, Time max) {
    if (((TCTATime*)ptr)->type == TRACE) {
      delete (TCTATime*)ptr;
      ptr = new TCTProfileTime();
    }
    ((TCTProfileTime*)ptr)->minDurationInclusive = min;
    ((TCTProfileTime*)ptr)->maxDurationInclusive = max;
  }
  
  TCTTraceTime* toTraceTime(void* ptr, const char* funcName) {
    TCTATime* aTime = (TCTATime*)ptr;
    if (aTime->type != TRACE) {
      print_msg(MSG_PRIO_MAX, "%s() called on profile time.\n", funcName);
      return NULL;
    }
    return ((TCTTraceTime*)aTime);
  }
  
  void TCTTime::setStartTime(Time exclusive, Time inclusive) {
    TCTTraceTime* traceTime = toTraceTime(ptr, "setStartTime");
    traceTime->startTimeExclusive = exclusive;
    traceTime->startTimeInclusive = inclusive;
  }

  void TCTTime::setEndTime(Time inclusive, Time exclusive) {
    TCTTraceTime* traceTime = toTraceTime(ptr, "setEndTime");
    traceTime->endTimeExclusive = exclusive;
    traceTime->endTimeInclusive = inclusive;
  }

  Time TCTTime::getStartTimeExclusive() const {
    TCTTraceTime* traceTime = toTraceTime(ptr, "getStartTimeExclusive");
    return traceTime->startTimeExclusive;
  }

  Time TCTTime::getStartTimeInclusive() const {
    TCTTraceTime* traceTime = toTraceTime(ptr, "getStartTimeInclusive");
    return traceTime->startTimeInclusive;
  }

  Time TCTTime::getEndTimeInclusive() const {
    TCTTraceTime* traceTime = toTraceTime(ptr, "getEndTimeInclusive");
    return traceTime->endTimeInclusive;
  }

  Time TCTTime::getEndTimeExclusive() const {
    TCTTraceTime* traceTime = toTraceTime(ptr, "getEndTimeExclusive");
    return traceTime->endTimeExclusive;
  }
  
  void TCTTime::addTime(const TCTTime& other) {
    TCTATime* aTime = (TCTATime*)ptr;
    TCTProfileTime* profTime = NULL;
    if (aTime->type == TRACE) {
      profTime = new TCTProfileTime(*aTime);
      delete aTime;
      ptr = profTime;
    } 
    else 
      profTime = (TCTProfileTime*) aTime;
    
    TCTATime* otherATime = (TCTATime*) other.ptr;
    profTime->numSamples += otherATime->numSamples;
    profTime->minDurationInclusive += otherATime->getMinDuration();
    profTime->maxDurationInclusive += otherATime->getMaxDuration();
  }
  
  void TCTTime::shiftTime(Time offset) {
    TCTATime* aTime = (TCTATime*)ptr;
    if (aTime->type == TRACE) {
      TCTTraceTime* traceTime = (TCTTraceTime*)aTime;
      traceTime->startTimeExclusive += offset;
      traceTime->startTimeInclusive += offset;
      traceTime->endTimeInclusive   += offset;
      traceTime->endTimeExclusive   += offset;
    }
  }
  
  void TCTTime::toProfileTime() {
    TCTATime* aTime = (TCTATime*)ptr;
    if (aTime->type == TRACE) {
      TCTProfileTime* profTime = new TCTProfileTime(*aTime);
      delete aTime;
      ptr = profTime;
    } 
  }

  void TCTTime::setAsAverageTime(const TCTTime& time1, long weight1, const TCTTime& time2, long weight2) {
    TCTATime* aTime = (TCTATime*)ptr;
    TCTATime* aTime1 = (TCTATime*)time1.ptr;
    TCTATime* aTime2 = (TCTATime*)time2.ptr;
    
    if (aTime1->type == TRACE && aTime2->type == TRACE) {
      if (aTime->type != TRACE) {
        delete aTime;
        aTime = new TCTTraceTime();
        ptr = aTime;
      }
      TCTTraceTime* traceTime = (TCTTraceTime*)aTime;
      TCTTraceTime* traceTime1 = (TCTTraceTime*)aTime1;
      TCTTraceTime* traceTime2 = (TCTTraceTime*)aTime2;
      traceTime->startTimeExclusive = computeWeightedAverage(
              traceTime1->startTimeExclusive, weight1, traceTime2->startTimeExclusive, weight2);
      traceTime->startTimeInclusive = computeWeightedAverage(
              traceTime1->startTimeInclusive, weight1, traceTime2->startTimeInclusive, weight2);
      traceTime->endTimeInclusive   = computeWeightedAverage(
              traceTime1->endTimeInclusive,   weight1, traceTime2->endTimeInclusive,   weight2);
      traceTime->endTimeExclusive   = computeWeightedAverage(
              traceTime1->endTimeExclusive,   weight1, traceTime2->endTimeExclusive,   weight2);
    }
    else {
      if (aTime->type != PROFILE) {
        delete aTime;
        aTime = new TCTProfileTime();
        ptr = aTime;
      }
      TCTProfileTime* profTime = (TCTProfileTime*)aTime;
      profTime->maxDurationInclusive = computeWeightedAverage(
              aTime1->getMaxDuration(), weight1, aTime2->getMaxDuration(), weight2);
      profTime->minDurationInclusive = computeWeightedAverage(
              aTime1->getMinDuration(), weight1, aTime2->getMinDuration(), weight2);
    }
    
    aTime->numSamples = (aTime1->numSamples * weight1 + aTime2->numSamples * weight2) / (double)(weight1 + weight2);
  }

  string TCTTime::toString() const {
    return ((TCTATime*)ptr)->toString();
  }
}