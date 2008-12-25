// -*-Mode: C++;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002-2007, Rice University 
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
//   $Source$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef prof_juicy_Prof_CCT_Tree_hpp 
#define prof_juicy_Prof_CCT_Tree_hpp

//************************* System Include Files ****************************

#include <iostream>
#include <vector>
#include <string>

#include <cstring> // for memcpy

//*************************** User Include Files ****************************

#include <include/general.h>

#include "Struct-Tree.hpp"
#include "MetricDesc.hpp"
#include "Epoch.hpp"

#include <lib/isa/ISATypes.hpp>

#include <lib/prof-lean/lush/lush-support.h>
#include <lib/prof-lean/hpcfile_cstree.h>

#include <lib/xml/xml.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/NonUniformDegreeTree.hpp>
#include <lib/support/SrcFile.hpp>
#include <lib/support/Unique.hpp>

//*************************** Forward Declarations ***************************

// Hack for interpreting Cilk-like LIPs.
#define FIXME_CILK_LIP_HACK
#define DBG_LUSH_PROC_FRAME 0

inline std::ostream&
operator<<(std::ostream& os, const hpcfile_metric_data_t x)
{
  os << "[" << x.i << " " << x.r << "]";
  return os;
}

#define FIXME_WRITE_CCT_DICTIONARIES 0

//***************************************************************************
// Tree
//***************************************************************************

namespace Prof {

namespace CallPath {
  class Profile;
}
class CSProfNode;

namespace CCT {


class Tree: public Unique {
public:
  enum {
    // User-level bit flags
    XML_FALSE =	(0 << 0),	/* No XML format */
    XML_TRUE  =	(1 << 0),	/* XML format */

    COMPRESSED_OUTPUT = (1 << 1),  /* no indentation */
    DBG_OUTPUT = (1 << 2),         /* show source line info for loops/stmts */

    // Not-generally-user-level bit flags
    XML_NO_ESC_CHARS = (1 << 10), /* don't substitute XML escape characters */

    // Private bit flags
    XML_EMPTY_TAG    = (1 << 15)  /* this is an empty XML tag */
    
  };

public:
  // -------------------------------------------------------
  // Constructor/Destructor
  // -------------------------------------------------------
  
  Tree(const CallPath::Profile* metadata); // FIXME: metrics and epoch

  virtual ~Tree();

  // -------------------------------------------------------
  // Tree data
  // -------------------------------------------------------
  CSProfNode* root() const { return m_root; }
  void        root(CSProfNode* x) { m_root = x; }

  bool empty() const { return (m_root == NULL); }

  const CallPath::Profile* metadata() { return m_metadata; }
  
  // -------------------------------------------------------
  // Given a Tree, merge into 'this'
  // -------------------------------------------------------
  void merge(const Tree* y, 
	     const SampledMetricDescVec* new_mdesc, 
	     uint x_numMetrics, uint y_numMetrics);

  // -------------------------------------------------------
  // Write contents
  // -------------------------------------------------------
  std::ostream& 
  writeXML(std::ostream& os = std::cerr, int dmpFlag = XML_TRUE) const;

  std::ostream& 
  dump(std::ostream& os = std::cerr, int dmpFlag = XML_TRUE) const;
  
  void ddump() const;


  // Given a set of flags 'dmpFlag', determines whether we need to
  // ensure that certain characters are escaped.  Returns xml::ESC_TRUE
  // or xml::ESC_FALSE. 
  static int doXMLEscape(int dmpFlag);
 
private:
  CSProfNode* m_root;
  const CallPath::Profile* m_metadata; // does not own
};


} // namespace CCT

} // namespace Prof


//***************************************************************************
// CSProfNode
//***************************************************************************

namespace Prof {

  //namespace CCT {


class CSProfNode;     // Everyone's base class 

class CSProfPgmNode; 
class CSProfCallSiteNode;
class CSProfLoopNode; 
class CSProfStmtRangeNode; 

class CSProfProcedureFrameNode;
class CSProfStatementNode;

// ---------------------------------------------------------
// CSProfNode: The base node for a call stack profile tree.
// ---------------------------------------------------------
class CSProfNode: public NonUniformDegreeTreeNode, public Unique {
public:
  enum NodeType {
    PGM,
    PROCEDURE_FRAME,
    LOOP,
    STATEMENT,
    CALLSITE,
    STMT_RANGE,
    ANY,
    NUMBER_OF_TYPES
  };
  
  static const std::string& NodeTypeToName(NodeType tp);
  static NodeType           IntToNodeType(long i);

private:
  static const std::string NodeNames[NUMBER_OF_TYPES];
  
public:
  CSProfNode(NodeType t, CSProfNode* _parent, Struct::ACodeNode* strct = NULL);
  virtual ~CSProfNode();
  
  // shallow copy (in the sense the children are not copied)
  CSProfNode(const CSProfNode& x)
    : m_type(x.m_type), m_strct(x.m_strct) // do not copy 'm_uid'
  { 
    ZeroLinks();
  }

  // --------------------------------------------------------
  // General Interface to fields 
  // --------------------------------------------------------
  NodeType type() const { return m_type; }

  // id: a unique id; 0 is reserved for a NULL value
  uint id() const { return m_uid; }

  // 'name()' is overridden by some derived classes
  virtual const std::string& name() const { return NodeTypeToName(type()); }
  
  // --------------------------------------------------------
  // Tree navigation 
  // --------------------------------------------------------
  CSProfNode* Parent() const 
    { return dynamic_cast<CSProfNode*>(NonUniformDegreeTreeNode::Parent()); }

  CSProfNode* FirstChild() const
    { return dynamic_cast<CSProfNode*>
	(NonUniformDegreeTreeNode::FirstChild()); }

  CSProfNode* LastChild() const
    { return dynamic_cast<CSProfNode*>
	(NonUniformDegreeTreeNode::LastChild()); }

  CSProfNode* NextSibling() const;
  CSProfNode* PrevSibling() const;

  bool IsLeaf() const { return (FirstChild() == NULL); }
  
  // --------------------------------------------------------
  // Ancestor: find first node in path from this to root with given type
  // --------------------------------------------------------
  // a node may be an ancestor of itself
  CSProfNode*          Ancestor(NodeType tp) const;
  
  CSProfPgmNode*       AncestorPgm() const;
  CSProfCallSiteNode*  AncestorCallSite() const;
  CSProfLoopNode*      AncestorLoop() const;
  CSProfStmtRangeNode* AncestorStmtRange() const;
  CSProfProcedureFrameNode* AncestorProcedureFrame() const; // CC
  CSProfStatementNode* AncestorStatement() const; // CC


  // --------------------------------------------------------
  // 
  // --------------------------------------------------------

  void 
  merge_prepare(uint numMetrics);

  void 
  merge(CSProfNode* y, const SampledMetricDescVec* new_mdesc,
	uint x_numMetrics, uint y_numMetrics);

  CSProfNode* 
  findDynChild(lush_assoc_info_t as_info, 
	       Epoch::LM_id_t lm_id, VMA ip, lush_lip_t* lip);


  // merge y into 'this'
  void 
  merge_node(CSProfNode* y);


  // --------------------------------------------------------
  // Static structure
  // --------------------------------------------------------

  // structure: static structure id for this node; the same static
  // structure will have the same structure().
  Struct::ACodeNode* 
  structure() const  { return m_strct; }

  void 
  structure(Struct::ACodeNode* strct) { m_strct = strct; }

  uint 
  structureId() const { return (m_strct) ? m_strct->id() : 0; }

  SrcFile::ln 
  begLine() const { return (m_strct) ? m_strct->begLine() : ln_NULL; }

  SrcFile::ln 
  endLine() const { return (m_strct) ? m_strct->endLine() : ln_NULL;  }


  // --------------------------------------------------------
  // Dump contents for inspection
  // --------------------------------------------------------
  virtual std::string Types() const; // this instance's base and derived types

  virtual std::string toString_me(int dmpFlag = CCT::Tree::XML_TRUE) const; 
  
  std::ostream& 
  writeXML(std::ostream& os = std::cerr, int dmpFlag = CCT::Tree::XML_TRUE,
	   const char *pre = "") const;

  std::ostream& 
  dump(std::ostream& os = std::cerr, int dmpFlag = CCT::Tree::XML_TRUE,
       const char *pre = "") const;

  void ddump() const;
  
  virtual std::string codeName() const;

protected:

  void writeXML_pre(std::ostream& os = std::cerr, 
		    int dmpFlag = CCT::Tree::XML_TRUE,
		    const char *prefix = "") const;
  void writeXML_post(std::ostream& os = std::cerr, 
		     int dmpFlag = CCT::Tree::XML_TRUE,
		     const char *prefix = "") const;

  void merge_fixup(const SampledMetricDescVec* mdesc, int metric_offset);
  
protected:
  // private: 
  NodeType m_type;
  uint m_uid; 
  Struct::ACodeNode* m_strct;
};


// - if x < y; 0 if x == y; + otherwise
int CSProfNodeLineComp(CSProfNode* x, CSProfNode* y);


//***************************************************************************
// IDynNode
//***************************************************************************

// IDynNode: a mixin interface representing dynamic nodes

class IDynNode {
public:

  // -------------------------------------------------------
  // 
  // -------------------------------------------------------
  
  IDynNode(CSProfNode* proxy, 
	   uint32_t cpid,
	   const SampledMetricDescVec* metricdesc)
    : m_proxy(proxy),
      m_as_info(lush_assoc_info_NULL), 
      m_lmId(Epoch::LM_id_NULL), m_ip(0), m_opIdx(0), m_lip(NULL), m_cpid(cpid),
      m_metricdesc(metricdesc)
    { }

  IDynNode(CSProfNode* proxy, 
	   lush_assoc_info_t as_info, VMA ip, ushort opIdx, lush_lip_t* lip,
	   uint32_t cpid,
	   const SampledMetricDescVec* metricdesc)
    : m_proxy(proxy), 
      m_as_info(as_info), 
      m_lmId(Epoch::LM_id_NULL), m_ip(ip), m_opIdx(opIdx), m_lip(lip), m_cpid(cpid),
      m_metricdesc(metricdesc)
    { }

  IDynNode(CSProfNode* proxy, 
	   lush_assoc_info_t as_info, VMA ip, ushort opIdx, lush_lip_t* lip,
	   uint32_t cpid,
	   const SampledMetricDescVec* metricdesc,
	   std::vector<hpcfile_metric_data_t>& metrics)
    : m_proxy(proxy),
      m_as_info(as_info), 
      m_lmId(Epoch::LM_id_NULL), m_ip(ip), m_opIdx(opIdx), m_lip(lip), m_cpid(cpid),
      m_metricdesc(metricdesc), m_metrics(metrics) 
    { }

  virtual ~IDynNode() {
    delete_lip(m_lip);
  }
   
  // deep copy
  IDynNode(const IDynNode& x)
    : //m_proxy(x.m_proxy),
      m_as_info(x.m_as_info), 
      m_lmId(x.m_lmId),
      m_ip(x.m_ip), m_opIdx(x.m_opIdx), 
      m_lip(clone_lip(x.m_lip)),
      m_cpid(x.m_cpid),
      m_metricdesc(x.m_metricdesc),
      m_metrics(x.m_metrics) 
    { }

  // deep copy
  IDynNode& operator=(const IDynNode& x) {
    if (this != &x) {
      //m_proxy = x.m_proxy;
      m_as_info = x.m_as_info;
      m_lmId = x.m_lmId;
      m_ip = x.m_ip;
      m_opIdx = x.m_opIdx;
      delete_lip(m_lip);
      m_lip = clone_lip(x.m_lip);
      m_cpid = x.m_cpid;
      m_metricdesc = x.m_metricdesc;
      m_metrics = x.m_metrics;
    }
    return *this;
  }


  // -------------------------------------------------------
  // 
  // -------------------------------------------------------
  CSProfNode* proxy() const { return m_proxy; }

  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  lush_assoc_info_t assocInfo() const 
    { return m_as_info; }
  void assocInfo(lush_assoc_info_t x) 
    { m_as_info = x; }

  lush_assoc_t assoc() const 
  { return lush_assoc_info__get_assoc(m_as_info); }

  Epoch::LM_id_t lm_id() const 
    { return m_lmId; }
  void lm_id(Epoch::LM_id_t x)
    { m_lmId = x; }

  virtual VMA ip() const 
  {
#ifdef FIXME_CILK_LIP_HACK
    if (lip_cilk_isvalid()) { return lip_cilk(); }
#endif
    return m_ip; 
  }

  void ip(VMA ip, ushort opIdx) 
  { 
#ifdef FIXME_CILK_LIP_HACK
    if (lip_cilk_isvalid()) { lip_cilk(ip); return; }
#endif
    m_ip = ip; m_opIdx = opIdx; 
  }

  ushort opIndex() const 
  { return m_opIdx; }


#ifdef FIXME_CILK_LIP_HACK
  VMA ip_real() const 
  { return m_ip; }

  bool lip_cilk_isvalid() const 
  { return m_lip && (lip_cilk() != 0) && (lip_cilk() != (VMA)m_lip); }

  VMA lip_cilk() const 
  { return m_lip->data8[0]; }
  
  void lip_cilk(VMA ip) 
  { m_lip->data8[0] = ip; }
#endif


  lush_lip_t* lip() const 
    { return m_lip; }
  void lip(lush_lip_t* lip) 
    { m_lip = lip; }

  static lush_lip_t* clone_lip(lush_lip_t* x) 
  {
    lush_lip_t* x_clone = NULL;
    if (x) {
      // NOTE: for consistency with hpcfile_alloc_CB
      size_t sz = sizeof(lush_lip_t);
      x_clone = (lush_lip_t*) new char[sz];
      memcpy(x_clone, x, sz);
    }
    return x_clone;
  }

  static void delete_lip(lush_lip_t* x) 
  { delete[] (char*)x; }

  uint32_t cpid() const 
    { return m_cpid; }

  hpcfile_metric_data_t metric(int i) const 
    { return m_metrics[i]; }
  uint numMetrics() const 
    { return m_metrics.size(); }

  void metricIncr(int i, hpcfile_metric_data_t incr)  
    { IDynNode::metricIncr((*m_metricdesc)[i], &m_metrics[i], incr); }
  void metricDecr(int i, hpcfile_metric_data_t decr)  
    { IDynNode::metricDecr((*m_metricdesc)[i], &m_metrics[i], decr); }


  const SampledMetricDescVec* metricdesc() const 
    { return m_metricdesc; }
  void metricdesc(const SampledMetricDescVec* x) 
    { m_metricdesc = x; }


  // -------------------------------------------------------
  // 
  // -------------------------------------------------------

  void mergeMetrics(const IDynNode& y, uint beg_idx = 0);
  void appendMetrics(const IDynNode& y);

  void expandMetrics_before(uint offset);
  void expandMetrics_after(uint offset);

  static inline void
  metricIncr(const SampledMetricDesc* mdesc, 
	     hpcfile_metric_data_t* x, hpcfile_metric_data_t incr)
  {
    if (hpcfile_csprof_metric_is_flag(mdesc->flags(), 
				      HPCFILE_METRIC_FLAG_REAL)) {
      x->r += incr.r;
    }
    else {
      x->i += incr.i;
    }
  }
  
  static inline void
  metricDecr(const SampledMetricDesc* mdesc, 
	     hpcfile_metric_data_t* x, hpcfile_metric_data_t decr)
  {
    if (hpcfile_csprof_metric_is_flag(mdesc->flags(), 
				      HPCFILE_METRIC_FLAG_REAL)) {
      x->r -= decr.r;
    }
    else {
      x->i -= decr.i;
    }
  }

  // -------------------------------------------------------
  // Dump contents for inspection
  // -------------------------------------------------------

  virtual std::string toString(const char* pre = "") const;

  std::string assocInfo_str() const;
  std::string lip_str() const;

  bool 
  hasMetrics() const 
  {
    for (uint i = 0; i < numMetrics(); i++) {
      hpcfile_metric_data_t m = metric(i);
      if (!hpcfile_metric_data_iszero(m)) {
	return true;
      }
    }
    return false;
  }

  // writeMetricsXML: write metrics (sparsely)
  virtual void writeMetricsXML(std::ostream& os, 
			       int dmpFlag = 0, const char* prefix = "") const;

  struct WriteMetricInfo_ {
    const SampledMetricDesc* mdesc;
    hpcfile_metric_data_t x;
  };
      
  static inline WriteMetricInfo_
  writeMetric(const SampledMetricDesc* mdesc, hpcfile_metric_data_t x) 
  {
    WriteMetricInfo_ info;
    info.mdesc = mdesc;
    info.x = x;
    return info;
  }
  
  
  virtual void dump(std::ostream& os = std::cerr, 
		    const char* pre = "") const;

  virtual void ddump() const;

private:
  CSProfNode* m_proxy;

  lush_assoc_info_t m_as_info;

  Epoch::LM_id_t m_lmId; // Epoch::LM id
  VMA m_ip;       // instruction pointer for this node
  ushort m_opIdx; // index in the instruction [OBSOLETE]

  lush_lip_t* m_lip; // lush logical ip

  uint32_t m_cpid; // call path node handle for tracing

  // FIXME: convert to metric-id a la Metric::Mgr
  const SampledMetricDescVec*        m_metricdesc; // does not own memory

  // FIXME: a vector of doubles, a la ScopeInfo
  std::vector<hpcfile_metric_data_t> m_metrics;
};


inline std::ostream&
operator<<(std::ostream& os, const IDynNode::WriteMetricInfo_& info)
{
  if (hpcfile_csprof_metric_is_flag(info.mdesc->flags(), 
				    HPCFILE_METRIC_FLAG_REAL)) {
    os << xml::MakeAttrNum(info.x.r);
  }
  else {
    os << xml::MakeAttrNum(info.x.i);
  }
  return os;
}


//***************************************************************************
// 
//***************************************************************************

// --------------------------------------------------------------------------
// CSProfPgmNode
// --------------------------------------------------------------------------

class CSProfPgmNode: public CSProfNode {
public: 
  // Constructor/Destructor
  CSProfPgmNode(const char* nm);
  virtual ~CSProfPgmNode();

  const std::string& name() const { return m_name; }
                                        
  void Freeze() { frozen = true;} // disallow additions to/deletions from tree
  bool IsFrozen() const { return frozen; }
  
  // Dump contents for inspection
  virtual std::string toString_me(int dmpFlag = CCT::Tree::XML_TRUE) const;
  
protected: 
private: 
  bool frozen;
  std::string m_name; // the program name
}; 


// --------------------------------------------------------------------------
// CSProfProcedureFrameNode
// --------------------------------------------------------------------------

class CSProfProcedureFrameNode: public CSProfNode {
public:
  // Constructor/Destructor
  CSProfProcedureFrameNode(CSProfNode* _parent);
  virtual ~CSProfProcedureFrameNode();

  // shallow copy (in the sense the children are not copied)
  CSProfProcedureFrameNode(const CSProfProcedureFrameNode& x)
    : CSProfNode(x)
    { }

  // -------------------------------------------------------
  // Static structure
  // -------------------------------------------------------
  // NOTE: m_strct is either Struct::Proc or Struct::Alien

  const std::string& 
  lmName() const { 
    if (m_strct) { 
      return m_strct->AncLM()->name();
    }
    else {
      return BOGUS; 
    }
  }

  uint 
  lmId() const { 
    return (m_strct) ? m_strct->AncLM()->id() : 0;
  }


  const std::string& 
  fileName() const {
    if (m_strct) {
      return (isAlien()) ? 
	dynamic_cast<Struct::Alien*>(m_strct)->fileName() :
	m_strct->AncFile()->name();
    }
    else {
      return BOGUS;
    }
  }

  uint 
  fileId() const {
    uint id = 0;
    if (m_strct) {
      id = (isAlien()) ? m_strct->id() : m_strct->AncFile()->id();
    }
    return id;
  }

  void 
  fileNameXXX(const std::string& fnm) {
    if (m_strct) { 
      if (isAlien()) { 
	dynamic_cast<Struct::Alien*>(m_strct)->fileName(fnm); 
      }
      else { 
	m_strct->AncFile()->name(fnm);
      }
    }
  }


  const std::string& 
  procName() const { 
    // Struct::Proc or Struct::Alien
    if (m_strct) { 
      return m_strct->name();
    }
    else {
      return BOGUS; 
    }
  }

#if (DBG_LUSH_PROC_FRAME)
      std::string nm = pctxtStrct->name();
      if (n_dyn && (n_dyn->assoc() != LUSH_ASSOC_NULL)) {
	nm += " (" + StrUtil::toStr(n_dyn->ip_real(), 16) 
	  + ", " + n_dyn->lip_str() + ") [" + n_dyn->assocInfo_str() + "]";
      }
#endif


  void
  procNameXXX(const std::string& pnm) const { 
    // Struct::Proc or Struct::Alien
    if (m_strct) { 
      if (isAlien()) {
	dynamic_cast<Struct::Alien*>(m_strct)->name(pnm);
      }
      else {
	dynamic_cast<Struct::Proc*>(m_strct)->name(pnm);
      }
    }
  }

  uint 
  procId() const { 
    return (m_strct) ? m_strct->id() : 0;
  }


  // Alien
  bool isAlien() const {
    return (m_strct && m_strct->Type() == Struct::ANode::TyALIEN);
  }

  // -------------------------------------------------------
  //
  // -------------------------------------------------------

  virtual std::string toString_me(int dmpFlag = CCT::Tree::XML_TRUE) const;

  virtual std::string codeName() const;
 
private: 
  static std::string BOGUS;
};


// --------------------------------------------------------------------------
// CSProfLoopNode
// --------------------------------------------------------------------------

class CSProfLoopNode: public CSProfNode {
public: 
  // Constructor/Destructor
  CSProfLoopNode(CSProfNode* _parent, Struct::ACodeNode* strct = NULL);
  ~CSProfLoopNode();

  // Dump contents for inspection
  virtual std::string toString_me(int dmpFlag = CCT::Tree::XML_TRUE) const; 
  
private:
};


// --------------------------------------------------------------------------
// CSProfStatementNode
// --------------------------------------------------------------------------
  
class CSProfStatementNode: public CSProfNode, public IDynNode {
 public:
  // Constructor/Destructor
  CSProfStatementNode(CSProfNode* _parent, 
		      uint32_t cpid,
		      const SampledMetricDescVec* metricdesc);
  virtual ~CSProfStatementNode();

  void operator=(const CSProfStatementNode& x);
  void operator=(const CSProfCallSiteNode& x);

  // Dump contents for inspection
  virtual std::string toString_me(int dmpFlag = CCT::Tree::XML_TRUE) const;
};


// --------------------------------------------------------------------------
// CSProfCallSiteNode
// --------------------------------------------------------------------------

class CSProfCallSiteNode: public CSProfNode, public IDynNode {
public:
  // Constructor/Destructor
  CSProfCallSiteNode(CSProfNode* _parent,
		     uint32_t cpid,
		     const SampledMetricDescVec* metricdesc);
  CSProfCallSiteNode(CSProfNode* _parent, 
		     lush_assoc_info_t as_info,
		     VMA ip, ushort opIdx, 
		     lush_lip_t* lip,
		     uint32_t cpid,
		     const SampledMetricDescVec* metricdesc,
		     std::vector<hpcfile_metric_data_t>& metrics);
  virtual ~CSProfCallSiteNode();
  
  // Node data
  virtual VMA ip() const 
  { 
#ifdef FIXME_CILK_LIP_HACK
    if (lip_cilk_isvalid()) { return lip_cilk(); }
#endif    
    return (IDynNode::ip_real() - 1); 
  }
  
  VMA ra() const { return IDynNode::ip_real(); }
    
  // Dump contents for inspection
  virtual std::string toString_me(int dmpFlag = CCT::Tree::XML_TRUE) const;

};


// --------------------------------------------------------------------------
// CSProfStmtRangeNode: FIXME: Not used!
// --------------------------------------------------------------------------

class CSProfStmtRangeNode: public CSProfNode {
public: 
  // Constructor/Destructor
  CSProfStmtRangeNode(CSProfNode* _parent, Struct::ACodeNode* strct = NULL);
  ~CSProfStmtRangeNode();
  
  // Dump contents for inspection
  virtual std::string toString_me(int dmpFlag = CCT::Tree::XML_TRUE) const;
};


  //} // namespace CCT

} // namespace Prof


//***************************************************************************

#include "CCT-TreeIterator.hpp"

//***************************************************************************


#endif /* prof_juicy_Prof_CCT_Tree_hpp */
