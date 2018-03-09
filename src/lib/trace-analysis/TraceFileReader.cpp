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
 * File:   TraceFileReader.cpp
 * Author: Lai Wei <lai.wei@rice.edu>
 * 
 * Created on March 7, 2018, 10:21 PM
 * 
 * Reads raw trace files to extract call path samples.
 */

#include "TraceFileReader.hpp"

#include <lib/analysis/CallPath.hpp>
#include <lib/prof-lean/hpcrun-fmt.h>
#include <lib/prof/NameMappings.hpp>

namespace TraceAnalysis {
  CallPathFrame::CallPathFrame(uint id, string name, VMA vma, FrameType type, VMA ra) :
          id(id), name(name), vma(vma), type(type), ra(ra) {}
  
  CallPathSample::CallPathSample(Time timestamp, uint dLCA, void* leafFrame) :
          timestamp(timestamp), dLCA(dLCA) {
    Prof::CCT::ANode* cur = (Prof::CCT::ANode*) leafFrame;
    Prof::CCT::ANode* last = cur;
    while (cur != NULL && cur->type() != cur->TyRoot) {
      if (cur->type() == cur->TyLoop) {
        CallPathFrame loop(
                cur->id(), 
                "loop@" + std::to_string(cur->begLine()),
                cur->structure()->vmaSet().begin()->beg(),
                CallPathFrame::Loop,
                0);
        frames.push_back(loop);
      }
      
      if (cur->type() == cur->TyCall) {
        // cur is call, last is proc.
        string name = last->structure()->name();
        bool isFake;
        name = normalize_name(name.c_str(), isFake);
        CallPathFrame func(
                cur->id(),
                name,
                last->structure()->vmaSet().begin()->beg(),
                CallPathFrame::Func,
                ((Prof::CCT::Call*)cur)->lmRA());
        frames.push_back(func);
      }
      
      last = cur;
      cur = cur->parent();
    }
    
    if (cur->type() == cur->TyRoot) {
      // cur is root, last is proc.
      string name = last->structure()->name();
      bool isFake;
      name = normalize_name(name.c_str(), isFake);
      CallPathFrame func(
              cur->id(),
              name,
              0,
              CallPathFrame::Root,
              0);
      frames.push_back(func);
    }
  }
  
  int CallPathSample::getDepth() {
    return frames.size() - 1;
  }
  
  CallPathFrame& CallPathSample::getFrameAtDepth(int depth) {
    return frames[frames.size() - 1 - depth];
  }
  
  TraceFileReader::TraceFileReader(CCTVisitor& cctVisitor, string filename, Time minTime) : 
          cctVisitor(cctVisitor), minTime(minTime) {
    file = fopen(filename.c_str(),"r");
    if (file != NULL) {
      hdr = new hpctrace_fmt_hdr_t();
      hpctrace_fmt_hdr_fread((hpctrace_fmt_hdr_t*)hdr, file);
    }
  }

  TraceFileReader::~TraceFileReader() {
    if (file != NULL) {
      delete (hpctrace_fmt_hdr_t*)hdr;
      fclose(file);
    }
  }
  
  CallPathSample* TraceFileReader::readNextSample() {
    if (file == NULL) return NULL;
    
    hpctrace_fmt_datum_t trace;
    int ret = hpctrace_fmt_datum_fread(&trace, ((hpctrace_fmt_hdr_t*)hdr)->flags, file);
    if (ret != HPCFMT_OK) return NULL;
    
    CallPathSample* cp = new CallPathSample(trace.time - minTime, trace.dLCA,
            cctVisitor.getLeafFrame(trace.cpId));
    
    //TODO: filter centain call paths, such as partial call paths
    
    return cp;
  }
}