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
  // Temporal Context Tree Time
  class TCTTime {
  public:
    TCTTime();
    TCTTime(const TCTTime& orig);
    virtual ~TCTTime();
    
    void clear();
    
    Time getDuration() const;
    Time getMinDuration() const;
    Time getMaxDuration() const;
    
    void setDuration(Time min, Time max);
    
    void setStartTime(Time exclusive, Time inclusive);
    void setEndTime(Time inclusive, Time exclusive);
    Time getStartTimeExclusive() const;
    Time getStartTimeInclusive() const;
    Time getEndTimeInclusive() const;
    Time getEndTimeExclusive() const;
    
    void addTime(const TCTTime& other);
    void setAsAverageTime(const TCTTime& time1, int weight1, const TCTTime& time2, int weight2);
    
    string toString() const;
    
  private:
    void* ptr;
  };
}

#endif /* TCT_TIME_HPP */

