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
 * File:   TCTTime.hpp
 * Author: Lai Wei <lai.wei@rice.edu>
 *
 * Created on March 4, 2018, 11:01 PM
 * 
 * Time structs in Temporal Context Tree. 
 */

#ifndef TCT_TIME_HPP
#define TCT_TIME_HPP

#include <string>
using std::string;

#include "../TraceAnalysisCommon.hpp"

namespace TraceAnalysis {
  // Temporal Context Tree Abstract Time
  class TCTATime {
  public:
    TCTATime() {}
    TCTATime(const TCTATime& orig) {}
    virtual ~TCTATime() {}
    
    virtual Time getDuration() {
      return getMinDuration() + getMaxDuration() / 2;
    }
    virtual Time getMinDuration() = 0;
    virtual Time getMaxDuration() = 0;
    
    virtual TCTATime* duplicate() = 0;
    
    virtual string toString() = 0;
  };
  
  // Temporal Context Tree Trace Time
  class TCTTraceTime : public TCTATime {
  public:
    TCTTraceTime() : TCTATime() {
      startTimeExclusive = 0;
      startTimeInclusive = 0;
      endTimeInclusive = 0;
      endTimeExclusive = 0;
    }
    TCTTraceTime(const TCTTraceTime& orig) : TCTATime(orig) {
      startTimeExclusive = orig.startTimeExclusive;
      startTimeInclusive = orig.startTimeInclusive;
      endTimeInclusive = orig.endTimeInclusive;
      endTimeExclusive = orig.endTimeExclusive;
    }
    virtual ~TCTTraceTime() {}
    
    virtual Time getMinDuration() {
      if (endTimeInclusive == startTimeExclusive) return 0; //TODO: for dummy trace time
      return endTimeInclusive - startTimeInclusive + 1;
    }
    
    virtual Time getMaxDuration() {
      if (endTimeInclusive == startTimeExclusive) return 0; //TODO: for dummy trace time
      return endTimeExclusive - startTimeExclusive - 1;
    }
    
    virtual TCTATime* duplicate() {
      return new TCTTraceTime(*this);
    }
    
    virtual string toString() {
      return timeToString((startTimeExclusive + startTimeInclusive)/2) + " ~ "
              + timeToString((endTimeInclusive + endTimeExclusive)/2);
    }
    
    void setStartTime(Time exclusive, Time inclusive) {
      startTimeExclusive = exclusive;
      startTimeInclusive = inclusive;
    }
    
    void setEndTime(Time inclusive, Time exclusive) {
      endTimeInclusive = inclusive;
      endTimeExclusive = exclusive;
    }
    
    void shiftTime(Time offset) {
      startTimeExclusive += offset;
      startTimeInclusive += offset;
      endTimeInclusive += offset;
      endTimeExclusive += offset;
    }
    
  private:
    Time startTimeExclusive;
    Time startTimeInclusive;
    Time endTimeInclusive;
    Time endTimeExclusive;
  };
  
  // Temporal Context Profile Time
  class TCTProfileTime : public TCTATime {
  public:
    TCTProfileTime() : TCTATime() {
      minDurationInclusive = 0;
      maxDurationInclusive = 0;
      minDurationExclusive = 0;
      maxDurationExclusive = 0;
    }
    TCTProfileTime(const TCTProfileTime& orig) : TCTATime(orig) {
      minDurationInclusive = orig.minDurationInclusive;
      maxDurationInclusive = orig.maxDurationInclusive;
      minDurationExclusive = orig.minDurationExclusive;
      maxDurationExclusive = orig.maxDurationExclusive;
    }
    virtual ~TCTProfileTime() {}
    
    virtual Time getMinDuration() {
      return minDurationInclusive;
    }
    
    virtual Time getMaxDuration() {
      return maxDurationInclusive;
    }
    
    virtual TCTATime* duplicate() {
      return new TCTProfileTime(*this);
    }
    
    virtual string toString() {
      return " In-time = " + timeToString((minDurationInclusive + maxDurationInclusive)/2) + 
             " Ex-time = " + timeToString((minDurationExclusive + maxDurationExclusive)/2);
    }
    
  private:
    Time minDurationInclusive;
    Time maxDurationInclusive;
    Time minDurationExclusive;
    Time maxDurationExclusive;
  };
}

#endif /* TCT_TIME_HPP */

