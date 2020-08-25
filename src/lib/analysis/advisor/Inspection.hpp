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
// Copyright ((c)) 2002-2018, Rice University
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

//***************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************



#ifndef Analysis_Advisor_Inspection_hpp 
#define Analysis_Advisor_Inspection_hpp

//************************* System Include Files ****************************

#include <string>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include <lib/binutils/LM.hpp>

#include <lib/prof/CallPath-Profile.hpp>
#include <lib/prof/Struct-Tree.hpp>

#include <vector>
#include <tuple>

//*************************** Forward Declarations ***************************

//****************************************************************************

namespace Analysis {

class InstructionBlame;

struct Inspection {
  typedef std::string (*InspectionCallBack)(const InstructionBlame &);

  double total;

  bool loop;
  bool stall;

  std::string optimization;
  std::string hint;

  std::vector<InstructionBlame> regions;
  std::vector<std::vector<InstructionBlame>> hotspots;

  // speedups[0..n-1] = overall speedup
  // speedup[n] = specific speedups
  std::vector<double> ratios;
  std::vector<double> speedups;

  // <before, after>
  std::pair<int, int> active_warp_count;
  std::pair<int, int> block_count;
  std::pair<int, int> thread_count;
  std::pair<int, int> reg_count;

  InspectionCallBack callback;

  Inspection()
      : total(-1.0),
        loop(false),
        stall(false),
        active_warp_count(-1, -1),
        block_count(-1, -1),
        thread_count(-1, -1),
        reg_count(-1, -1),
        callback(NULL) {}

  void clear() {
    hint.clear();
    optimization.clear();
    regions.clear();
    hotspots.clear();
    speedups.clear();
    ratios.clear();
    total = -1.0;
    stall = false;
    loop = false;
    callback = NULL;
    active_warp_count = std::pair<int, int>(-1, -1);
    block_count = std::pair<int, int>(-1, -1);
    thread_count = std::pair<int, int>(-1, -1);
    reg_count = std::pair<int, int>(-1, -1);
  }
};


class InspectionFormatter {
 public:
  InspectionFormatter() {}

  virtual std::string format(const Inspection &inspection) = 0;

  ~InspectionFormatter() {} 

 protected:
  std::stack<Prof::Struct::Alien *> getInlineStack(Prof::Struct::ACodeNode *stmt);
};


class SimpleInspectionFormatter : public InspectionFormatter {
 public:
  SimpleInspectionFormatter() {}

  virtual std::string format(const Inspection &inspection);

  ~SimpleInspectionFormatter() {}
 
 private:
  std::string formatInlineStack(std::stack<Prof::Struct::Alien *> &st);
};


}  // namespace Analysis


#endif  // Analysis_Advisor_Inspection_hpp
