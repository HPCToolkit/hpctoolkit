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

//************************* System Include Files ****************************

#include <iostream>
#include <fstream>
#include <stack>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/errno.h>

#include <string>
using std::string;

#ifdef NO_STD_CHEADERS  // FIXME
# include <string.h>
#else
# include <cstring>
using namespace std; // For compatibility with non-std C headers
#endif

//*************************** User Include Files ****************************

#include "CSProfileUtils.hpp"

#include <lib/prof-lean/hpcfile_csproflib.h>

#include <lib/binutils/LM.hpp>

#include <lib/xml/xml.hpp>

#include <lib/support/Assertion.h>

#if 0
#define XCSPROF_DEBUG 1   
#define DEB_ON_LINUX 0
#endif

//*************************** Forward Declarations ***************************

using namespace xml;

extern "C" {
  static void* cstree_create_node_CB(void* tree, 
				     hpcfile_cstree_nodedata_t* data,
				     int num_metrics);
  static void  cstree_link_parent_CB(void* tree, void* node, void* parent);

  static void* hpcfile_alloc_CB(size_t sz);
  static void  hpcfile_free_CB(void* mem);
}


bool AddPGMToCSProfTree(CSProfTree* tree, const char* progName);

void ConvertOpIPToIP(VMA opIP, VMA& ip, ushort& opIdx);

//****************************************************************************
// Dump a CSProfTree and collect source file information
//****************************************************************************

const char *CSPROFILEdtd =
#include <lib/xml/CSPROFILE.dtd.h>


void WriteCSProfileInDatabase(CSProfile* prof, const string& dbDirectory) {
  string csxmlFileName = dbDirectory+"/profile.xml";
  filebuf fb;
  fb.open(csxmlFileName.c_str(), ios::out);
  std::ostream os(&fb);
  WriteCSProfile(prof, os, true);
  fb.close();
}


/* version=1.0.2 for alpha memory profile---FMZ */
void
WriteCSProfile(CSProfile* prof, std::ostream& os, bool prettyPrint)
{
  os << "<?xml version=\"1.0\"?>" << std::endl;
  os << "<!DOCTYPE CSPROFILE [\n" << CSPROFILEdtd << "]>" << std::endl;
  os.flush();
  os << "<CSPROFILE version=\"1.0.2\">\n";
  os << "<CSPROFILEHDR>\n</CSPROFILEHDR>\n"; 
  os << "<CSPROFILEPARAMS>\n";
  os << "<TARGET name"; WriteAttrStr(os, prof->GetTarget()); os << "/>\n";

  // write out metrics
  suint numberofmetrics = prof->GetNumberOfMetrics();
  for (int i=0; i<numberofmetrics; i++) {
      CSProfileMetric * metric = prof->GetMetric(i);
      os << "<METRIC shortName"; WriteAttrNum(os, i);
      os << " nativeName";       WriteAttrNum(os, metric->GetName());
      os << " period";           WriteAttrNum(os, metric->GetPeriod());
      os << " flags";            WriteAttrNum(os, metric->GetFlags());
      os << "/>\n";
   }

  os << "</CSPROFILEPARAMS>\n";

  os.flush();
  
  int dumpFlags = (CSProfTree::XML_TRUE); // CSProfTree::XML_NO_ESC_CHARS
  if (!prettyPrint) { dumpFlags |= CSProfTree::COMPRESSED_OUTPUT; }
  
  prof->GetTree()->Dump(os, dumpFlags);

  os << "</CSPROFILE>\n";
  os.flush();
}

bool 
AddSourceFileInfoToCSProfile(CSProfile* prof, binutils::LM* lm,
                              VMA startaddr, VMA endaddr, bool lastone)
{
  bool noError            = true;
  VMA curr_ip; 

  /* point to the first load module in the Epoch table */
  CSProfTree* tree = prof->GetTree();
  CSProfNode* root = tree->GetRoot(); 

#if 0
  startaddr = lm->GetLM()->GetTextStart();
  endaddr   = lm->GetLM()->GetTextEnd();     

// FMZ debug
// callsite IP need to adjusted to the new text start
  cout << "startadd" << hex <<"0x"<< startaddr <<endl;
  cout << "endadd"   << hex <<"0x"<< endaddr   <<endl;   
#endif

  for (CSProfNodeIterator it(root); it.CurNode(); ++it) { 
       CSProfNode* n = it.CurNode();
       CSProfCallSiteNode* nn = dynamic_cast<CSProfCallSiteNode*>(n);
       if (nn && !(nn->GotSrcInfo()))  {
         curr_ip = nn->GetIP();  //FMZ
         if ((curr_ip >= startaddr ) &&
             (lastone || curr_ip <=endaddr )) { 
              // in the current load module   
              AddSourceFileInfoToCSTreeNode(nn,lm,true);   
              nn->SetSrcInfoDone(true);
          } 
       }    
     }
  return noError;
 }

bool 
AddSourceFileInfoToCSTreeNode(CSProfCallSiteNode* node, 
                              binutils::LM* lm,
                              bool istext)
{
  bool noError = true;
  CSProfCallSiteNode* n = node;  

  if (n) {
    string func, file;
    suint srcLn;
    lm->GetSourceFileInfo(n->GetIP(), n->GetOpIndex(), func, file, srcLn);
    func = GetBestFuncName(func);

    n->SetFile(file);
    n->SetProc(func);
    n->SetLine(srcLn);

    n->SetFileIsText(istext);

    suint procFrameLine;
    lm->GetProcFirstLineInfo( n->GetIP(), n->GetOpIndex(), procFrameLine);
    xDEBUG(DEB_PROC_FIRST_LINE,
	   fprintf(stderr, "after AddSourceFileInfoToCSTreeNode: %s starts at %d, file=%s and p=%s=\n", n->GetProc().c_str(), procFrameLine, file.c_str(), func.c_str()););

    // if file name is missing then using load module name. 
    if (file.empty() || func.empty()) {
        n->SetFile(lm->GetName());
        n->SetLine(0); //don't need to have line number for loadmodule
        n->SetFileIsText(false);
      }
   }  

  return noError;
}

//****************************************************************************
// Set of routines to build a scope tree while reading data file
//****************************************************************************

CSProfile* 
ReadCSProfileFile_HCSPROFILE(const char* fnm,const char *execnm) 
{
  hpcfile_csprof_data_t data; 
  epoch_table_t epochtbl;
  int ret1, ret2;

  
  // Read profile
  FILE* fs = hpcfile_open_for_read(fnm);
  ret1 = hpcfile_csprof_read(fs, &data, &epochtbl, hpcfile_alloc_CB, hpcfile_free_CB);  

  if ( ret1 != HPCFILE_OK) { 
      std::cerr<< "There are errors in reading data file !"  << std::endl;
       return NULL;
   }

  suint num_of_metrics = data.num_metrics;

  fprintf(stderr, "Number of metrics:%d\n", num_of_metrics);
  fflush(stderr);

  CSProfile* prof = new CSProfile(num_of_metrics);

  ret2 = hpcfile_cstree_read(fs, prof->GetTree(), 
			     cstree_create_node_CB, cstree_link_parent_CB,
			     hpcfile_alloc_CB, hpcfile_free_CB, num_of_metrics); 

  if ( ret2 != HPCFILE_OK) { 
      std::cerr<< "There is no performance database generated, the reasons could be :\n" 
               << "\t there is no tree nodes in the data file, i.e no sample collected.\n"
               << "\t there are errors in cstree_read. "  << std::endl; 
      delete prof;
      return NULL;
   }
  
  suint num_of_ldmodules = epochtbl.epoch_modlist->num_loadmodule;

  CSProfEpoch* epochmdlist = new CSProfEpoch(num_of_ldmodules);

  for (int iv=0; iv< num_of_ldmodules; iv++)
    { 
        //get all the  load module
        CSProfLDmodule* mli = new CSProfLDmodule();

         mli->SetName(epochtbl.epoch_modlist[0].loadmodule[iv].name);
         mli->SetVaddr(epochtbl.epoch_modlist[0].loadmodule[iv].vaddr);
         mli->SetMapaddr(epochtbl.epoch_modlist[0].loadmodule[iv].mapaddr);  
         mli->SetUsedFlag(false);
         epochmdlist->SetLoadmodule(iv,mli);

    }
  
  epochmdlist->SortLoadmoduleByVMA(); 

#ifdef XCSPROF_DEBUG 
  std::cerr<<"after sorting" << std::endl;
  epochmdlist->Dump(); 
#endif 

  hpcfile_close(fs);
#if 0
  if ( !(ret1 == HPCFILE_OK && ret2 == HPCFILE_OK) ) { 
    delete prof;
    return NULL; 
   }  
#endif

  // Extract profiling info
  prof->SetTarget(execnm); 
  
  // Extract metrics
  
  for (int i=0; i<num_of_metrics ; i++) {
      CSProfileMetric* metric = prof->GetMetric(i);
      metric->SetName(data.metrics[i].metric_name);
      metric->SetFlags(data.metrics[i].flags);
      metric->SetPeriod(data.metrics[i].sample_period);
   }

  prof->SetEpoch(epochmdlist);

#if 0
  std::cerr<<"***** In Prof ***** " << std::endl;
  prof->ProfileDumpEpoch();
#endif

  // We must deallocate pointer-data
  hpcfile_free_CB(data.target);
  // must deallocate metrics[] and  data.metrics
  // hpcfile_free_CB(data.event);

  // Add PGM node to tree
  AddPGMToCSProfTree(prof->GetTree(), prof->GetTarget());
  
  return prof;
}

static void* 
cstree_create_node_CB(void* tree, 
		      hpcfile_cstree_nodedata_t* data,
		      int num_metrics)
 {
  CSProfTree* t = (CSProfTree*)tree; 
  
  VMA ip;
  ushort opIdx;
  ConvertOpIPToIP((VMA)data->ip, ip, opIdx);
  vector<suint> metricsVector;
  metricsVector.clear();
  int i;
  for (i=0; i<num_metrics; i++) {
    metricsVector.push_back((suint) data->metrics[i]);
  }

  CSProfCallSiteNode* n = new CSProfCallSiteNode(NULL, ip, 
						 opIdx, metricsVector); 
  n->SetSrcInfoDone(false);
  
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
ConvertOpIPToIP(VMA opIP, VMA& ip, ushort& opIdx)
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
typedef std::map<string, CSProfCallSiteNode*> StringToCallSiteMap;
typedef std::map<string, CSProfCallSiteNode*>::iterator 
  StringToCallSiteMapIt;
typedef std::map<string, CSProfCallSiteNode*>::value_type
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
      string myid = string(c->GetFile()) + string(c->GetProc()) 
	+ StrUtil::toStr(c->GetLine());
      
      StringToCallSiteMapIt it = sourceInfoMap.find(myid);
      if (it != sourceInfoMap.end()) { 
	// found -- we have a duplicate
	CSProfCallSiteNode* c1 = (*it).second;
	c1->addMetrics(c);
	/*
	  suint newWeight = c->GetWeight() + c1->GetWeight();
	  suint newCalls = c->GetCalls() + c1->GetCalls();
	  suint newDeath = c->GetDeath() + c1->GetDeath();
	  c1->SetWeight(newWeight);
	  c1->SetCalls(newCalls);
	  c1->SetDeath(newDeath);
	*/
	
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

//***************************************************************************
// Routines for normalizing sibling call sites withing the same procedure
//***************************************************************************
typedef std::map<string, CSProfProcedureFrameNode*> StringToProcedureFramesMap;
typedef std::map<string, CSProfProcedureFrameNode*>::iterator 
  StringToProcedureFramesMapIt;
typedef std::map<string, CSProfProcedureFrameNode*>::value_type
  StringToProcedureFramesMapVal;

bool NormalizeSameProcedureChildren(CSProfile* prof, binutils::LM *lm,
				    VMA startaddr, VMA endaddr, bool lastone);

bool 
NormalizeInternalCallSites(CSProfile* prof, binutils::LM *lm, 
                           VMA startaddr, VMA endaddr, bool lastone)
{
  // Remove duplicate/inplied file and procedure information from tree
  bool pass1 = true;

  bool pass2 = NormalizeSameProcedureChildren(prof, lm,startaddr, endaddr, lastone);
  
  return (pass1 && pass2);
}


// FIXME
// If pc values from the leaves map to the same source file info,
// coalese these leaves into one.
bool NormalizeSameProcedureChildren(CSProfile* prof, CSProfNode* node, binutils::LM* lm,
                                    VMA startaddr, VMA endaddr, bool lastone);

bool 
NormalizeSameProcedureChildren(CSProfile* prof, binutils::LM *lm,
                               VMA startaddr, VMA endaddr, bool lastone)
{
  CSProfTree* csproftree = prof->GetTree();
  if (!csproftree) { return true; }
  
  xDEBUG(DEB_UNIFY_PROCEDURE_FRAME,
	 fprintf(stderr, "start normalizing same procedure children\n");
	 );

  return NormalizeSameProcedureChildren(prof,csproftree->GetRoot(),lm,startaddr,endaddr,lastone);
}


bool 
NormalizeSameProcedureChildren(CSProfile* prof, CSProfNode* node, binutils::LM *lm,
                               VMA startaddr, VMA endaddr, bool lastone)
{
  bool noError = true;
  
  if (!node) { return noError; }

  // FIXME: Use this set to determine if we have a duplicate source line
  StringToProcedureFramesMap proceduresMap; 

  // For each immediate child of this node...
  for (CSProfNodeChildIterator it(node); it.Current(); /* */) {   

    CSProfCodeNode* child = dynamic_cast<CSProfCodeNode*>(it.CurNode());  
    BriefAssertion(child); // always true (FIXME)

    it++; // advance iterator -- it is pointing at 'child' 

    // recur 
    if (! child->IsLeaf()) {
      noError = noError && NormalizeSameProcedureChildren(prof, child, lm,startaddr, endaddr, lastone);
    }

    bool inspect = (child->GetType() == CSProfNode::CALLSITE);
    
    if (inspect) {
      CSProfCallSiteNode* c = dynamic_cast<CSProfCallSiteNode*>(child); 
      VMA curr_ip = c->GetIP(); //FMZ
      if ((curr_ip>= startaddr) && 
         ( lastone || curr_ip<= endaddr))  {  
       //only handle functions in the current load module
        xDEBUG(DEB_UNIFY_PROCEDURE_FRAME,
	   fprintf(stderr, "analyzing node %s %lx\n", 
		   c->GetProc().c_str(), c->GetIP()););

      string myid = c->GetFile() + c->GetProc();
      StringToProcedureFramesMapIt it = proceduresMap.find(myid);
      CSProfProcedureFrameNode* procFrameNode;
      if (it != proceduresMap.end()) { 
	// found 
	procFrameNode = (*it).second;
      } else { 
	// no entry found -- add
	procFrameNode= new CSProfProcedureFrameNode(NULL); 
	procFrameNode->SetFile(c->GetFile());
	
	string procFrameName = c->GetProc();

	xDEBUG(DEB_UNIFY_PROCEDURE_FRAME,
	       std::cerr << "c->GetProc:" << procFrameName << std::endl;
	       );

	if ( !procFrameName.empty() ) {
	  xDEBUG(DEB_UNIFY_PROCEDURE_FRAME,
		 std::cerr << "non empty procedure" << std::endl;
		 );
	} else {
	  procFrameName = string("unknown ") + StrUtil::toStr(c->GetIP(), 16);
	  xDEBUG(DEB_UNIFY_PROCEDURE_FRAME,
		 std::cerr << "empty procedure; use procedure name" << procFrameName << std::endl;
		 );
	}
	procFrameNode->SetProc(procFrameName);

	// if the procedure name could not be found, use 
        // unknown 0Xipaddr

	procFrameNode ->SetFileIsText(c->FileIsText());

	if (!procFrameNode->FileIsText()) {
	  // if child is not text: set f line to 0 and is text to false
	  procFrameNode->SetLine(0);
	} else {
	  // determine the first line of the enclosing procedure
	  suint procFrameLine;
	  xDEBUG(DEB_PROC_FIRST_LINE,
		 fprintf(stderr, "looking up the first line for %s\n",
			 c->GetProc().c_str()););
	  //  determine the appropriate module info for the current 
	  //  procedure frame

	  lm->GetProcFirstLineInfo(c->GetIP(), c->GetOpIndex(), procFrameLine);
	  procFrameNode->SetLine(procFrameLine);
	}

	procFrameNode->Link(node);
	proceduresMap.insert(StringToProcedureFramesMapVal(myid, 
							   procFrameNode));
      }
      
      if ( child->IsLeaf() ) {
	xDEBUG(DEB_UNIFY_PROCEDURE_FRAME,
	       fprintf(stderr, "converting node %s %lx into stmt node begin\n", 
		       c->GetProc().c_str(), c->GetIP()););
	CSProfStatementNode *stmtNode = new CSProfStatementNode(NULL);
	stmtNode->copyCallSiteNode(c);
	xDEBUG(DEB_UNIFY_PROCEDURE_FRAME,
	       fprintf(stderr, "converting node %s %lx into stmt node end\n", 
		       c->GetProc().c_str(), c->GetIP()););
	stmtNode -> Link(procFrameNode);
	child->Unlink();
	delete child;
      } else {
	child->Unlink();
	c->Link(procFrameNode);
      }
  
    } 
  } 
 }
  return noError;
}


/** Perform DFS traversal of the tree nodes and copy
    the source files for the executable into the database
    directory. */
bool copySourceFiles(CSProfNode* node, 
		     std::vector<string>& searchPaths,
		     const string& dbSourceDirectory);

/** Copies the source files for the executable into the database 
    directory. */
void copySourceFiles(CSProfile *prof, 
		     std::vector<string>& searchPaths,
		     const string& dbSourceDirectory) {
  CSProfTree* csproftree = prof->GetTree();
  if (!csproftree) { return ; }

  copySourceFiles(csproftree->GetRoot(), 
		  searchPaths, 
		  dbSourceDirectory);
}


bool innerCopySourceFiles(CSProfNode* node, 
			  std::vector<string>& searchPaths,
			  const string& dbSourceDirectory);

/** Perform DFS traversal of the tree nodes and copy
    the source files for the executable into the database
    directory. */
bool copySourceFiles(CSProfNode* node, 
		     std::vector<string>& searchPaths,
		     const string& dbSourceDirectory) {
  xDEBUG(DEB_MKDIR_SRC_DIR, 
	 cerr << "descend into node" << std::endl;);
  bool noError = true;
  if (!node) {
    return noError;
  }
  // For each immediate child of this node...
  for (CSProfNodeChildIterator it(node); it.CurNode(); it++) {
    // recur 
    noError = noError && copySourceFiles(it.CurNode(), 
					 searchPaths,
					 dbSourceDirectory);
  }
 
  noError = noError && 
    innerCopySourceFiles(node, searchPaths, dbSourceDirectory);
  return noError;
}

/** Normalizes a file path. Handle construncts such as :
 ~/...
 ./...
 .../dir1/../...
 .../dir1/./.... 
 ............./
*/
string normalizeFilePath(const string& filePath, 
			 std::stack<string>& pathSegmentsStack) {

  char cwdName[MAX_PATH_SIZE +1];
  getcwd(cwdName, MAX_PATH_SIZE);
  string crtDir=cwdName; 
  
  string filePathString = filePath;
  char filePathChr[MAX_PATH_SIZE +1];
  strcpy(filePathChr, filePathString.c_str());

  // ~/...
  if ( filePathChr[0] == '~' ) {
    if ( filePathChr[1]=='\0' ||
	 filePathChr[1]=='/' ) {
      // reference to our home directory
      string homeDir = getenv("HOME"); 
      xDEBUG(DEB_NORM_SEARCH_PATH, 
	     cerr << "home dir:" << homeDir << std::endl;);
      filePathString = homeDir + (filePathChr+1); 
      xDEBUG(DEB_NORM_SEARCH_PATH, 
	     cerr << "new filePathString=" << filePathString << std::endl;);
    } else {
      // parse the beginning of the filePath to determine the other username
      int filePos = 0;
      char userHome[MAX_PATH_SIZE+1]; 
      for ( ;
	    ( filePathChr[filePos] != '\0' &&
	      filePathChr[filePos] != '/');
	    filePos++) {
	userHome[filePos]=filePathChr[filePos];
      }
      userHome[filePos]='\0';
      if (chdir (userHome) == 0 ) {
	char userHomeDir[MAX_PATH_SIZE +1];
	getcwd(userHomeDir, MAX_PATH_SIZE);
	filePathString = userHomeDir;
	filePathString = filePathString + (filePathChr + filePos);
	xDEBUG(DEB_NORM_SEARCH_PATH, 
	       cerr << "new filePathString=" << 
	       filePathString << std::endl;);
      }
    }
  }

 
  // ./...
  if ( filePathChr[0] == '.') {
    xDEBUG(DEB_NORM_SEARCH_PATH, 
	   cerr << "crt dir:" << crtDir << std::endl;);
    filePathString = crtDir + "/"+string(filePathChr+1); 
    xDEBUG(DEB_NORM_SEARCH_PATH, 
	   cerr << "updated filePathString=" << filePathString << std::endl;);
  }

  if ( filePathChr[0] != '/') {
    xDEBUG(DEB_NORM_SEARCH_PATH, 
	   cerr << "crt dir:" << crtDir << std::endl;);
    filePathString = crtDir + "/"+ string(filePathChr); 
    xDEBUG(DEB_NORM_SEARCH_PATH, 
	   cerr << "applied crtDir: " << crtDir << " and got updated filePathString=" << filePathString << std::endl;);
  }

  // ............./
  if ( filePathChr[ strlen(filePathChr) -1] == '/') {
    strcpy(filePathChr, filePathString.c_str());
    filePathChr [strlen(filePathChr)-1]='\0';
    filePathString = filePathChr; 
    xDEBUG(DEB_NORM_SEARCH_PATH, 
	   cerr << "after removing trailing / got filePathString=" << filePathString << std::endl;);
  }

  // parse the path and determine segments separated by '/' 
  strcpy(filePathChr, filePathString.c_str());

  int crtPos=0;
  char crtSegment[MAX_PATH_SIZE+1];
  int crtSegmentPos=0;
  crtSegment[crtSegmentPos]='\0';

  for (; crtPos <= strlen(filePathChr); crtPos ++) {
    xDEBUG(DEB_NORM_SEARCH_PATH, 
	   cerr << "parsing " << filePathChr[crtPos] << std::endl;);
    if (filePathChr[crtPos] ==  '/' || 
	filePathChr[crtPos] ==  '\0') {
      crtSegment[crtSegmentPos]='\0';
      xDEBUG(DEB_NORM_SEARCH_PATH, 
	     cerr << "parsed segment " << crtSegment << std::endl;);
      if ( !strcmp(crtSegment,".")) {
	//  .../dir1/./.... 
      } else if (!strcmp(crtSegment,"..")) {
	// .../dir1/../...
	if (pathSegmentsStack.empty()) {
	  cerr << "Invalid path" << filePath << std::endl ;
	  return "";
	} else {
	  pathSegmentsStack.pop();
	}
      }  else {
	if (crtSegmentPos>0) {
	  pathSegmentsStack.push(string(crtSegment));
	  xDEBUG(DEB_NORM_SEARCH_PATH, 
		 cerr << "pushed segment " << crtSegment << std::endl;);
	}
      }
      crtSegment[0]='\0';
      crtSegmentPos=0;
    } else {
      crtSegment[crtSegmentPos] = filePathChr[crtPos];
      crtSegmentPos++;
    }
  }

  if (pathSegmentsStack.empty()) {
    cerr << "Invalid path" << filePath << std::endl ;
    return "";
  }

  std::stack<string> stackCopy = pathSegmentsStack;
  string resultPath = stackCopy.top();
  stackCopy.pop();
  for (;! stackCopy.empty(); ) {
    resultPath = stackCopy.top() + "/"+resultPath;
    stackCopy.pop();
  }
  resultPath = "/"+resultPath;
  xDEBUG(DEB_NORM_SEARCH_PATH, 
	 cerr << "normalized path: " << resultPath << std::endl;);
  return resultPath;
}

string normalizeFilePath(const string& filePath) {
  std::stack<string> pathSegmentsStack;
  string resultPath = normalizeFilePath(filePath, pathSegmentsStack);
  return resultPath;
}

/** Decompose a normalized path into path segments.*/
void breakPathIntoSegments(const string& normFilePath, 
			   std::stack<string>& pathSegmentsStack) {
  string resultPath = normalizeFilePath(normFilePath,
					pathSegmentsStack);
}



bool innerCopySourceFiles (CSProfNode* node, 
			   std::vector<string>& searchPaths,
			   const string& dbSourceDirectory) {
  bool inspect; 
  string nodeSourceFile;
  string relativeSourceFile;
  string procedureFrame;
  bool sourceFileWasCopied = false;
  bool fileIsText = false; 

  switch( node->GetType()) {
  case CSProfNode::CALLSITE:
    {
      CSProfCallSiteNode* c = dynamic_cast<CSProfCallSiteNode*>(node);
      nodeSourceFile = c->GetFile();
      fileIsText = c->FileIsText();
      inspect = true;
      procedureFrame = c->GetProc();
      xDEBUG(DEB_MKDIR_SRC_DIR,
	     cerr << "will analyze CALLSITE for proc" << procedureFrame
	     << nodeSourceFile << 
	     "textFile " << fileIsText << std::endl;);
    }
    break;
  case CSProfNode::STATEMENT:
    {
      CSProfStatementNode* st = 
	dynamic_cast<CSProfStatementNode*>(node);
      nodeSourceFile = st->GetFile();
      fileIsText = st->FileIsText();
      inspect = true;
      procedureFrame = st->GetProc();
      xDEBUG(DEB_MKDIR_SRC_DIR,
	     cerr << "will analyze STATEMENT for proc" << procedureFrame
	     << nodeSourceFile << 
	     "textFile " << fileIsText << std::endl;);
    }
    break;
  case CSProfNode::PROCEDURE_FRAME:
    {
      CSProfProcedureFrameNode* pf = 
	dynamic_cast<CSProfProcedureFrameNode*>(node);
      nodeSourceFile = pf->GetFile();
      fileIsText = pf->FileIsText();
      inspect = true;
      procedureFrame = pf->GetProc();
      xDEBUG(DEB_MKDIR_SRC_DIR,
	     cerr << "will analyze PROCEDURE_FRAME for proc" << 
	     procedureFrame << nodeSourceFile << 
	     "textFile " << fileIsText << std::endl;);
    }
    break;
  default:
    inspect = false;
    break;
  } 
  if (inspect) {
    // copy source file for current node
    xDEBUG(DEB_MKDIR_SRC_DIR,
	   cerr << "attempt to copy " << nodeSourceFile << std::endl;);
// FMZ     if (! nodeSourceFile.empty()) {
     if (fileIsText && !nodeSourceFile.empty()) {
      xDEBUG(DEB_MKDIR_SRC_DIR,
	     cerr << "attempt to copy text, nonnull " << nodeSourceFile << std::endl;);
      
      // foreach  search paths
      //    normalize  searchPath + file
      //    if (file can be opened) 
      //    break into components 
      //    duplicate directory structure (mkdir segment by segment)
      //    copy the file (use system syscall)
      int ii;
      bool searchDone = false;
      for (ii=0; !searchDone && ii<searchPaths.size(); ii++) {
	string testPath;
	if ( nodeSourceFile[0] == '/') {
	  testPath = nodeSourceFile;
	  searchDone = true;
	}  else {
	  testPath = searchPaths[ii]+"/"+nodeSourceFile;
	}
	xDEBUG(DEB_MKDIR_SRC_DIR,
	       cerr << "search test path " << testPath << std::endl;);
	string normTestPath = normalizeFilePath(testPath);
	xDEBUG(DEB_MKDIR_SRC_DIR,
	       cerr << "normalized test path " << normTestPath << std::endl;);
	if (normTestPath == "") {
	  xDEBUG(DEB_MKDIR_SRC_DIR,
		 cerr << "null test path " << std::endl;);
	} else {
	  xDEBUG(DEB_MKDIR_SRC_DIR,
		 cerr << "attempt to text open" << normTestPath << std::endl;);
	  FILE *sourceFileHandle = fopen(normTestPath.c_str(), "rt");
	  if (sourceFileHandle != NULL) {
	    searchDone = true;
	    char normTestPathChr[MAX_PATH_SIZE+1];
	    strcpy(normTestPathChr, normTestPath.c_str());
	    relativeSourceFile = normTestPathChr+1;
	    sourceFileWasCopied = true;
	    xDEBUG(DEB_MKDIR_SRC_DIR,
		   cerr << "text open succeeded; path changed to " <<
		   relativeSourceFile << std::endl;);
	    fclose (sourceFileHandle);

	    // check if the file already exists (we've copied it for a previous sample)
	    string testFilePath = dbSourceDirectory + normTestPath;
	    FILE *testFileHandle = fopen(testFilePath.c_str(), "rt");
	    if (testFileHandle != NULL) {
	      fclose(testFileHandle);
	    } else {
	      // we've found the source file and we need to copy it into the database
	      std::stack<string> pathSegmentsStack;
	      breakPathIntoSegments (normTestPath,
				     pathSegmentsStack);
	      std::vector<string> pathSegmentsVector;
	      for (; !pathSegmentsStack.empty();) {
		string crtSegment = pathSegmentsStack.top();
		pathSegmentsStack.pop(); 
		pathSegmentsVector.insert(pathSegmentsVector.begin(),
					  crtSegment);
		xDEBUG(DEB_MKDIR_SRC_DIR,
		       cerr << "inserted path segment " << 
		       pathSegmentsVector[0] << std::endl;);
	      }

	      xDEBUG(DEB_MKDIR_SRC_DIR,
		     cerr << "converted stack to vector" << std::endl;);

	      char filePathChr[MAX_PATH_SIZE +1];
	      strcpy(filePathChr, dbSourceDirectory.c_str());
	      chdir(filePathChr);

	      xDEBUG(DEB_MKDIR_SRC_DIR,
		     cerr << "after chdir " << std::endl;);

	      string subPath = dbSourceDirectory;
	      int pathSegmentIndex;
	      for (pathSegmentIndex=0; 
		   pathSegmentIndex<pathSegmentsVector.size()-1;
		   pathSegmentIndex++) {
		subPath = subPath+"/"+pathSegmentsVector[pathSegmentIndex];
		xDEBUG(DEB_MKDIR_SRC_DIR,
		       cerr << "about to mkdir " 
		       << subPath << std::endl;);
		int mkdirResult =  
		  mkdir (subPath.c_str(), 
			 S_IRWXU | S_IRGRP | S_IXGRP 
			 | S_IROTH | S_IXOTH); 
		if (mkdirResult == -1) {
		  switch (errno) {
		  case EEXIST:  
		    xDEBUG(DEB_MKDIR_SRC_DIR,
			   cerr << "EEXIST " << std::endl;); 
		    // we created the same directory 
		    // for a different source file
		    break;
		  default:
		    cerr << "mkdir failed for " << subPath << std::endl;
		    perror("mkdir failed:");
		    exit (1);
		  }
		}
	      }
	      strcpy(filePathChr, subPath.c_str());
	      chdir(filePathChr);
	      string cpCommand = "cp -f "+normTestPath+" .";
	      system (cpCommand.c_str());
	      // fix the file name  so it points to the one in the source directory
	    }
	  }
	}
      }
    }
  }
  if (inspect && sourceFileWasCopied) {
    switch( node->GetType()) {
    case CSProfNode::CALLSITE:
      {
	CSProfCallSiteNode* c = dynamic_cast<CSProfCallSiteNode*>(node);
	c->SetFile(relativeSourceFile);
      }
      break;
    case CSProfNode::STATEMENT:
      {
	CSProfStatementNode* st = 
	  dynamic_cast<CSProfStatementNode*>(node);
	st->SetFile(relativeSourceFile);
      }
      break;
    case CSProfNode::PROCEDURE_FRAME:
      {
	CSProfProcedureFrameNode* pf = 
	  dynamic_cast<CSProfProcedureFrameNode*>(node);
	pf->SetFile(relativeSourceFile);
      }
      break;
    default:
      cerr << "Invalid node type" << std::endl;
      exit(0);
      break;
    }
  }

  return true;
}

void
LdmdSetUsedFlag(CSProfile* prof)
{ 
   VMA curr_ip;  

   CSProfTree* tree = prof->GetTree();
   CSProfNode* root = tree->GetRoot();

   for (CSProfNodeIterator it(root); it.CurNode(); ++it) {
    CSProfNode* n = it.CurNode();
    CSProfCallSiteNode* nn = dynamic_cast<CSProfCallSiteNode*>(n);

    if (nn)  {
      curr_ip = nn->GetIP();
      CSProfLDmodule * csploadmd = prof->GetEpoch()->FindLDmodule(curr_ip);
      if (!(csploadmd->GetUsedFlag()))
          csploadmd->SetUsedFlag(true);
    }
   }  
} 

