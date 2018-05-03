
#include "TCT-Time.hpp"

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

namespace TraceAnalysis {
  enum TimeType {
    TRACE,
    PROFILE
  };
    
  // Temporal Context Tree Abstract Time
  class TCTATime {
  public:
    TCTATime(TimeType type) : type(type) {}
    TCTATime(const TCTATime& orig) : type(orig.type) {}
    virtual ~TCTATime() {}
    
    virtual void clear() = 0;
    
    virtual Time getDuration() const {
      return (getMinDuration() + getMaxDuration()) / 2;
    }
    virtual Time getMinDuration() const = 0;
    virtual Time getMaxDuration() const = 0;
    
    virtual TCTATime* duplicate() = 0;
    
    virtual string toString() const = 0;
    
    const TimeType type;
  };
  
  // Temporal Context Tree Trace Time
  class TCTTraceTime : public TCTATime {
  public:
    TCTTraceTime() : TCTATime(TRACE) {
      clear();
    }
    TCTTraceTime(const TCTTraceTime& orig) : TCTATime(orig) {
      startTimeExclusive = orig.startTimeExclusive;
      startTimeInclusive = orig.startTimeInclusive;
      endTimeInclusive = orig.endTimeInclusive;
      endTimeExclusive = orig.endTimeExclusive;
    }
    virtual ~TCTTraceTime() {}
    
    virtual void clear() {
      startTimeExclusive = 0;
      startTimeInclusive = 0;
      endTimeInclusive = 0;
      endTimeExclusive = 0;
    }
    
    virtual Time getMinDuration() const {
      //if (endTimeInclusive == startTimeExclusive) return 0; //TODO: for dummy trace time
      return endTimeInclusive - startTimeInclusive + 1;
    }
    
    virtual Time getMaxDuration() const {
      //if (endTimeInclusive == startTimeExclusive) return 0; //TODO: for dummy trace time
      return endTimeExclusive - startTimeExclusive - 1;
    }
    
    virtual TCTATime* duplicate() {
      return new TCTTraceTime(*this);
    }
    
    virtual string toString() const {
      return timeToString((startTimeExclusive + startTimeInclusive)/2) + " ~ "
              + timeToString((endTimeInclusive + endTimeExclusive)/2);
      //return //timeToString(startTimeExclusive) + "/" + timeToString(startTimeInclusive) + " ~ "
        //+ timeToString(endTimeInclusive) + "/" + timeToString(endTimeExclusive) + 
        //", Duration = " + timeToString(getMinDuration()) + "/" + timeToString(getMaxDuration());
    }
    
    Time startTimeExclusive;
    Time startTimeInclusive;
    Time endTimeInclusive;
    Time endTimeExclusive;
  };
  
  // Temporal Context Tree Profile Time
  class TCTProfileTime : public TCTATime {
  public:
    TCTProfileTime() : TCTATime(PROFILE) {
      clear();
    }
    TCTProfileTime(const TCTProfileTime& orig) : TCTATime(orig) {
      minDurationInclusive = orig.minDurationInclusive;
      maxDurationInclusive = orig.maxDurationInclusive;
    }
    TCTProfileTime(const TCTATime& other) : TCTATime(PROFILE) {
      minDurationInclusive = other.getMinDuration();
      maxDurationInclusive = other.getMaxDuration();
    }
    virtual ~TCTProfileTime() {}
    
    virtual void clear() {
      minDurationInclusive = 0;
      maxDurationInclusive = 0;
    }
    
    virtual Time getMinDuration() const {
      return minDurationInclusive;
    }
    
    virtual Time getMaxDuration() const {
      return maxDurationInclusive;
    }
    
    virtual TCTATime* duplicate() {
      return new TCTProfileTime(*this);
    }
    
    virtual string toString() const {
      return " Duration = " + timeToString((minDurationInclusive + maxDurationInclusive)/2);
      //return " Duration = " + timeToString(minDurationInclusive) + "/" + timeToString(maxDurationInclusive);
    }

    Time minDurationInclusive;
    Time maxDurationInclusive;
  };
  
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
    profTime->minDurationInclusive += otherATime->getMinDuration();
    profTime->maxDurationInclusive += otherATime->getMaxDuration();
  }

  void TCTTime::setAsAverageTime(const TCTTime& time1, int weight1, const TCTTime& time2, int weight2) {
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
      traceTime->endTimeInclusive= computeWeightedAverage(
              traceTime1->endTimeInclusive, weight1, traceTime2->endTimeInclusive, weight2);
      traceTime->endTimeExclusive= computeWeightedAverage(
              traceTime1->endTimeExclusive, weight1, traceTime2->endTimeExclusive, weight2);
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
  }
  
  /*
      void shiftTime(Time offset) {
      startTimeExclusive += offset;
      startTimeInclusive += offset;
      endTimeInclusive += offset;
      endTimeExclusive += offset;
    }
    
   */

  string TCTTime::toString() const {
    return ((TCTATime*)ptr)->toString();
  }
}