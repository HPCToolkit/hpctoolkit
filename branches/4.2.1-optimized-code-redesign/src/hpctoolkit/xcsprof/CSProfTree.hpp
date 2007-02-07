// -*-Mode: C++;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002, Rice University 
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

#ifndef CSProfTree_hpp 
#define CSProfTree_hpp

//************************* System Include Files ****************************

#include <iostream>
#include <vector>
#include <string>

//*************************** User Include Files ****************************

#include <include/general.h>

// FIXME: CSProfTree should be merged or at least relocated a la PgmScopeTree
#include <lib/prof-juicy/PgmScopeTree.hpp>

#include <lib/isa/ISATypes.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/NonUniformDegreeTree.hpp>
#include <lib/support/Unique.hpp>

//*************************** Forward Declarations ***************************

using namespace std;
int AddXMLEscapeChars(int dmpFlag);

//***************************************************************************
// CSProfTree
//***************************************************************************

class CSProfNode;

class CSProfTree: public Unique {
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
  CSProfTree();
  virtual ~CSProfTree();

  // Tree data
  CSProfNode* GetRoot() const { return root; }
  bool        IsEmpty() const { return (root == NULL); }

  void SetRoot(CSProfNode* x) { root = x; } 

  // Dump contents for inspection
  virtual void Dump(std::ostream& os = std::cerr, 
		    int dmpFlag = XML_TRUE) const;
  virtual void DDump() const;
 
private:
  CSProfNode* root;
};

//***************************************************************************
// CSProfNode, CSProfCodeNode.
//***************************************************************************

class CSProfNode;     // Everyone's base class 
class CSProfCodeNode; // Base class for describing source code

class CSProfPgmNode; 
class CSProfGroupNode; 
class CSProfCallSiteNode;
class CSProfLoopNode; 
class CSProfStmtRangeNode; 
class CSProfMetricsNode; 

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
  
  static const string& NodeTypeToName(NodeType tp);
  static NodeType    IntToNodeType(long i);

private:
  static const string NodeNames[NUMBER_OF_TYPES];
  
public:
  CSProfNode(NodeType t, CSProfNode* _parent);
  virtual ~CSProfNode();
  
  // --------------------------------------------------------
  // General Interface to fields 
  // --------------------------------------------------------
  NodeType     GetType() const     { return type; }
  unsigned int GetUniqueId() const { return uid; }

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
  CSProfNode*          Ancestor(NodeType tp) const;
  
  CSProfPgmNode*       AncestorPgm() const;
  CSProfGroupNode*     AncestorGroup() const;
  CSProfCallSiteNode*  AncestorCallSite() const;
  CSProfLoopNode*      AncestorLoop() const;
  CSProfStmtRangeNode* AncestorStmtRange() const;
  CSProfProcedureFrameNode* AncestorProcedureFrame() const; // CC
  CSProfStatementNode* AncestorStatement() const; // CC

  // --------------------------------------------------------
  // Dump contents for inspection
  // --------------------------------------------------------
  virtual std::string ToDumpString(int dmpFlag = CSProfTree::XML_TRUE) const; 
  virtual std::string ToDumpMetricsString(int dmpFlag) const; 

  virtual std::string Types(); // lists this instance's base and derived types 
  
  void DumpSelfBefore(std::ostream& os = std::cerr, 
		      int dmpFlag = CSProfTree::XML_TRUE,
		      const char *prefix = "") const;
  void DumpSelfAfter (std::ostream& os = std::cerr, 
		      int dmpFlag = CSProfTree::XML_TRUE,
		      const char *prefix = "") const;
  void Dump          (std::ostream& os = std::cerr, 
		      int dmpFlag = CSProfTree::XML_TRUE,
		      const char *pre = "") const;
  void DumpLineSorted(std::ostream& os = std::cerr, 
		      int dmpFlag = CSProfTree::XML_TRUE,
		      const char *pre = "") const;
  
  void DDump();
  void DDumpSort();
  
 protected:
  // private: 
  NodeType type;
  unsigned int uid; 
};

// ---------------------------------------------------------
// CSProfCodeNode is a base class for all CSProfNode's that describe
// elements of source files (code, e.g., loops, statements).
// ---------------------------------------------------------
class CSProfCodeNode : public CSProfNode {
protected: 
  CSProfCodeNode(NodeType t, CSProfNode* _parent, 
		 suint begLn = UNDEF_LINE, suint endLn = UNDEF_LINE);
  
public: 
  virtual ~CSProfCodeNode();
  
  // Node data
  suint GetBegLine() const { return begLine; }; // in source code
  suint GetEndLine() const { return endLine; }; // in source code
  
  bool            ContainsLine(suint ln) const; 
  CSProfCodeNode* CSProfCodeNodeWithLine(suint ln) const; 

  void SetLineRange(suint begLn, suint endLn); // be careful when using!

  // eraxxon: I made this stuff virtual in order to handle both
  // CSProfCallSiteNode/CSProfProcedureFrameNode or
  // CSProfStatementNode in the same way.  FIXME: The abstractions
  // need to be fixed! Classes have been duplicated with impunity.
  virtual const std::string& GetFile() const
    { DIAG_Die(DIAG_Unimplemented); return BOGUS; }
  virtual void SetFile(const char* fnm) 
    { DIAG_Die(DIAG_Unimplemented); }

  virtual const std::string& GetProc() const 
    { DIAG_Die(DIAG_Unimplemented); return BOGUS; }
  virtual void SetProc(const char* pnm) 
    { DIAG_Die(DIAG_Unimplemented); }

  virtual void SetLine(suint ln) 
    { DIAG_Die(DIAG_Unimplemented); } 
 
  virtual VMA GetIP() const 
    { DIAG_Die(DIAG_Unimplemented); return 0; }
  virtual ushort GetOpIndex() const 
    { DIAG_Die(DIAG_Unimplemented); return 0; }

  virtual void SetIP(VMA _ip, ushort _opIndex) 
    { DIAG_Die(DIAG_Unimplemented); }

  virtual bool FileIsText() const
    { DIAG_Die(DIAG_Unimplemented); return false; }
  virtual void SetFileIsText(bool bi) 
    { DIAG_Die(DIAG_Unimplemented); }
  virtual bool GotSrcInfo() const
    { DIAG_Die(DIAG_Unimplemented); } 
  virtual void SetSrcInfoDone(bool bi) 
    { DIAG_Die(DIAG_Unimplemented); }
  
  // Dump contents for inspection
  virtual std::string ToDumpString(int dmpFlag = CSProfTree::XML_TRUE) const;
    
protected: 
  void Relocate();
  suint begLine;
  suint endLine;
  static string BOGUS;
}; 

// - if x < y; 0 if x == y; + otherwise
int CSProfCodeNodeLineComp(CSProfCodeNode* x, CSProfCodeNode* y);

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
  virtual std::string ToDumpString(int dmpFlag = CSProfTree::XML_TRUE) const;
  
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
  virtual std::string ToDumpString(int dmpFlag = CSProfTree::XML_TRUE) const;

private: 
  std::string name; 
};

// ---------------------------------------------------------
// CSProfCallSiteNode is the back-bone of the call stack tree
// children of: CSProfPgmNode, CSProfGroupNode, CSProfCallSiteNode
// children: CSProfGroupNode, CSProfCallSiteNode, CSProfLoopNode,
//   CSProfStmtRangeNode
// ---------------------------------------------------------
  
class CSProfCallSiteNode: public CSProfCodeNode {
public:
  // Constructor/Destructor
  CSProfCallSiteNode(CSProfNode* _parent);
  CSProfCallSiteNode(CSProfNode* _parent, 
		     VMA _ip, ushort _opIndex, vector<suint> _metrics);
  virtual ~CSProfCallSiteNode();
  
  // Node data
  VMA GetIP() const { return ip-1; }
  ushort GetOpIndex() const { return opIndex; }
  
  const std::string& GetFile() const { return file; }
  const std::string& GetProc() const { return proc; }
  suint              GetLine() const { return begLine; }

  void SetIP(VMA _ip, ushort _opIndex) { ip = _ip; opIndex = _opIndex; }

  void SetFile(const char* fnm) { file = fnm; }
  void SetFile(const std::string& fnm) { file = fnm; }

  void SetProc(const char* pnm) { proc = pnm; }
  void SetProc(const std::string& pnm) { proc = pnm; }

  void SetLine(suint ln) { begLine = endLine = ln; /* SetLineRange(ln, ln); */ } 
  bool FileIsText() const {return fileistext;} 
  void SetFileIsText(bool bi) {fileistext = bi;}
  bool GotSrcInfo() const {return donewithsrcinfproc;} 
  void SetSrcInfoDone(bool bi) {donewithsrcinfproc=bi;}

  suint GetMetric(int metricIndex) {return metrics[metricIndex];}
  vector<suint>& GetMetrics() {return metrics;}
  
  // Dump contents for inspection
  virtual std::string ToDumpString(int dmpFlag = CSProfTree::XML_TRUE) const;
  virtual std::string ToDumpMetricsString(int dmpFlag = CSProfTree::XML_TRUE) const;
 
  /// add metrics from call site node c to current node.
  void addMetrics(CSProfCallSiteNode* c);
protected: 
  VMA ip;        // instruction pointer for this node
  ushort opIndex; // index in the instruction 

  vector<suint> metrics;  

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
  
class CSProfStatementNode: public CSProfCodeNode {
 public:
  // Constructor/Destructor
  CSProfStatementNode(CSProfNode* _parent);
  virtual ~CSProfStatementNode();

  void operator=(const CSProfStatementNode& x);
  void copyCallSiteNode(CSProfCallSiteNode* _node); // FIXME: remove

  // Node data
  VMA GetIP() const { return ip; }
  ushort GetOpIndex() const { return opIndex; }
  
  const std::string& GetFile() const { return file; }
  const std::string& GetProc() const { return proc; }
  suint              GetLine() const { return begLine; }

  void SetIP(VMA _ip, ushort _opIndex) { ip = _ip; opIndex = _opIndex; }

  void SetFile(const char* fnm) { file = fnm; }
  void SetFile(const std::string& fnm) { file = fnm; }

  void SetProc(const char* pnm) { proc = pnm; }
  void SetProc(const std::string& pnm) { proc = pnm; }

  void SetLine(suint ln) { begLine = endLine = ln; /* SetLineRange(ln, ln); */ } 
  bool FileIsText() const {return fileistext;}
  void SetFileIsText(bool bi) {fileistext = bi;}
  bool GotSrcInfo() const {return donewithsrcinfproc;} 
  void SetSrcInfoDone(bool bi) {donewithsrcinfproc=bi;}

  suint GetMetric(int metricIndex) const {return metrics[metricIndex];}
  suint GetMetricCount() const {return metrics.size();}
  
  // Dump contents for inspection
  virtual std::string ToDumpString(int dmpFlag = CSProfTree::XML_TRUE) const;
  virtual std::string ToDumpMetricsString(int dmpFlag = CSProfTree::XML_TRUE) const;

  /// add metrics from call site node c to current node.
  void addMetrics(const CSProfStatementNode* c);
 
protected: 
  VMA ip;        // instruction pointer for this node
  ushort opIndex; // index in the instruction 

  vector<suint> metrics;  

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

  suint GetLine() const { return begLine; }
  void SetLine(suint ln) { begLine = endLine = ln; /* SetLineRange(ln, ln); */ } 
  bool FileIsText() const {return fileistext;}
  void SetFileIsText(bool bi) {fileistext = bi;}

  // Dump contents for inspection
  virtual std::string ToDumpString(int dmpFlag = CSProfTree::XML_TRUE) const;
 
private: 
  // source file info
  std::string file; 
  bool   fileistext; //separated from load module
  std::string proc;
};

// ---------------------------------------------------------
// CSProfLoopNode:
// children of: CSProfGroupNode, CSProfCallSiteNode or CSProfLoopNode
// children: CSProfGroupNode, CSProfLoopNode or CSProfStmtRangeNode
// ---------------------------------------------------------
class CSProfLoopNode: public CSProfCodeNode {
public: 
  // Constructor/Destructor
  CSProfLoopNode(CSProfNode* _parent, suint begLn, suint endLn,
		 int _id = -1);
  ~CSProfLoopNode();
  
  // Dump contents for inspection
  virtual std::string CodeName() const;
  virtual std::string ToDumpString(int dmpFlag = CSProfTree::XML_TRUE) const; 
  
private:
  int id;
};

// ---------------------------------------------------------
// CSProfStmtRangeNode:
// children of: CSProfGroupNode, CSProfCallSiteNode, or CSProfLoopNode
// children: none
// ---------------------------------------------------------
class CSProfStmtRangeNode: public CSProfCodeNode {
public: 
  // Constructor/Destructor
  CSProfStmtRangeNode(CSProfNode* _parent, suint begLn, suint endLn, 
		      int _id = -1);
  ~CSProfStmtRangeNode();
  
  // Dump contents for inspection
  virtual std::string CodeName() const;
  virtual std::string ToDumpString(int dmpFlag = CSProfTree::XML_TRUE) const;
  
private:
  int id;
};

#ifndef xDEBUG
#define xDEBUG(flag, code) {if (flag) {code; fflush(stdout); fflush(stderr);}} 
#endif

#define DEB_READ_MMETRICS 0
#define DEB_UNIFY_PROCEDURE_FRAME 0

#include "CSProfTreeIterator.hpp"

#endif
