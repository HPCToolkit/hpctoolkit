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

#ifndef prof_Prof_CCT_Tree_hpp
#define prof_Prof_CCT_Tree_hpp

//************************* System Include Files ****************************

#include <iostream>

#include <string>
#include <vector>
#include <list>
#include <set>

#include <typeinfo>

#include <cstring> // for memcpy

//*************************** User Include Files ****************************

#include <include/uint.h>

#include "CCT-Merge.hpp"

#include "Metric-Mgr.hpp"
#include "Metric-ADesc.hpp"
#include "Metric-IData.hpp"

#include "Struct-Tree.hpp"

#include "LoadMap.hpp"

#include <lib/isa/ISATypes.hpp>

#include <lib/prof-lean/hpcrun-fmt.h>
#include <lib/prof-lean/lush/lush-support.h>

#include <lib/binutils/VMAInterval.hpp> // TODO

#include <lib/xml/xml.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/NonUniformDegreeTree.hpp>
#include <lib/support/SrcFile.hpp>
#include <lib/support/Unique.hpp>


//*************************** Forward Declarations ***************************

inline std::ostream&
operator<<(std::ostream& os, const hpcrun_metricVal_t x)
{
  os << "[" << x.i << " " << x.r << "]";
  return os;
}


//***************************************************************************
// Tree
//***************************************************************************

namespace Prof {

namespace CallPath {
  class Profile;
}


namespace CCT {

class ANode;


class Tree
  : public Unique
{
public:

  enum {
    // Output flags
    OFlg_Compressed      = (1 << 0), // Write in compressed format
    OFlg_LeafMetricsOnly = (1 << 1), // Write metrics only at leaves (outdated)
    OFlg_Debug           = (1 << 2), // Debug: show xtra source line info
    OFlg_DebugAll        = (1 << 3), // Debug: (may be invalid format)
    OFlg_StructId        = (1 << 4)  // Add hpcstruct node id (for debug)
  };


public:
  // -------------------------------------------------------
  // Constructor/Destructor
  // -------------------------------------------------------
  
  Tree(const CallPath::Profile* metadata);

  virtual ~Tree();

  // -------------------------------------------------------
  // Tree data
  // -------------------------------------------------------
  ANode*
  root() const
  { return m_root; }

  void
  root(ANode* x)
  { m_root = x; }

  bool
  empty() const
  { return (m_root == NULL); }

  const CallPath::Profile*
  metadata() const
  { return m_metadata; }
  
  // -------------------------------------------------------
  // Given a Tree, merge into 'this'
  // -------------------------------------------------------
  MergeEffectList*
  merge(const Tree* y, uint x_newMetricBegIdx,
	uint mrgFlag = 0, uint oFlag = 0);

  // -------------------------------------------------------
  // dense ids (only used when explicitly requested)
  // -------------------------------------------------------

  // makeDensePreorderIds: deterministically creates dense ids,
  // reserving 0 as a NULL (unused) id; returns maxDenseId()
  uint
  makeDensePreorderIds();
  
  // maxDenseId(): returns the maximum id actually used
  uint
  maxDenseId() const
  { return m_maxDenseId; }
 
  // -------------------------------------------------------
  // nodeId -> ANode map (built on demand)
  // -------------------------------------------------------

  ANode*
  findNode(uint nodeId) const;

  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  // pruneCCTByNodeId
  void
  pruneCCTByNodeId(const uint8_t* prunedNodes);

  // -------------------------------------------------------
  // 
  // -------------------------------------------------------
  
  // verifyUniqueCPIds: Ensure all the cpIds in the Tree are unique.
  bool
  verifyUniqueCPIds();

  // -------------------------------------------------------
  // Write contents
  // -------------------------------------------------------
  std::ostream&
  writeXML(std::ostream& os,
	   uint metricBeg = Metric::IData::npos,
	   uint metricEnd = Metric::IData::npos,
	   uint oFlags = 0) const;

  std::ostream&
  dump(std::ostream& os = std::cerr, uint oFlags = 0) const;
  
  void
  ddump() const;


  // Given a set of flags 'flags', determines whether we need to
  // ensure that certain characters are escaped.  Returns xml::ESC_TRUE
  // or xml::ESC_FALSE.
  static int
  doXMLEscape(uint oFlags);

public:
  typedef std::map<uint, ANode*> NodeIdToANodeMap;

  
private:
  // CCT and metadata for interpreting CCT (e.g., metrics)
  ANode* m_root;
  const CallPath::Profile* m_metadata; // does not own

  // dense id
  uint m_maxDenseId;
  mutable NodeIdToANodeMap* m_nodeidMap;

  // merge information, cached here for performance
  MergeContext* m_mergeCtxt;
};


} // namespace CCT

} // namespace Prof


//***************************************************************************
// ANode
//***************************************************************************

namespace Prof {

namespace CCT {

class ANode;     // Everyone's base class
class ADynNode;
class AProcNode;

class Root;

class ProcFrm;
class Proc;
class Loop;
class Call;
class Stmt;

// ---------------------------------------------------------
// ANode: The base node for a call stack profile tree.
// ---------------------------------------------------------
class ANode
  : public NonUniformDegreeTreeNode,
    public Metric::IData,
    public Unique
{
public:
  typedef std::vector<ANode*> Vec;

public:
  enum ANodeTy {
    TyRoot = 0,
    TyProcFrm,
    TyProc,
    TyLoop,
    TyCall,
    TyStmt,
    TyANY,
    TyNUMBER
  };
  
  static const std::string&
  ANodeTyToName(ANodeTy tp);
  
  static ANodeTy
  IntToANodeType(long i);

  // N.B.: An easy implementation for now (but not thread-safe!)
  static uint s_raToCallsiteOfst;

 
private:
  static const std::string NodeNames[TyNUMBER];


public:
  ANode(ANodeTy type, ANode* parent, Struct::ACodeNode* strct = NULL)
    : NonUniformDegreeTreeNode(parent),
      Metric::IData(),
      m_type(type), m_id(s_nextUniqueId), m_strct(strct)
  {
    s_nextUniqueId += 2; // cf. HPCRUN_FMT_RetainIdFlag
  }

  ANode(ANodeTy type,
	ANode* parent, Struct::ACodeNode* strct, const Metric::IData& metrics)
    : NonUniformDegreeTreeNode(parent),
      Metric::IData(metrics),
      m_type(type), m_id(s_nextUniqueId), m_strct(strct)
  {
    s_nextUniqueId += 2; // cf. HPCRUN_FMT_RetainIdFlag
  }

  virtual ~ANode()
  { }
  
  // deep copy of internals (but without children)
  ANode(const ANode& x)
    : NonUniformDegreeTreeNode(NULL),
      Metric::IData(x),
      m_type(x.m_type), /*m_id: skip*/ m_strct(x.m_strct)
  {
    zeroLinks();
    s_nextUniqueId += 2; // cf. HPCRUN_FMT_RetainIdFlag
  }

  // deep copy of internals (but without children)
  ANode&
  operator=(const ANode& x)
  {
    if (this != &x) {
      //NonUniformDegreeTreeNode::operator=(x);
      Metric::IData::operator=(x);
      m_type = x.m_type;
      // m_id: skip
      m_strct = x.m_strct;
    }
    return *this;
  }


  // --------------------------------------------------------
  // General data
  // --------------------------------------------------------
  ANodeTy
  type() const
  { return m_type; }


  // id: a unique id; 0 is reserved for a NULL value
  //
  // N.B.: To support reading/writing, ids must be consistent with
  // HPCRUN_FMT_RetainIdFlag.
  uint
  id() const
  { return m_id; }
  
  
  void
  id(uint id)
  { m_id = id; }

  
  // 'name()' is overridden by some derived classes
  virtual const std::string&
  name() const
  { return ANodeTyToName(type()); }


  // structure: static structure id for this node; the same static
  // structure will have the same structure().
  Struct::ACodeNode*
  structure() const
  { return m_strct; }

  void
  structure(const Struct::ACodeNode* strct)
  { m_strct = const_cast<Struct::ACodeNode*>(strct); }

  uint
  structureId() const
  { return (m_strct) ? m_strct->id() : 0; }

  SrcFile::ln
  begLine() const
  { return (m_strct) ? m_strct->begLine() : ln_NULL; }

  SrcFile::ln
  endLine() const
  { return (m_strct) ? m_strct->endLine() : ln_NULL;  }
  

  // --------------------------------------------------------
  // Tree navigation
  // --------------------------------------------------------
  ANode*
  parent() const
  { return static_cast<ANode*>(NonUniformDegreeTreeNode::Parent()); }

  ANode*
  firstChild() const
  { return static_cast<ANode*>(NonUniformDegreeTreeNode::FirstChild()); }

  ANode*
  lastChild() const
  { return static_cast<ANode*>(NonUniformDegreeTreeNode::LastChild()); }

  ANode*
  nextSibling() const
  {
    // siblings are linked in a circular list
    if (parent()->lastChild() != this) {
      return dynamic_cast<ANode*>(NonUniformDegreeTreeNode::NextSibling());
    }
    return NULL;
  }

  ANode*
  prevSibling() const
  {
    // siblings are linked in a circular list
    if (parent()->firstChild() != this) {
      return dynamic_cast<ANode*>(NonUniformDegreeTreeNode::PrevSibling());
    }
    return NULL;
  }


  // --------------------------------------------------------
  // ancestor: find first ANode in path from this to root with given type
  // (Note: We assume that a node *can* be an ancestor of itself.)
  // --------------------------------------------------------
  ANode*
  ancestor(ANodeTy tp) const;
  
  Root*
  ancestorRoot() const;

  ProcFrm*
  ancestorProcFrm() const;

  Proc*
  ancestorProc() const;

  Loop*
  ancestorLoop() const;

  Call*
  ancestorCall() const;

  Stmt*
  ancestorStmt() const;


  // --------------------------------------------------------
  // Metrics (cf. Struct::ANode)
  //   N.B.: Intervals are in the form: [mBegId, mEndId)
  // --------------------------------------------------------

  void
  zeroMetricsDeep(uint mBegId, uint mEndId);


  // aggregateMetricsIncl: aggregates metrics for inclusive CCT
  // metrics. [mBegId, mEndId) forms an interval for batch processing.
  void
  aggregateMetricsIncl(uint mBegId, uint mEndId);

  void
  aggregateMetricsIncl(const VMAIntervalSet& ivalset);

  void
  aggregateMetricsIncl(uint mBegId)
  { aggregateMetricsIncl(mBegId, mBegId + 1); }


  // aggregateMetricsExcl: aggregates metrics for exclusive CCT
  // metrics. [mBegId, mEndId) forms an interval for batch processing.
  void
  aggregateMetricsExcl(uint mBegId, uint mEndId);

  void
  aggregateMetricsExcl(const VMAIntervalSet& ivalset);

  void
  aggregateMetricsExcl(uint mBegId)
  { aggregateMetricsExcl(mBegId, mBegId + 1); }

private:
  //
  // laks 2015.10.21: we don't want accumulate the exclusive cost of 
  // an inlined statement to the caller. Instead, we assume an inline
  // function (Proc) as the same as a normal procedure (ProcFrm).
  // And the lowest common ancestor for Proc and ProcFrm is AProcNode.
  void
  aggregateMetricsExcl(AProcNode* frame, const VMAIntervalSet& ivalset);

public:
  // computeMetrics: compute this subtree's Metric::DerivedDesc metric
  //   values for metric ids [mBegId, mEndId)
  // computeMetricsMe: same, but for the node (not the subtree)
  void
  computeMetrics(const Metric::Mgr& mMgr, uint mBegId, uint mEndId,
		 bool doFinal);

  void
  computeMetricsMe(const Metric::Mgr& mMgr, uint mBegId, uint mEndId,
		   bool doFinal);


  // computeMetricsIncr: compute this subtree's Metric::DerivedIncrDesc metric
  //   values for metric ids [mBegId, mEndId)
  // computeMetricsIncrMe: same, but for the node (not the subtree)
  void
  computeMetricsIncr(const Metric::Mgr& mMgr, uint mBegId, uint mEndId,
		     Metric::AExprIncr::FnTy fn);

  void
  computeMetricsIncrMe(const Metric::Mgr& mMgr, uint mBegId, uint mEndId,
		       Metric::AExprIncr::FnTy fn);

  // pruneByMetrics: TODO: make this static for consistency
  void
  pruneByMetrics(const Metric::Mgr& mMgr, const VMAIntervalSet& ivalset,
		 const ANode* root, double thresholdPct,
		 uint8_t* prunedNodes = NULL);

  // pruneChildrenByNodeId:
  void
  pruneChildrenByNodeId(const uint8_t* prunedNodes);

  // pruneByNodeId: Note that 'x' may be deleted
  static void
  pruneByNodeId(ANode*& x, const uint8_t* prunedNodes);

  // deleteChaff: deletes all subtrees y in x for which y has no
  // persistant 'cpId'.  Returns true if subtree x was deleted; false
  // otherwise.  If 'deletedNodes' is non-NULL, records all deleted
  // nodes (assuming dense node ids).
  static bool
  deleteChaff(ANode* x, uint8_t* deletedNodes = NULL);

public:

  // --------------------------------------------------------
  // merging
  // --------------------------------------------------------

  // mergeDeep: Let 'this' = x and let y be a node corresponding to x
  //   in the sense that we may think of y as being locally merged
  //   with x (according to ADynNode::isMergable()).  Given y,
  //   recursively merge y's children into x.
  // N.B.: assume we can destroy y.
  // N.B.: assume x already has space to store merged metrics
  std::list<MergeEffect>*
  mergeDeep(ANode* y, uint x_numMetrics, MergeContext& mrgCtxt, uint oFlag = 0);

  
  // merge: Let 'this' = x and let y be a node corresponding to x.
  //   Merge y into x.
  // N.B.: assume we can destroy y.
  MergeEffect
  merge(ANode* y);

  
  virtual MergeEffect
  mergeMe(const ANode& y, MergeContext* mrgCtxt = NULL, uint metricBegIdx = 0, bool mayConflict = true);


  // findDynChild: Let z = 'this' be an interior ADynNode (otherwise the
  //   return value is trivially NULL).  Given an ADynNode y_dyn, finds
  //   the first direct ADynNode descendent x_dyn, if any, for which
  //   ADynNode::isMergable(x_dyn, y_dyn) holds.
  //
  // If the CCT does not have structure information, we only need to
  //   inspect the children of z.  Otherwise, it is necessary to find
  //   the collection of z's direct ADynNode descendents.
  CCT::ADynNode*
  findDynChild(const ADynNode& y_dyn);


  // --------------------------------------------------------
  // 
  // --------------------------------------------------------

  // makeDensePreorderIds: given the *next* id to use, return the
  //   *next* id to use
  uint
  makeDensePreorderIds(uint nextId);


  // --------------------------------------------------------
  // Output
  // --------------------------------------------------------

  virtual std::string
  toString(uint oFlags = 0, const char* pfx = "") const;

  virtual std::string
  toStringMe(uint oFlags = 0) const;

  std::ostream&
  writeXML(std::ostream& os,
	   uint metricBeg = Metric::IData::npos,
	   uint metricEnd = Metric::IData::npos,
	   uint oFlags = 0, const char* pfx = "") const;


  std::ostream&
  writeXML_path(std::ostream& os,
	   uint metricBeg = Metric::IData::npos,
	   uint metricEnd = Metric::IData::npos,
	   uint oFlags = 0, const char* pfx = "") const;

  std::ostream&
  dump(std::ostream& os = std::cerr, uint oFlags = 0, const char* pfx = "") const;

  void
  adump() const;

  void
  ddump() const;

  void
  ddumpMe() const;
  
  virtual std::string
  codeName() const;

protected:

  bool
  writeXML_pre(std::ostream& os,
	       uint metricBeg = Metric::IData::npos,
	       uint metricEnd = Metric::IData::npos,
	       uint oFlags = 0,
	       const char* pfx = "") const;
  void
  writeXML_post(std::ostream& os, uint oFlags = 0, const char* pfx = "") const;

  // --------------------------------------------------------
  // Makes room for new metrics. Also checks and resolves
  // any cpId conflicts between 2 trees.
  // --------------------------------------------------------

  MergeEffectList*
  mergeDeep_fixInsert(int newMetrics, MergeContext& mrgCtxt);


private:
  static uint s_nextUniqueId;
  
protected:
  ANodeTy m_type; // obsolete with typeid(), but hard to replace
  uint m_id;
  Struct::ACodeNode* m_strct;
};


// - if x < y; 0 if x == y; + otherwise
int ANodeLineComp(ANode* x, ANode* y);


//***************************************************************************
// ADynNode
//***************************************************************************

// ---------------------------------------------------------
// ADynNode: represents dynamic nodes
// ---------------------------------------------------------
class ADynNode
  : public ANode
{
public:

  // -------------------------------------------------------
  // 
  // -------------------------------------------------------
  
  ADynNode(ANodeTy type, ANode* parent, Struct::ACodeNode* strct, uint cpId)
    : ANode(type, parent, strct),
      m_cpId(cpId),
      m_as_info(lush_assoc_info_NULL),
      m_lmId(LoadMap::LMId_NULL), m_lmIP(0), m_opIdx(0), m_lip(NULL)
  { }

  ADynNode(ANodeTy type, ANode* parent, Struct::ACodeNode* strct,
	   uint cpId, lush_assoc_info_t as_info,
	   LoadMap::LMId_t lmId, VMA ip, ushort opIdx, lush_lip_t* lip)
    : ANode(type, parent, strct),
      m_cpId(cpId),
      m_as_info(as_info),
      m_lmId(lmId), m_lmIP(ip), m_opIdx(opIdx), m_lip(lip)
  { }

  ADynNode(ANodeTy type, ANode* parent, Struct::ACodeNode* strct,
	   uint cpId, lush_assoc_info_t as_info,
	   LoadMap::LMId_t lmId, VMA ip, ushort opIdx, lush_lip_t* lip,
	   const Metric::IData& metrics)
    : ANode(type, parent, strct, metrics),
      m_cpId(cpId),
      m_as_info(as_info),
      m_lmId(lmId), m_lmIP(ip), m_opIdx(opIdx), m_lip(lip)
  { }

  virtual ~ADynNode()
  { delete m_lip; }
   
  // deep copy of internals (but without children)
  ADynNode(const ADynNode& x)
    : ANode(x),
      m_cpId(x.m_cpId),
      m_as_info(x.m_as_info),
      m_lmId(x.m_lmId),
      m_lmIP(x.m_lmIP), m_opIdx(x.m_opIdx),
      m_lip(clone_lip(x.m_lip))
  { }

  // deep copy of internals (but without children)
  ADynNode&
  operator=(const ADynNode& x)
  {
    if (this != &x) {
      ANode::operator=(x);
      m_cpId = x.m_cpId;
      m_as_info = x.m_as_info;
      m_lmId = x.m_lmId;
      m_lmIP = x.m_lmIP;
      m_opIdx = x.m_opIdx;
      delete m_lip;
      m_lip = clone_lip(x.m_lip);
    }
    return *this;
  }


  // -------------------------------------------------------
  // call path id
  // -------------------------------------------------------

  // call path id: a persistent call path id (persistent in the sense
  // that it must be consistent with hpctrace).  0 is reserved as a
  // NULL value.

  uint
  cpId() const
  { return m_cpId; }


  void
  cpId(uint id)
  { m_cpId = id; }
  

  // -------------------------------------------------------
  // logical unwinding association
  // -------------------------------------------------------

  lush_assoc_info_t
  assocInfo() const
  { return m_as_info; }

  void
  assocInfo(lush_assoc_info_t x)
  { m_as_info = x; }

  lush_assoc_t
  assoc() const
  { return lush_assoc_info__get_assoc(m_as_info); }


  // -------------------------------------------------------
  // load-module id, ip 
  // (N.B.: a load-module-ip is a static ip as opposed to a run-time ip)
  // -------------------------------------------------------

  LoadMap::LMId_t
  lmId() const
  {
    if (isValid_lip()) { return lush_lip_getLMId(m_lip); }
    return m_lmId;
  }

  LoadMap::LMId_t
  lmId_real() const
  { return m_lmId; }

  void
  lmId(LoadMap::LMId_t x)
  {
    if (isValid_lip()) { lush_lip_setLMId(m_lip, (uint16_t)x); return; }
    m_lmId = x;
  }

  void
  lmId_real(LoadMap::LMId_t x)
  { m_lmId = x; }

  virtual VMA
  lmIP() const
  {
    if (isValid_lip()) {
      return lush_lip_getLMIP(m_lip);
    }
    return m_lmIP;
  }

  VMA
  lmIP_real() const
  { return m_lmIP; }

  void
  lmIP(VMA lmIP, ushort opIdx)
  {
    if (isValid_lip()) {
      lush_lip_setLMIP(m_lip, lmIP);
      m_opIdx = 0;
      return;
    }
    m_lmIP  = lmIP;
    m_opIdx = opIdx;
  }

  ushort
  opIndex() const
  { return m_opIdx; }


  // -------------------------------------------------------
  // logical ip
  // -------------------------------------------------------

  lush_lip_t*
  lip() const
  { return m_lip; }
  
  void
  lip(const lush_lip_t* lip)
  { m_lip = const_cast<lush_lip_t*>(lip); }

  static lush_lip_t*
  clone_lip(const lush_lip_t* x)
  {
    lush_lip_t* x_clone = NULL;
    if (x) {
      x_clone = new lush_lip_t;
      memcpy(x_clone, x, sizeof(lush_lip_t));
    }
    return x_clone;
  }

  bool
  isValid_lip() const
  { return (m_lip && (lush_lip_getLMId(m_lip) != 0)); }


  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  bool
  isPrimarySynthRoot() const
  {
    return (m_lmId == LoadMap::LMId_NULL && m_lmIP == HPCRUN_FMT_LMIp_NULL);
  }


  bool
  isSecondarySynthRoot() const
  {
    return (m_lmId == LoadMap::LMId_NULL && m_lmIP == HPCRUN_FMT_LMIp_Flag1);
  }


  // -------------------------------------------------------
  // merging
  // -------------------------------------------------------

  static bool
  isMergable(const ADynNode& x, const ADynNode& y)
  {
    if (x.isLeaf() == y.isLeaf()
	&& x.lmId_real() == y.lmId_real()) {

      // 1. additional tests for standard merge condition (N.B.: order
      //    is based on early failure rather than conceptual grouping)
      if (x.lmIP_real() == y.lmIP_real()
	  && lush_lip_eq(x.lip(), y.lip())
	  && lush_assoc_class_eq(x.assoc(), y.assoc())
	  && lush_assoc_info__path_len_eq(x.assocInfo(), y.assocInfo())) {
	return true;
      }
      
      // 2. special merge condition for hpcprof-mpi when IPs have been
      //    lost after Analysis::CallPath::coalesceStmts(CallPath::Profile).
      //    (Typically, merges occur before structure information is
      //    added, so this will turn into a nop in the common case.)
      Struct::ACodeNode* x_strct = x.structure();
      Struct::ACodeNode* y_strct = y.structure();
      if (x.isLeaf() && (x_strct && y_strct) // x & y are structured leaves
	  // fast and slow cases when (a) CCT scopes obey
	  // Non-Overlapping Principle and (b) when they don't.
	  && ((x_strct == y_strct) ||
	      (x_strct->parent() == y_strct->parent()
	       && x_strct->begLine() == y_strct->begLine()) )) {
	return true;
      }
    }
    
    return false;
  }


  static bool
  hasMergeEffects(const ADynNode& x, const ADynNode& y)
  {
    // form 1: !( (x == y) || (x == 0) || (y == 0) )
    // form 2:    (x != y) && (x != 0) && (y != 0)
    return (x.cpId() != y.cpId()
	    && x.cpId() != HPCRUN_FMT_CCTNodeId_NULL
	    && y.cpId() != HPCRUN_FMT_CCTNodeId_NULL);
  }


  virtual MergeEffect
  mergeMe(const ANode& y, MergeContext* mrgCtxt = NULL, uint metricBegIdx = 0, bool mayConflict = true);
  

  // -------------------------------------------------------
  // Dump contents for inspection
  // -------------------------------------------------------

  std::string
  assocInfo_str() const;
  
  std::string
  lip_str() const;

  std::string
  nameDyn() const;

  void
  writeDyn(std::ostream& os, uint oFlags = 0, const char* prefix = "") const;


private:
  uint m_cpId; // dynamic id

  lush_assoc_info_t m_as_info;

  LoadMap::LMId_t m_lmId; // LoadMap::LM id
  VMA    m_lmIP;           // static instruction pointer
  ushort m_opIdx;          // index in the instruction [OBSOLETE]

  lush_lip_t* m_lip; // logical ip
};


//***************************************************************************
// AProcNode
//***************************************************************************

// ---------------------------------------------------------
// AProcNode: represents procedure nodes
// ---------------------------------------------------------
class AProcNode
  : public ANode
{
public:

  // -------------------------------------------------------
  // 
  // -------------------------------------------------------
  
  AProcNode(ANodeTy type, ANode* parent, Struct::ACodeNode* strct)
    : ANode(type, parent, strct)
  { }

  virtual ~AProcNode()
  { }
   
  // deep copy of internals (but without children)
  AProcNode(const AProcNode& x)
    : ANode(x)
  { }

  // deep copy of internals (but without children)
  AProcNode&
  operator=(const AProcNode& x)
  {
    if (this != &x) {
      ANode::operator=(x);
    }
    return *this;
  }


  // --------------------------------------------------------
  // Static structure for ProcFrm/Proc
  // --------------------------------------------------------

  virtual const std::string&
  lmName() const
  { return (m_strct) ? m_strct->ancestorLM()->name() : BOGUS; }

  virtual uint
  lmId() const
  { return (m_strct) ? m_strct->ancestorLM()->id() : 0; }


  virtual const std::string&
  fileName() const
  {
    if (m_strct) {
      return (isAlien()) ?
	static_cast<Struct::Alien*>(m_strct)->fileName() :
	m_strct->ancestorFile()->name();
    }
    else {
      return BOGUS;
    }
  }

  virtual uint
  fileId() const
  {
    uint id = 0;
    if (m_strct) {
      id = (isAlien()) ? m_strct->id() : m_strct->ancestorFile()->id();
    }
    return id;
  }

  virtual const std::string&
  procName() const
  { return (m_strct) ? m_strct->name() : BOGUS; }

  virtual uint
  procId() const
  { return (m_strct) ? m_strct->id() : 0; }


  virtual bool
  isAlien() const = 0;


protected:
  static std::string BOGUS;

private:
};


//***************************************************************************
// 
//***************************************************************************

// --------------------------------------------------------------------------
// Root
// --------------------------------------------------------------------------

class Root
  : public ANode
{
public:
  // Constructor/Destructor
  Root(const std::string& nm)
    : ANode(TyRoot, NULL),
      m_name(nm)
  { }

  Root(const char* nm)
    : ANode(TyRoot, NULL),
      m_name((nm) ? nm : "")
  { }

  virtual ~Root()
  { }

  const std::string&
  name() const { return m_name; }
  
  // Dump contents for inspection
  virtual std::string
  toStringMe(uint oFlags = 0) const;
  
protected:
private:
  std::string m_name; // the program name
};


// --------------------------------------------------------------------------
// ProcFrm
// --------------------------------------------------------------------------

class ProcFrm
  : public AProcNode
{
public:
  // Constructor/Destructor
  ProcFrm(ANode* parent, Struct::ACodeNode* strct = NULL)
    : AProcNode(TyProcFrm, parent, strct)
  { }

  virtual ~ProcFrm()
  { }

  // shallow copy (in the sense the children are not copied)
  ProcFrm(const ProcFrm& x)
    : AProcNode(x)
  { }


  // -------------------------------------------------------
  // Static structure (NOTE: m_strct is always Struct::Proc)
  // -------------------------------------------------------

  virtual const std::string&
  fileName() const
  { return (m_strct) ? m_strct->ancestorFile()->name() : BOGUS; }

  virtual uint
  fileId() const
  { return (m_strct) ? m_strct->ancestorFile()->id() : 0; }

  virtual bool
  isAlien() const
  { return false; }

  std::string
  procNameDbg() const;


  // -------------------------------------------------------
  //
  // -------------------------------------------------------

  virtual std::string
  toStringMe(uint oFlags = 0) const;

  virtual std::string
  codeName() const;

private:
};


// --------------------------------------------------------------------------
// Proc:
// --------------------------------------------------------------------------

class Proc
  : public AProcNode
{
public:
  // Constructor/Destructor
  Proc(ANode* parent, Struct::ACodeNode* strct = NULL)
    : AProcNode(TyProc, parent, strct)
  { }
  
  virtual ~Proc()
  { }
  

  // -------------------------------------------------------
  // Static structure (NOTE: m_strct is either Struct::Proc or Struct::Alien)
  // -------------------------------------------------------

  virtual bool
  isAlien() const
  { return (m_strct && typeid(*m_strct) == typeid(Struct::Alien)); }

  
  // -------------------------------------------------------
  //
  // -------------------------------------------------------

  virtual std::string
  toStringMe(uint oFlags = 0) const;

private:
};


// --------------------------------------------------------------------------
// Loop
// --------------------------------------------------------------------------

class Loop
  : public ANode
{
public:
  // Constructor/Destructor
  Loop(ANode* parent, Struct::ACodeNode* strct = NULL)
    : ANode(TyLoop, parent, strct)
  { }

  virtual uint
  fileId() const
  {
    uint id = 0;
    if (m_strct) {
      id = m_strct->id(); 
    }
    return id;
  }

  virtual ~Loop()
  { }

  // Dump contents for inspection
  virtual std::string
  toStringMe(uint oFlags = 0) const;
  
private:
};


// --------------------------------------------------------------------------
// Call (callsite)
// --------------------------------------------------------------------------

class Call
  : public ADynNode
{
public:
  // Constructor/Destructor
  Call(ANode* parent, uint cpId)
    : ADynNode(TyCall, parent, NULL, cpId)
  { }

  Call(ANode* parent,
       uint cpId, lush_assoc_info_t as_info,
       LoadMap::LMId_t lmId, VMA ip, ushort opIdx, lush_lip_t* lip,
       const Metric::IData& metrics)
    : ADynNode(TyCall, parent, NULL,
	       cpId, as_info, lmId, ip, opIdx, lip,
	       metrics)
  { }
  
  virtual ~Call()
  { }
  
  // Node data
  virtual VMA
  lmIP() const
  {
    VMA ip = ADynNode::lmIP_real();
    if (isValid_lip()) {
      ip = lush_lip_getLMIP(lip());
    }
    return (ip != 0) ? (ip - s_raToCallsiteOfst) : 0;
  }

  // lmRA(): static (as opposed to run-time) return address
  VMA
  lmRA() const
  { return ADynNode::lmIP_real(); }
  
  // Dump contents for inspection
  virtual std::string
  toStringMe(uint oFlags = 0) const;

};


// --------------------------------------------------------------------------
// Stmt
// --------------------------------------------------------------------------
  
class Stmt
  : public ADynNode
{
 public:
  // Constructor/Destructor
  Stmt(ANode* parent, uint cpId)
    : ADynNode(TyStmt, parent, NULL, cpId)
  { }

  Stmt(ANode* parent,
       uint cpId, lush_assoc_info_t as_info,
       LoadMap::LMId_t lmId, VMA ip, ushort opIdx, lush_lip_t* lip,
       const Metric::IData& metrics)
    : ADynNode(TyStmt, parent, NULL,
	       cpId, as_info, lmId, ip, opIdx, lip,
	       metrics)
  { }
  
  virtual ~Stmt()
  { }

  Stmt&
  operator=(const Stmt& x)
  {
    if (this != &x) {
      ADynNode::operator=(x);
    }
    return *this;
  }

  // Dump contents for inspection
  virtual std::string
  toStringMe(uint oFlags = 0) const;
};


} // namespace CCT

} // namespace Prof


//***************************************************************************

#include "CCT-TreeIterator.hpp"

//***************************************************************************


#endif /* prof_Prof_CCT_Tree_hpp */
