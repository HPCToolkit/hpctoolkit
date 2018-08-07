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
 * File:   TCT-Time-Internal.hpp
 * Author: Lai Wei <lai.wei@rice.edu>
 *
 * Created on July 21, 2018, 9:20 PM
 */

#ifndef TCT_TIME_INTERNAL_HPP
#define TCT_TIME_INTERNAL_HPP

#include <boost/serialization/access.hpp>

#include <string>
using std::string;

namespace TraceAnalysis {
  enum TimeType {
    TRACE,
    PROFILE
  };
    
  // Temporal Context Tree Abstract Time
  class TCTATime {
    friend class boost::serialization::access;
  private:
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version);
    
  public:
    TCTATime(TimeType type) : type(type), numSamples(0) {}
    TCTATime(const TCTATime& orig) : type(orig.type), numSamples(orig.numSamples) {}
    virtual ~TCTATime() {}
    
    virtual void clear() {
      numSamples = 0;
    }
    
    virtual Time getDuration() const {
      return (getMinDuration() + getMaxDuration()) / 2;
    }
    virtual Time getMinDuration() const = 0;
    virtual Time getMaxDuration() const = 0;
    
    virtual TCTATime* duplicate() = 0;
    
    virtual string toString() const {
      if (numSamples == (long)numSamples)
        return " Samples = " + std::to_string((long)numSamples);
      else
        return " Samples = " + std::to_string(numSamples);
    }
    
    const TimeType type;
    double numSamples;
  };
  
  // Temporal Context Tree Trace Time
  class TCTTraceTime : public TCTATime {
    friend class boost::serialization::access;
  private:
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version);
    
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
      TCTATime::clear();
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
      return TCTATime::toString() + ", " + timeToString((startTimeExclusive + startTimeInclusive)/2) + " ~ "
              + timeToString((endTimeInclusive + endTimeExclusive)/2);
    }
    
    Time startTimeExclusive;
    Time startTimeInclusive;
    Time endTimeInclusive;
    Time endTimeExclusive;
  };
  
  // Temporal Context Tree Profile Time
  class TCTProfileTime : public TCTATime {
    friend class boost::serialization::access;
  private:
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version);
    
  public:
    TCTProfileTime() : TCTATime(PROFILE) {
      clear();
    }
    TCTProfileTime(const TCTProfileTime& orig) : TCTATime(orig) {
      minDurationInclusive = orig.minDurationInclusive;
      maxDurationInclusive = orig.maxDurationInclusive;
    }
    TCTProfileTime(const TCTATime& other) : TCTATime(PROFILE) {
      numSamples = other.numSamples;
      minDurationInclusive = other.getMinDuration();
      maxDurationInclusive = other.getMaxDuration();
    }
    virtual ~TCTProfileTime() {}
    
    virtual void clear() {
      TCTATime::clear();
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
      return TCTATime::toString() + ", Duration = " + timeToString((minDurationInclusive + maxDurationInclusive)/2);
    }

    Time minDurationInclusive;
    Time maxDurationInclusive;
  };
}

#endif /* TCT_TIME_INTERNAL_HPP */

