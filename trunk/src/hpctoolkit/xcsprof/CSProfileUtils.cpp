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
//    CSProfileUtils.C
//
// Purpose:
//    [The purpose of this file]
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <iostream>
#include <fstream>

#ifdef NO_STD_CHEADERS  // FIXME
# include <string.h>
#else
# include <cstring>
using namespace std; // For compatibility with non-std C headers
#endif

//*************************** User Include Files ****************************

#include "CSProfileUtils.h"
#include <lib/binutils/LoadModule.h>
#include <lib/binutils/PCToSrcLineMap.h>
#include <lib/binutils/LoadModuleInfo.h>
#include <lib/xml/xml.h>
#include <lib/support/String.h>
#include <lib/support/Assertion.h>

#include <lib/hpcfile/hpcfile_csproflib.h>

//*************************** Forward Declarations ***************************

using namespace xml;

extern "C" {
  static void* cstree_create_node_CB(void* tree, 
				     hpcfile_cstree_nodedata_t* data);
  static void  cstree_link_parent_CB(void* tree, void* node, void* parent);

  static void* hpcfile_alloc_CB(size_t sz);
  static void  hpcfile_free_CB(void* mem);
}


bool AddPGMToCSProfTree(CSProfTree* tree, const char* progName);

void ConvertOpIPToIP(Addr opIP, Addr& ip, ushort& opIdx);

//****************************************************************************
// Dump a CSProfTree and collect source file information
//****************************************************************************

const char *CSPROFILEdtd =
#include <lib/xml/CSPROFILE.dtd.h>

void
WriteCSProfile(CSProfile* prof, std::ostream& os, bool prettyPrint)
{
  os << "<?xml version=\"1.0\"?>" << std::endl;
  os << "<!DOCTYPE CSPROFILE [\n" << CSPROFILEdtd << "]>" << std::endl;
  os.flush();

  CSProfileMetric* metric = prof->GetMetric();
  os << "<CSPROFILE version=\"1.0\">\n";
  os << "<CSPROFILEHDR>\n</CSPROFILEHDR>\n";
  os << "<CSPROFILEPARAMS>\n";
  {
    os << "<TARGET name"; WriteAttrStr(os, prof->GetTarget()); os << "/>\n";
    
    os << "<METRIC shortName"; WriteAttrNum(os, 0);
    os << " nativeName";       WriteAttrNum(os, metric->GetName());
    os << " period";           WriteAttrNum(os, metric->GetPeriod());
    os << "/>\n";

    os << "</CSPROFILEPARAMS>\n";
  }
  os.flush();
  
  int dumpFlags = (CSProfTree::XML_TRUE); // CSProfTree::XML_NO_ESC_CHARS
  if (!prettyPrint) { dumpFlags |= CSProfTree::COMPRESSED_OUTPUT; }
  
  prof->GetTree()->Dump(os, dumpFlags);
}

bool 
AddSourceFileInfoToCSProfile(CSProfile* prof, LoadModuleInfo* lm)
{
  bool noError = true;

  CSProfTree* tree = prof->GetTree();
  CSProfNode* root = tree->GetRoot();
  
  for (CSProfNodeIterator it(root); it.CurNode(); ++it) {
    CSProfNode* n = it.CurNode();
    AddSourceFileInfoToCSTreeNode(n, lm);
  }

  return noError;
}

bool 
AddSourceFileInfoToCSTreeNode(CSProfNode* node, LoadModuleInfo* lm)
{
  bool noError = true;

  CSProfCallSiteNode* n = dynamic_cast<CSProfCallSiteNode*>(node);
  if (n) {
    String func, file;
    SrcLineX srcLn; 
    lm->GetSymbolicInfo(n->GetIP(), n->GetOpIndex(), func, file, srcLn);
    
    n->SetFile(file);
    n->SetProc(func);
    n->SetLine(srcLn.GetSrcLine());
  }
  
  return noError;
}

//****************************************************************************
// Set of routines to build a scope tree while reading data file
//****************************************************************************

CSProfile* 
ReadCSProfileFile_HCSPROFILE(const char* fnm) 
{
  hpcfile_csprof_data_t data;
  int ret1, ret2;

  CSProfile* prof = new CSProfile;
  
  // Read profile
  FILE* fs = hpcfile_open_for_read(fnm);
  ret1 = hpcfile_csprof_read(fs, &data, hpcfile_alloc_CB, hpcfile_free_CB);
  ret2 = hpcfile_cstree_read(fs, prof->GetTree(), 
			     cstree_create_node_CB, cstree_link_parent_CB,
			     hpcfile_alloc_CB, hpcfile_free_CB);
  hpcfile_close(fs);

  if ( !(ret1 == HPCFILE_OK && ret2 == HPCFILE_OK) ) { 
    delete prof;
    return NULL;
  }

  // Extract profiling info
  prof->SetTarget(data.target);
  CSProfileMetric* metric = prof->GetMetric();
  metric->SetName(data.event);
  metric->SetPeriod(data.sample_period);

  // We must deallocate pointer-data
  hpcfile_free_CB(data.target);
  hpcfile_free_CB(data.event);

  // Add PGM node to tree
  AddPGMToCSProfTree(prof->GetTree(), prof->GetTarget());
  
  return prof;
}

static void* 
cstree_create_node_CB(void* tree, hpcfile_cstree_nodedata_t* data)
{
  CSProfTree* t = (CSProfTree*)tree; 
  
  Addr ip;
  ushort opIdx;
  ConvertOpIPToIP((Addr)data->ip, ip, opIdx);

  CSProfCallSiteNode* n = 
    new CSProfCallSiteNode(NULL, ip, opIdx, (suint)data->weight);
  
  // Initialize the tree, if necessary
  if (t->IsEmpty()) {
    t->SetRoot(n);
  }
  
  return n;
}

static void  
cstree_link_parent_CB(void* tree, void* node, void* parent)
{
  CSProfCallSiteNode* p = (CSProfCallSiteNode*)parent;
  CSProfCallSiteNode* n = (CSProfCallSiteNode*)node;
  n->Link(p);
}

static void* 
hpcfile_alloc_CB(size_t sz)
{
  return (new char[sz]);
}

static void  
hpcfile_free_CB(void* mem)
{
  delete[] (char*)mem;
}

// ConvertOpIPToIP: Find the instruction pointer 'ip' and operation
// index 'opIdx' from the operation pointer 'opIP'.  The operation
// pointer is represented by adding 0, 1, or 2 to the instruction
// pointer for the first, second and third operation, respectively.
void 
ConvertOpIPToIP(Addr opIP, Addr& ip, ushort& opIdx)
{
  opIdx = (ushort)(opIP & 0x3); // the mask ...00011 covers 0, 1 and 2
  ip = opIP - opIdx;
}

bool
AddPGMToCSProfTree(CSProfTree* tree, const char* progName)
{
  bool noError = true;

  // Add PGM node
  CSProfNode* n = tree->GetRoot();
  if (!n || n->GetType() != CSProfNode::PGM) {
    CSProfNode* root = new CSProfPgmNode(progName);
    if (n) { 
      n->Link(root); // 'root' is parent of 'n'
    }
    tree->SetRoot(root);
  }
  
  return noError;
}


//***************************************************************************
// Routines for normalizing the ScopeTree
//***************************************************************************

bool CoalesceCallsiteLeaves(CSProfile* prof);

bool 
NormalizeCSProfile(CSProfile* prof)
{
  // Remove duplicate/inplied file and procedure information from tree
  bool pass1 = true;

  bool pass2 = CoalesceCallsiteLeaves(prof);
  
  return (pass1 && pass2);
}


// FIXME
// If pc values from the leaves map to the same source file info,
// coalese these leaves into one.
bool CoalesceCallsiteLeaves(CSProfNode* node);

bool 
CoalesceCallsiteLeaves(CSProfile* prof)
{
  CSProfTree* csproftree = prof->GetTree();
  if (!csproftree) { return true; }
  
  return CoalesceCallsiteLeaves(csproftree->GetRoot());
}


// FIXME
typedef std::map<String, CSProfCallSiteNode*, StringLt> StringToCallSiteMap;
typedef std::map<String, CSProfCallSiteNode*, StringLt>::iterator 
  StringToCallSiteMapIt;
typedef std::map<String, CSProfCallSiteNode*, StringLt>::value_type
  StringToCallSiteMapVal;

bool 
CoalesceCallsiteLeaves(CSProfNode* node)
{
  bool noError = true;
  
  if (!node) { return noError; }

  // FIXME: Use this set to determine if we have a duplicate source line
  StringToCallSiteMap sourceInfoMap;
  
  // For each immediate child of this node...
  for (CSProfNodeChildIterator it(node); it.Current(); /* */) {
    CSProfCodeNode* child = dynamic_cast<CSProfCodeNode*>(it.CurNode());
    BriefAssertion(child); // always true (FIXME)
    it++; // advance iterator -- it is pointing at 'child'
    
    bool inspect = (child->IsLeaf() 
		    && (child->GetType() == CSProfNode::CALLSITE));
    
    if (inspect) {

      // This child is a leaf. Test for duplicate source line info.
      CSProfCallSiteNode* c = dynamic_cast<CSProfCallSiteNode*>(child);
      String myid = String(c->GetFile()) + String(c->GetProc()) 
	+ String(c->GetLine());
      
      StringToCallSiteMapIt it = sourceInfoMap.find(myid);
      if (it != sourceInfoMap.end()) { 
	// found -- we have a duplicate
	CSProfCallSiteNode* c1 = (*it).second;
	
	// add weights of 'c' and 'c1' 
	suint newWeight = c->GetWeight() + c1->GetWeight();
	c1->SetWeight(newWeight);
	
	// remove 'child' from tree
	child->Unlink();
	delete child;
      } else { 
	// no entry found -- add
	sourceInfoMap.insert(StringToCallSiteMapVal(myid, c));
      }

      
    } else if (!child->IsLeaf()) {
      // Recur:
      noError = noError && CoalesceCallsiteLeaves(child);
    }
    
  } 
  
  return noError;
}
