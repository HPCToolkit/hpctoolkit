// $Id$
// -*-C++-*-
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
//    CSProfTree.H
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef CSProfTree_H 
#define CSProfTree_H

//************************* System Include Files ****************************

#include <iostream>

//*************************** User Include Files ****************************

#include <include/general.h>

#include <lib/support/NonUniformDegreeTree.h>
#include <lib/support/Unique.h>
#include <lib/support/String.h>

#include <lib/ISA/ISATypes.h>

//*************************** Forward Declarations ***************************

int AddXMLEscapeChars(int dmpFlag);

const suint UNDEF_LINE = 0;

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
    ANY,
    NUMBER_OF_TYPES
  };
  
  static const char* NodeTypeToName(NodeType tp); 
  static NodeType    IntToNodeType(long i);

private:
  static const char* NodeNames[NUMBER_OF_TYPES];
  
public:
  CSProfNode(NodeType t, CSProfNode* _parent);
  virtual ~CSProfNode();
  
  // --------------------------------------------------------
  // General Interface to fields 
  // --------------------------------------------------------
  NodeType     GetType() const     { return type; }
  unsigned int GetUniqueId() const { return uid; }

  // 'GetName()' is overridden by some derived classes
  virtual String GetName() const { return NodeTypeToName(GetType()); }
  
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

  // --------------------------------------------------------
  // Dump contents for inspection
  // --------------------------------------------------------
  virtual String ToDumpString(int dmpFlag = CSProfTree::XML_TRUE) const; 
  virtual String Types(); // lists this instance's base and derived types 
  
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
  
private: 
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
  
  // Dump contents for inspection
  virtual String ToDumpString(int dmpFlag = CSProfTree::XML_TRUE) const;
    
protected: 
  void Relocate();
  suint begLine;
  suint endLine;
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

  String GetName() const { return name; }
                                        
  void Freeze() { frozen = true;} // disallow additions to/deletions from tree
  bool IsFrozen() const { return frozen; }
  
  // Dump contents for inspection
  virtual String ToDumpString(int dmpFlag = CSProfTree::XML_TRUE) const;
  
protected: 
private: 
  bool frozen;
  String name; // the program name
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
  
  String GetName() const { return name; }
  
  // Dump contents for inspection
  virtual String ToDumpString(int dmpFlag = CSProfTree::XML_TRUE) const;

private: 
  String name; 
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
		     Addr _ip, ushort _opIndex, suint _weight);
  virtual ~CSProfCallSiteNode();
  
  // Node data
  suint GetWeight() const { return weight; }
  Addr GetIP() const { return ip; }
  ushort GetOpIndex() const { return opIndex; }
  
  const char* GetFile() const { return file; }
  const char* GetProc() const { return proc; }
  suint       GetLine() const { return begLine; }

  void SetIP(Addr _ip, ushort _opIndex) { ip = _ip; opIndex = _opIndex; }
  void SetWeight(suint w) { weight = w; }

  void SetFile(const char* fnm) { file = fnm; }
  void SetProc(const char* pnm) { proc = pnm; }
  void SetLine(suint ln) { begLine = endLine = ln; /* SetLineRange(ln, ln); */ }
  
  // Dump contents for inspection
  virtual String ToDumpString(int dmpFlag = CSProfTree::XML_TRUE) const;
  
private: 
  Addr ip;        // instruction pointer for this node
  ushort opIndex; // index in the instruction 

  // 'weight': if non-zero, indicates the end of a sample from this
  // node to the tree's root.  The value indicates the number of times
  // the sample has been recorded.
  suint weight;

  // source file info
  String file;
  String proc;
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
  virtual String CodeName() const;
  virtual String ToDumpString(int dmpFlag = CSProfTree::XML_TRUE) const; 
  
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
  virtual String CodeName() const;
  virtual String ToDumpString(int dmpFlag = CSProfTree::XML_TRUE) const;
  
private:
  int id;
};

#include "CSProfTreeIterator.h" 

#endif
