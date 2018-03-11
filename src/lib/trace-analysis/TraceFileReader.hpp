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
 * File:   TraceFileReader.hpp
 * Author: Lai Wei <lai.wei@rice.edu>
 *
 * Created on March 7, 2018, 10:21 PM
 * 
 * Reads raw trace files to extract call path samples.
 */

#ifndef TRACEFILEREADER_HPP
#define TRACEFILEREADER_HPP

#include <stdio.h>
#include <string>
using std::string;
#include <vector>
using std::vector;

#include "CCTVisitor.hpp"
#include "TraceAnalysisCommon.hpp"

namespace TraceAnalysis {
  class CallPathSample;
  class TraceFileReader;
  
  // A frame on a call path.
  class CallPathFrame {
    friend class CallPathSample;
    
  public:
    enum FrameType {
      // Root frame
      Root,
      // Call frame
      Func,
      // Loop added to call path 
      Loop
    };
    
    const uint id;
    const uint procID;
    const string name;
    const VMA vma;
    const FrameType type;
    // ra is only valid when type == Func.
    const VMA ra;
    
  private:
    CallPathFrame(uint id, uint procID, string name, VMA vma, FrameType type, VMA ra);
  };
  
  // A sample of call path. 
  // Post-mortem analysis has added "loop frames" to call paths.
  class CallPathSample {
    friend class TraceFileReader;
    
  public:
    const Time timestamp;
  
    uint getDLCA() {
      return dLCA;
    }
    // Return inclusive depth of the call path, which is number of frames minus one.
    int getDepth();
    // Return a frame at a given depth (0 \<= depth \<= getDepth()).
    CallPathFrame& getFrameAtDepth(int depth);
    
  private:
    vector<CallPathFrame> frames;
    uint dLCA;
    
    CallPathSample(Time timestamp, uint dLCA, void* ptr);
  };
  
  // Handles read from raw trace file.
  class TraceFileReader {
  public:
    TraceFileReader(CCTVisitor& cctVisitor, string filename, Time minTime);
    virtual ~TraceFileReader();

    // Return a pointer to the next trace sample in the trace file.
    // Return NULL when error or at end of file.
    // Caller responsible for deallocating pointer.
    CallPathSample* readNextSample();

  private:
    CCTVisitor& cctVisitor;
    const Time minTime;
    FILE* file;
    void* hdr;
  };
}

#endif /* TRACEFILEREADER_HPP */

