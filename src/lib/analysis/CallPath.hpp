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
// Copyright ((c)) 2002-2016, Rice University
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

#ifndef Analysis_CallPath_CallPath_hpp 
#define Analysis_CallPath_CallPath_hpp

//************************* System Include Files ****************************

#include <iostream>
#include <vector>
#include <stack>
#include <string>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include "Args.hpp"
#include "Util.hpp"

#include <lib/binutils/LM.hpp>

#include <lib/prof/CallPath-Profile.hpp>
#include <lib/prof/Struct-Tree.hpp>

//*************************** Forward Declarations ***************************

//****************************************************************************

namespace Analysis {

namespace CallPath {

extern std::ostream* dbgOs; // for parallel debugging


// ---------------------------------------------------------
//
// ---------------------------------------------------------

Prof::CallPath::Profile*
read(const Util::StringVec& profileFiles, const Util::UIntVec* groupMap,
     int mergeTy, uint rFlags = 0, uint mrgFlags = 0);

Prof::CallPath::Profile*
read(const char* prof_fnm, uint groupId, uint rFlags = 0);

static inline Prof::CallPath::Profile*
read(const string& prof_fnm, uint groupId, uint rFlags = 0)
{
  return read(prof_fnm.c_str(), groupId, rFlags);
}


void
readStructure(Prof::Struct::Tree* structure, const Analysis::Args& args);


// ---------------------------------------------------------
// 
// ---------------------------------------------------------

// overlayStaticStructure: In the CCT collected by hpcrun, call sites
// (CCT::Call) and statements (CCT::Stmt) have no procedure frames.
// Overlaying static structure creates frames for CCT::Call and
// CCT::Stmt nodes.
// 
// After static structure has been overlayed on the CCT, CCT::Stmt's
// are still distinct by instruction pointer even if they map to the
// same Struct::Stmt.  Currently,
// coalesceStmts(Prof::CallPath::Profile) applies the Non-Overlapping
// Principle to scope within the CCT.
//
// The resulting CCT obeys the following invariants:
// - Every CCT:Call node has one or more CCT::ProcFrm nodes as
//   children.  Conversely, except for the root, every CCT::ProcFrm
//   has a CCT::Call node for a parent.
// - Every CCT::Call and CCT::Stmt is a descendant of a CCT::ProcFrm
// - A CCT::Stmt node is always a leaf.
void
overlayStaticStructureMain(Prof::CallPath::Profile& prof,
			   string agent, bool doNormalizeTy);

void
overlayStaticStructureMain(Prof::CallPath::Profile& prof,
			   Prof::LoadMap::LM* loadmap_lm,
			   Prof::Struct::LM* lmStrct);


// lm is optional and may be NULL
void 
overlayStaticStructure(Prof::CallPath::Profile& prof,
		       Prof::LoadMap::LM* loadmap_lm,
		       Prof::Struct::LM* lmStrct, BinUtil::LM* lm);

// specialty function for hpcprof-mpi
void
noteStaticStructureOnLeaves(Prof::CallPath::Profile& prof);

void
pruneBySummaryMetrics(Prof::CallPath::Profile& prof, uint8_t* prunedNodes);

void
normalize(Prof::CallPath::Profile& prof, string agent, bool doNormalizeTy);

void
pruneStructTree(Prof::CallPath::Profile& prof);


// ---------------------------------------------------------
// 
// ---------------------------------------------------------

void
applyThreadMetricAgents(Prof::CallPath::Profile& prof, string agent);

void
applySummaryMetricAgents(Prof::CallPath::Profile& prof, string agent);


// ---------------------------------------------------------
//
// ---------------------------------------------------------

void
makeDatabase(Prof::CallPath::Profile& prof, const Analysis::Args& args);


} // namespace CallPath

} // namespace Analysis

//****************************************************************************

#endif // Analysis_CallPath_CallPath_hpp
