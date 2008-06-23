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

#include <string.h> // for memcpy

#include <iostream>
#include <vector>
#include <string>

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
using SrcFile::ln_NULL;
#include <lib/support/Unique.hpp>

//*************************** Forward Declarations ***************************

// Hack for interpreting Cilk-like LIPs.
#define FIXME_CILK_LIP_HACK

inline std::ostream&
operator<<(std::ostream& os, const hpcfile_metric_data_t x)
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
class CSProfNode;

namespace CCT {


class Tree: public Unique {
public:
  enum {
    // User-level bit flags
    XML_FALSE =	(0 << 0),	/* No XML format */
    XML_TRUE  =	(1 << 0),	/* XML format */

    COMPRESSED_OUTPUT = (1 << 1),  /* Use compressed output format */

    // Not-generally-user-level bit flags
    XML_NO_ESC_CHARS = (1 << 10), /* don't substitute XML escape characters */

    // Private bit flags
    XML_EMPTY_TAG    = (1 << 15)  /* this is an empty XML tag */
    
  };

public:
  // Constructor/Destructor

  // FIXME: metadata should be metrics and epoch, not 
  Tree(const CallPath::Profile* metadata);
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
  // Dump contents for inspection
  // -------------------------------------------------------
  virtual void dump(std::ostream& os = std::cerr, 
		    int dmpFlag = XML_TRUE) const;
  virtual void ddump() const;


  // Given a set of flags 'dmpFlag', determines whether we need to
  // ensure that certain characters are escaped.  Returns xml::ESC_TRUE
  // or xml::ESC_FALSE. 
  static int AddXMLEscapeChars(int dmpFlag);
 
private:
  CSProfNode* m_root;
  const CallPath::Profile* m_metadata; // does not own
};


} // namespace CCT

} // namespace Prof


//***************************************************************************
// CSProfNode, CSProfCodeNode.
//***************************************************************************

// tallent: wait before using Prof::CCT
namespace Prof {


class CSProfNode;     // Everyone's base class 
class CSProfCodeNode; // Base class for describing source code

class CSProfPgmNode; 
class CSProfGroupNode; 
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
    GROUP,
    CALLSITE,
    LOOP,
    STMT_RANGE,
    PROCEDURE_FRAME,
    STATEMENT,
    ANY,
    NUMBER_OF_TYPES
  };
  
  static const std::string& NodeTypeToName(NodeType tp);
  static NodeType           IntToNodeType(long i);

private:
  static const std::string NodeNames[NUMBER_OF_TYPES];
  
public:
  CSProfNode(NodeType t, CSProfNode* _parent);
  virtual ~CSProfNode();
  
  // --------------------------------------------------------
  // General Interface to fields 
  // --------------------------------------------------------
  NodeType GetType() const     { return type; }
  uint     GetUniqueId() const { return uid; }

  // 'GetName()' is overridden by some derived classes
  virtual const std::string& GetName() const { return NodeTypeToName(GetType()); }
  
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
  CSProfGroupNode*     AncestorGroup() const;
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

  // --------------------------------------------------------
  // Dump contents for inspection
  // --------------------------------------------------------
  virtual std::string ToDumpString(int dmpFlag = CCT::Tree::XML_TRUE) const; 

  virtual std::string Types(); // lists this instance's base and derived types 
  
  void DumpSelfBefore(std::ostream& os = std::cerr, 
		      int dmpFlag = CCT::Tree::XML_TRUE,
		      const char *prefix = "") const;
  void DumpSelfAfter (std::ostream& os = std::cerr, 
		      int dmpFlag = CCT::Tree::XML_TRUE,
		      const char *prefix = "") const;
  void Dump          (std::ostream& os = std::cerr, 
		      int dmpFlag = CCT::Tree::XML_TRUE,
		      const char *pre = "") const;
  void DumpLineSorted(std::ostream& os = std::cerr, 
		      int dmpFlag = CCT::Tree::XML_TRUE,
		      const char *pre = "") const;
  
  void DDump();
  void DDumpSort();

protected:
  void merge_fixup(const SampledMetricDescVec* mdesc, int metric_offset);
  
protected:
  // private: 
  NodeType type;
  uint uid; 
};


// ---------------------------------------------------------
// CSProfCodeNode is a base class for all CSProfNode's that describe
// elements of source files (code, e.g., loops, statements).
// ---------------------------------------------------------
class CSProfCodeNode : public CSProfNode {
protected: 
  CSProfCodeNode(NodeType t, CSProfNode* _parent, 
		 SrcFile::ln begLn = ln_NULL, SrcFile::ln endLn = ln_NULL,
		 uint sId = 0);
  
public: 
  virtual ~CSProfCodeNode();
  
  // Node data
  SrcFile::ln GetBegLine() const { return begLine; } // in source code
  SrcFile::ln GetEndLine() const { return endLine; } // in source code
  
  bool            ContainsLine(SrcFile::ln ln) const; 
  CSProfCodeNode* CSProfCodeNodeWithLine(SrcFile::ln ln) const;

  void SetLineRange(SrcFile::ln begLn, SrcFile::ln endLn); // use carefully!

  // tallent: I made this stuff virtual in order to handle both
  // CSProfCallSiteNode/CSProfProcedureFrameNode or
  // CSProfStatementNode in the same way.  FIXME: The abstractions
  // need to be fixed! Classes had been duplicated with impunity; I'm
  // just trying to contain the mess.
  virtual const std::string& GetFile() const
    { DIAG_Die(DIAG_Unimplemented); return BOGUS; }
  virtual void SetFile(const char* fnm) 
    { DIAG_Die(DIAG_Unimplemented); }
  virtual void SetFile(const std::string& fnm)
    { DIAG_Die(DIAG_Unimplemented); }

  virtual const std::string& GetProc() const 
    { DIAG_Die(DIAG_Unimplemented); return BOGUS; }
  virtual void SetProc(const char* pnm) 
    { DIAG_Die(DIAG_Unimplemented); }
  virtual void SetProc(const std::string& pnm)
    { DIAG_Die(DIAG_Unimplemented); }

  virtual void SetLine(SrcFile::ln ln) 
    { DIAG_Die(DIAG_Unimplemented); } 
 
  virtual bool FileIsText() const
    { DIAG_Die(DIAG_Unimplemented); return false; }
  virtual void SetFileIsText(bool bi) 
    { DIAG_Die(DIAG_Unimplemented); }
  virtual bool GotSrcInfo() const
    { DIAG_Die(DIAG_Unimplemented); } 
  virtual void SetSrcInfoDone(bool bi) 
    { DIAG_Die(DIAG_Unimplemented); }

  uint  structureId() const { return m_sId; }
  uint& structureId()       { return m_sId; }
  
  // Dump contents for inspection
  virtual std::string ToDumpString(int dmpFlag = CCT::Tree::XML_TRUE) const;
    
protected: 
  void Relocate();
  SrcFile::ln begLine;
  SrcFile::ln endLine;
  uint m_sId;  // static structure id
  static std::string BOGUS;
}; 

// - if x < y; 0 if x == y; + otherwise
int CSProfCodeNodeLineComp(CSProfCodeNode* x, CSProfCodeNode* y);


//***************************************************************************
// IDynNode
//***************************************************************************

// IDynNode: a mixin interface representing dynamic nodes

class IDynNode {
public:

  // -------------------------------------------------------
  // 
  // -------------------------------------------------------
  
  IDynNode(CSProfCodeNode* proxy, 
	   u_int32_t cpid,
	   const SampledMetricDescVec* metricdesc)
    : m_proxy(proxy),
      m_as_info(lush_assoc_info_NULL), 
      m_lmId(Epoch::LM_id_NULL), m_ip(0), m_opIdx(0), m_lip(NULL), m_cpid(cpid),
      m_metricdesc(metricdesc)
    { }

  IDynNode(CSProfCodeNode* proxy, 
	   lush_assoc_info_t as_info, VMA ip, ushort opIdx, lush_lip_t* lip,
	   u_int32_t cpid,
	   const SampledMetricDescVec* metricdesc)
    : m_proxy(proxy), 
      m_as_info(as_info), 
      m_lmId(Epoch::LM_id_NULL), m_ip(ip), m_opIdx(opIdx), m_lip(lip), m_cpid(cpid),
      m_metricdesc(metricdesc)
    { }

  IDynNode(CSProfCodeNode* proxy, 
	   lush_assoc_info_t as_info, VMA ip, ushort opIdx, lush_lip_t* lip,
	   u_int32_t cpid,
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
  CSProfCodeNode* proxy() const { return m_proxy; }

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

  u_int32_t cpid() const 
    { return m_cpid; }

  hpcfile_metric_data_t metric(int i) const 
    { return m_metrics[i]; }
  uint numMetrics() const 
    { return m_metrics.size(); }

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
  

  // -------------------------------------------------------
  // Dump contents for inspection
  // -------------------------------------------------------

  virtual std::string toString(const char* pre = "") const;

  std::string assocInfo_str() const;
  std::string lip_str() const;

  // writeMetrics_xml: write metrics (sparsely)
  virtual void writeMetrics_xml(std::ostream& os, 
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
  CSProfCodeNode* m_proxy;

  lush_assoc_info_t m_as_info;

  Epoch::LM_id_t m_lmId; // Epoch::LM id
  VMA m_ip;       // instruction pointer for this node
  ushort m_opIdx; // index in the instruction [OBSOLETE]

  lush_lip_t* m_lip; // lush logical ip

  u_int32_t m_cpid; // call path node handle for tracing

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
// CSProfPgmNode, CSProfCallSiteNode, CSProfGroupNodes,
// CSProfLoopNode, CSProfStmtRangeNode
//***************************************************************************

// ---------------------------------------------------------
// CSProfPgmNode is root of the call stack tree
// ---------------------------------------------------------
class CSProfPgmNode: public CSProfNode {
public: 
  // Constructor/Destructor
  CSProfPgmNode(const char* nm);
  virtual ~CSProfPgmNode();

  const std::string& GetName() const { return name; }
                                        
  void Freeze() { frozen = true;} // disallow additions to/deletions from tree
  bool IsFrozen() const { return frozen; }
  
  // Dump contents for inspection
  virtual std::string ToDumpString(int dmpFlag = CCT::Tree::XML_TRUE) const;
  
protected: 
private: 
  bool frozen;
  std::string name; // the program name
}; 

// ---------------------------------------------------------
// CSProfGroupNodes: Describe several different types of scopes
//   (including user-defined ones)
//
// children of: PgmScope, CSProfGroupNode, CSProfCallSiteNode, CSProfLoopNode.
// children: CSProfGroupNode, CSProfCallSiteNode, CSProfLoopNode, 
//   CSProfStmtRangeNode
// ---------------------------------------------------------
class CSProfGroupNode: public CSProfNode {
public: 
  // Constructor/Destructor
  CSProfGroupNode(CSProfNode* _parent, const char* nm);
  ~CSProfGroupNode();
  
  const std::string& GetName() const { return name; }
  
  // Dump contents for inspection
  virtual std::string ToDumpString(int dmpFlag = CCT::Tree::XML_TRUE) const;

private: 
  std::string name; 
};

// ---------------------------------------------------------
// CSProfCallSiteNode is the back-bone of the call stack tree
// children of: CSProfPgmNode, CSProfGroupNode, CSProfCallSiteNode
// children: CSProfGroupNode, CSProfCallSiteNode, CSProfLoopNode,
//   CSProfStmtRangeNode
// ---------------------------------------------------------

class CSProfCallSiteNode: public CSProfCodeNode, public IDynNode {
public:
  // Constructor/Destructor
  CSProfCallSiteNode(CSProfNode* _parent,
		     u_int32_t cpid,
		     const SampledMetricDescVec* metricdesc);
  CSProfCallSiteNode(CSProfNode* _parent, 
		     lush_assoc_info_t as_info,
		     VMA ip, ushort opIdx, 
		     lush_lip_t* lip,
		     u_int32_t cpid,
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
  
  const std::string& GetFile() const { return file; }
  const std::string& GetProc() const { return proc; }
  SrcFile::ln        GetLine() const { return begLine; }


  void SetFile(const char* fnm) { file = fnm; }
  void SetFile(const std::string& fnm) { file = fnm; }

  void SetProc(const char* pnm) { proc = pnm; }
  void SetProc(const std::string& pnm) { proc = pnm; }

  void SetLine(SrcFile::ln ln) { begLine = endLine = ln; /* SetLineRange(ln, ln); */ } 
  bool FileIsText() const {return fileistext;} 
  void SetFileIsText(bool bi) {fileistext = bi;}
  bool GotSrcInfo() const {return donewithsrcinfproc;} 
  void SetSrcInfoDone(bool bi) {donewithsrcinfproc=bi;}
  
  // Dump contents for inspection
  virtual std::string ToDumpString(int dmpFlag = CCT::Tree::XML_TRUE) const;
 
protected: 
  // source file info
  std::string file; 
  bool   fileistext; //separated from load module 
  bool   donewithsrcinfproc ;
  std::string proc;
};

// ---------------------------------------------------------
// CSProfStatementNode correspond to leaf nodes in the call stack tree 
// children of: CSProfPgmNode, CSProfGroupNode, CSProfCallSiteNode
// ---------------------------------------------------------
  
class CSProfStatementNode: public CSProfCodeNode, public IDynNode {
 public:
  // Constructor/Destructor
  CSProfStatementNode(CSProfNode* _parent, 
		      u_int32_t cpid,
		      const SampledMetricDescVec* metricdesc);
  virtual ~CSProfStatementNode();

  void operator=(const CSProfStatementNode& x);
  void operator=(const CSProfCallSiteNode& x);

  // Node data
  const std::string& GetFile() const { return file; }
  const std::string& GetProc() const { return proc; }
  SrcFile::ln        GetLine() const { return begLine; }


  void SetFile(const char* fnm) { file = fnm; }
  void SetFile(const std::string& fnm) { file = fnm; }

  void SetProc(const char* pnm) { proc = pnm; }
  void SetProc(const std::string& pnm) { proc = pnm; }

  void SetLine(SrcFile::ln ln) { begLine = endLine = ln; /* SetLineRange(ln, ln); */ } 
  bool FileIsText() const { return fileistext; }
  void SetFileIsText(bool bi) { fileistext = bi; }
  bool GotSrcInfo() const { return donewithsrcinfproc; }
  void SetSrcInfoDone(bool bi) { donewithsrcinfproc = bi; }
  
  // Dump contents for inspection
  virtual std::string ToDumpString(int dmpFlag = CCT::Tree::XML_TRUE) const;

protected: 

  // source file info
  std::string file; 
  bool   fileistext; //separated from load module
  bool   donewithsrcinfproc;
  std::string proc;
};

// ---------------------------------------------------------
// CSProfProcedureFrameNode is  
// children of: CSProfPgmNode, CSProfGroupNode, CSProfCallSiteNode
// children: CSProfCallSiteNode, CSProfStatementNode
// ---------------------------------------------------------
  
class CSProfProcedureFrameNode: public CSProfCodeNode {
public:
  // Constructor/Destructor
  CSProfProcedureFrameNode(CSProfNode* _parent);
  virtual ~CSProfProcedureFrameNode();
  
  // Node data
  const std::string& GetFile() const { return file; }
  void SetFile(const char* fnm) { file = fnm; }
  void SetFile(const std::string& fnm) { file = fnm; }

  const std::string& GetProc() const { return proc; }
  void SetProc(const char* pnm) { proc = pnm; }
  void SetProc(const std::string& pnm) { proc = pnm; }

  SrcFile::ln GetLine() const { return begLine; }
  void SetLine(SrcFile::ln ln) { begLine = endLine = ln; /* SetLineRange(ln, ln); */ } 
  bool FileIsText() const {return fileistext;}
  void SetFileIsText(bool bi) {fileistext = bi;}

  // Alien
  bool  isAlien() const { return m_alien; }
  bool& isAlien()       { return m_alien; } 

  // Dump contents for inspection
  virtual std::string ToDumpString(int dmpFlag = CCT::Tree::XML_TRUE) const;
 
private: 
  // source file info
  std::string file;
  bool   fileistext; //separated from load module
  std::string proc;
  bool m_alien;
};

// ---------------------------------------------------------
// CSProfLoopNode:
// children of: CSProfGroupNode, CSProfCallSiteNode or CSProfLoopNode
// children: CSProfGroupNode, CSProfLoopNode or CSProfStmtRangeNode
// ---------------------------------------------------------
class CSProfLoopNode: public CSProfCodeNode {
public: 
  // Constructor/Destructor
  CSProfLoopNode(CSProfNode* _parent, SrcFile::ln begLn, SrcFile::ln endLn,
		 uint sId = 0);
  ~CSProfLoopNode();
  
  // Dump contents for inspection
  virtual std::string CodeName() const;
  virtual std::string ToDumpString(int dmpFlag = CCT::Tree::XML_TRUE) const; 
  
private:
};

// ---------------------------------------------------------
// CSProfStmtRangeNode:
// children of: CSProfGroupNode, CSProfCallSiteNode, or CSProfLoopNode
// children: none
// ---------------------------------------------------------
class CSProfStmtRangeNode: public CSProfCodeNode {
public: 
  // Constructor/Destructor
  CSProfStmtRangeNode(CSProfNode* _parent, 
		      SrcFile::ln begLn, SrcFile::ln endLn, 
		      uint sId = 0);
  ~CSProfStmtRangeNode();
  
  // Dump contents for inspection
  virtual std::string CodeName() const;
  virtual std::string ToDumpString(int dmpFlag = CCT::Tree::XML_TRUE) const;
};

#ifndef xDEBUG
#define xDEBUG(flag, code) {if (flag) {code; fflush(stdout); fflush(stderr);}} 
#endif

#define DEB_READ_MMETRICS 0
#define DEB_UNIFY_PROCEDURE_FRAME 0


} // namespace Prof

//***************************************************************************

#include "CCT-TreeIterator.hpp"

//***************************************************************************


#endif /* prof_juicy_Prof_CCT_Tree_hpp */
