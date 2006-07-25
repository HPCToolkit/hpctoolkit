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

#ifndef ScopeInfo_h 
#define ScopeInfo_h 

//************************ System Include Files ******************************

#include <iostream>

//************************* User Include Files *******************************

#include <include/general.h>

#include "PerfMetric.hpp"

#include <lib/support/Unique.hpp>
#include <lib/support/NonUniformDegreeTree.hpp>
#include <lib/support/String.hpp>
#include <lib/support/Files.hpp>
#include <lib/support/Nan.h>

//************************ Forward Declarations ******************************

#define UNDEF_LINE 0

class WordSetSortedIterator;
class DoubleVector;

class GroupScopeMap;
class LoadModScopeMap;
class FileScopeMap;
class ProcScopeMap;
class LineScopeMap;

//***************************************************************************
// ScopesInfo
//***************************************************************************

class PgmScope;

class ScopesInfo : public Unique {
public:
  ScopesInfo(const char* name);
  ~ScopesInfo();
  
  PgmScope* Root() const { return root; };
  void SetRoot(PgmScope* newRoot);

  void CollectCrossReferences();

private:
  PgmScope *root;
};


//***************************************************************************
// ScopeInfo, CodeInfo.
//***************************************************************************

// FIXME: It would make more sense for GroupScope and LoadModScope to
// simply be ScopeInfos and not CodeInfos, but the assumption that
// *only* a PgmScope is not a CodeInfo is deeply embedded and would
// take a while to untangle.

class ScopeInfo;   // Base class for all scopes
class CodeInfo;    // Base class for everyone but PGM

class PgmScope;    // Tree root
class GroupScope;
class LoadModScope;
class FileScope;
class ProcScope;
class LoopScope;
class StmtRangeScope;

class LineScope;  // to be deprecated FIXME
class RefScope;   // to be deprecated

// ---------------------------------------------------------
// ScopeInfo: The base node for a program scope tree
// ---------------------------------------------------------
class ScopeInfo: public NonUniformDegreeTreeNode {
public:

  enum ScopeType {
    PGM,
    GROUP,
    LM,
    FILE,
    PROC,
    LOOP,
    STMT_RANGE,
    
    LINE,  // to be deprecated FIXME
    REF,   // to be deprecated
    
    ANY,
    NUMBER_OF_SCOPES
  };

  static const char* ScopeTypeToName(ScopeType tp);
  static const char* ScopeTypeToXMLelement(ScopeType tp); 
  static ScopeType   IntToScopeType(long i); 


  enum { // Dump flags
    // User-level bit flags
    DUMP_LEAF_METRICS = (1 << 0), /* Dump only leaf metrics */

       /* *** NOTE: none of the following flags are implemented *** */
    XML_COMPRESSED_OUTPUT = (1 << 1),  /* Use compressed output format */

    // Not-generally-user-level bit flags
    XML_NO_ESC_CHARS = (1 << 10), /* don't substitute XML escape characters */

    // Private bit flags
    XML_EMPTY_TAG    = (1 << 15)  /* this is an empty XML tag */
    
  };

protected:
  ScopeInfo(const ScopeInfo& other) { *this = other; }
  ScopeInfo& operator=(const ScopeInfo& other);

public:
  ScopeInfo(ScopeType type, ScopeInfo* parent = NULL);
  virtual ~ScopeInfo(); 
  
  // ---------------------------------------------------------------------
  // Name() is overwritten by: RefScope,ProcScope,FileScope     
  // ---------------------------------------------------------------------
  virtual String Name() const        { return ScopeTypeToName(Type()); }

  // ---------------------------------------------------------------------
  // interface to fields 
  // ---------------------------------------------------------------------
  ScopeType     Type() const         { return type; }
  unsigned int  UniqueId() const     { return uid; }

  bool   HasPerfData(int i) const;     // checks whether PerfData(i) is set
  double PerfData(int i) const;        // returns NaN iff !HasPerfData(i) 
  void   SetPerfData(int i, double d); // asserts out iff HasPerfData(i) 
  
  // ---------------------------------------------------------------------
  // Parent
  // ---------------------------------------------------------------------
  ScopeInfo *Parent() const 
                { return (ScopeInfo*) NonUniformDegreeTreeNode::Parent(); }
  
  CodeInfo  *CodeInfoParent() const;   // return dyn_cast<CodeInfo*>(Parent())
  
  // ---------------------------------------------------------------------
  // find the first ScopeInfo in path from this to root with a given type
  // ---------------------------------------------------------------------
  ScopeInfo* Ancestor(ScopeType type) const; 
  
  PgmScope*       Pgm() const;           // return Ancestor(PGM)
  GroupScope*     Group() const;         // return Ancestor(GROUP)
  LoadModScope*   LoadMod() const;       // return Ancestor(LM)
  FileScope*      File() const;          // return Ancestor(FILE)
  ProcScope*      Proc() const;          // return Ancestor(PROC)
  LoopScope*      Loop() const;          // return Ancestor(LOOP)
  StmtRangeScope* StmtRange() const;     // return Ancestor(STMT_RANGE);

  LineScope*  Line()  const;             // return Ancestor(LINE_SCOPE)

  // ---------------------------------------------------------------------
  // Tree navigation 
  //   1) all ScopeInfos contain CodeInfos as children 
  //   2) PgmRoot is the only ScopeInfo type that is not also a CodeInfo; 
  //      since PgmScopes have no siblings, it is safe to make Next/PrevScope 
  //      return CodeInfo pointers 
  // ---------------------------------------------------------------------
  CodeInfo* FirstEnclScope() const;      // return  FirstChild()
  CodeInfo* LastEnclScope()  const;      // return  LastChild()
  CodeInfo* NextScope()      const;      // return  NULL or NextSibling()
  CodeInfo* PrevScope()      const;      // return  NULL or PrevSibling()
  bool      IsLeaf()         const       { return  FirstEnclScope() == NULL; }
  
  // ---------------------------------------------------------------------
  // cloning
  // ---------------------------------------------------------------------
  
  // Clone: return a shallow copy, unlinked from the tree
  virtual ScopeInfo* Clone() { return new ScopeInfo(*this); }
  
  // ---------------------------------------------------------------------
  // debugging and printing 
  // ---------------------------------------------------------------------
  virtual String ToString() const; 
  virtual String ToXML() const; 
  virtual String Types() ; // lists this instance's base and derived types 
  
  void DumpSelf(std::ostream &os = std::cerr, const char *prefix = "") const;
  void Dump    (std::ostream &os = std::cerr, const char *pre = "") const; 
  void XML_DumpSelf(std::ostream &os = std::cout, int dmpFlag = 0,
		    const char *prefix = "") const;
  void XML_Dump(std::ostream &os = std::cout, int dmpFlag = 0,
		const char *pre = "") const; 

  void CSV_DumpSelf(const PgmScope &root, std::ostream &os = std::cout) const;
  virtual void CSV_Dump(const PgmScope &root, std::ostream &os = std::cout, 
               const char *file_name = NULL, const char *routine_name = NULL,
               int lLevel = 0) const; 

  void TSV_DumpSelf(const PgmScope &root, std::ostream &os = std::cout) const;
  virtual void TSV_Dump(const PgmScope &root, std::ostream &os = std::cout, 
               const char *file_name = NULL, const char *routine_name = NULL,
               int lLevel = 0) const; 

  void CollectCrossReferences();
  int NoteHeight();
  void NoteDepth();

  int ScopeHeight() const { return height; }
  int ScopeDepth() const { return depth; }

protected: 

  ScopeType type;
  unsigned int uid; 
  DoubleVector* perfData; 

  // cross reference information
  int height;
  int depth;
};

// ----------------------------------------------------------------------
// CodeInfo is a base class for all scopes other than PGM and LM.
// Describes some kind of code, i.e. Files, Procedures, Loops...
// ----------------------------------------------------------------------
class CodeInfo : public ScopeInfo {
protected: 
  CodeInfo(ScopeType t, ScopeInfo* mom = NULL, 
	   suint beg = UNDEF_LINE, suint end = UNDEF_LINE); 
  CodeInfo(const CodeInfo& other) : ScopeInfo(other.type) { *this = other; }
  CodeInfo& operator=(const CodeInfo& other);

public: 
  virtual ~CodeInfo();

  suint BegLine() const { return begLine; }
  suint EndLine()   const { return endLine; } 
  
  bool      ContainsLine(suint ln) const; 
  CodeInfo* CodeInfoWithLine(suint ln) const; 

  // ---------------------------------------------------------------------
  // retuns a string of the form: 
  //   File()->Name() + ":" + <Line-Range> + [" " + optional] 
  //
  // where Line-Range is either: 
  //                     BegLine() + "-" + EndLine()      or simply 
  //                     BegLine() 
  // where optional is a ProcScope's or RefScope's Name()
  // ---------------------------------------------------------------------
  virtual String CodeName() const; 

  String CodeLineName(suint line) const; 

  virtual ScopeInfo* Clone() { return new CodeInfo(*this); }

  virtual String ToString() const; 
  virtual String ToXML() const; 
  virtual String XMLLineNumbers() const;
  
  CodeInfo *GetFirst() const { return first; } 
  CodeInfo *GetLast() const { return last; } 

  virtual void CSV_Dump(const PgmScope &root, std::ostream &os = std::cout, 
               const char *file_name = NULL, const char *routine_name = NULL,
               int lLevel = 0) const; 

  virtual void TSV_Dump(const PgmScope &root, std::ostream &os = std::cout, 
               const char *file_name = NULL, const char *routine_name = NULL,
               int lLevel = 0) const; 

protected: 
  void SetLineRange(suint beg, suint end); 
  void Relocate(); 
  suint begLine; 
  suint endLine; 

public:
  CodeInfo *first;
  CodeInfo *last;
}; 

//***************************************************************************
// PgmScope, GroupScope, LoadModScope, FileScope, ProcScope, LoopScope,
// StmtRangeScope
//***************************************************************************

// ---------------------------------------------------------
// PgmScope is root of the scope tree
// ---------------------------------------------------------
class PgmScope: public ScopeInfo {
protected:
  PgmScope(const PgmScope& other) : ScopeInfo(other.type) { *this = other; }
  PgmScope& operator=(const PgmScope& other);

public: 
  PgmScope(const char* n); 
  virtual ~PgmScope(); 

  GroupScope* FindGroup(const char* nm) const;

  // find by 'realpath'
  LoadModScope* FindLoadMod(const char* nm) const;
  FileScope*    FindFile(const char* nm) const;

  void Freeze() { frozen = true;} // disallow additions to/deletions from tree
  bool IsFrozen() const { return frozen; }

  String Name() const { return name; }
  void   SetName(const char* n) { name = n; }

  virtual ScopeInfo* Clone() { return new PgmScope(*this); }

  virtual String ToString() const;
  virtual String ToXML() const;

  void XML_Dump(std::ostream &os = std::cout, int dmpFlag = 0, 
		const char *pre = "") const;
  void CSV_TreeDump(std::ostream &os = std::cout) const;
  void TSV_TreeDump(std::ostream &os = std::cout) const;
   
protected: 
private: 
  void AddToGroupMap(GroupScope& grp);
  void AddToLoadModMap(LoadModScope& lm);
  void AddToFileMap(FileScope& file);
  friend class GroupScope; 
  friend class LoadModScope;
  friend class FileScope; 

  bool frozen;
  String name;  // the program name

  GroupScopeMap*   groupMap;
  LoadModScopeMap* lmMap;     // mapped by 'realpath'
  FileScopeMap*    fileMap;   // mapped by 'realpath'
}; 

// ---------------------------------------------------------
// GroupScopes are children of PgmScope's, GroupScope's, LoadModScopes's, 
//   FileScope's, ProcScope's, LoopScope's
// children: GroupScope's, LoadModScope's, FileScope's, ProcScope's,
//   LoopScope's, StmtRangeScopes,
// They may be used to describe several different types of scopes
//   (including user-defined ones)
// ---------------------------------------------------------
// FIXME: See note about GroupScope above.
class GroupScope: public CodeInfo {
public: 
  GroupScope(const char* grpName, ScopeInfo* mom,
	     int firstLine = UNDEF_LINE,
	     int lastLine = UNDEF_LINE);
  virtual ~GroupScope();

  String Name() const                  { return name; } 

  virtual String CodeName() const;

  virtual ScopeInfo* Clone() { return new GroupScope(*this); }

  virtual String ToString() const;
  virtual String ToXML() const;

private: 
  String name; 
};

// ---------------------------------------------------------
// LoadModScopes are children of PgmScope's or GroupScope's
// children: GroupScope's, FileScope's
// ---------------------------------------------------------
// FIXME: See note about LoadModScope above.
class LoadModScope: public CodeInfo {
public: 
  LoadModScope(const char* lmName, ScopeInfo *mom); 
  virtual ~LoadModScope(); 

  virtual String BaseName() const  { return BaseFileName(name); }
  String Name() const { return name; }

  virtual String CodeName() const;

  virtual ScopeInfo* Clone() { return new LoadModScope(*this); }

  virtual String ToString() const;
  virtual String ToXML() const; 

protected: 
private: 
  String name; // the program name
}; 

// ---------------------------------------------------------
// FileScopes are children of PgmScope's, GroupScope's and LoadModScope's.
// children: GroupScope's, ProcScope's, LoopScope's, or StmtRangeScope's.
// FileScopes may refer to an unreadable file
// ---------------------------------------------------------
class FileScope: public CodeInfo {
protected:
  FileScope(const FileScope& other) : CodeInfo(other.type) { *this = other; }
  FileScope& operator=(const FileScope& other);

public: 
  FileScope(const char *fileNameWithPath, bool srcIsReadable_, 
	    const char* preproc, 
	    ScopeInfo *mom, int beg = UNDEF_LINE, int end = UNDEF_LINE);
            // fileNameWithPath/mom must not be NULL
            // srcIsReadable == fopen(fileNameWithPath, "r") works 
  virtual ~FileScope(); 

  String Name() const { return name; } // fileNameWithPath from constructor 

  ProcScope* FindProc(const char* nm) const;
                                        
  virtual void SetName(const char* fname) { name = fname; }
    
  virtual String BaseName() const  { return BaseFileName(name); }
  virtual String CodeName() const;

  bool HasSourceFile() const { return srcIsReadable; } // srcIsReadable

  virtual ScopeInfo* Clone() { return new FileScope(*this); }
  
  virtual String ToString() const; 
  virtual String ToXML() const; 

  virtual void CSV_Dump(const PgmScope &root, std::ostream &os = std::cout, 
               const char *file_name = NULL, const char *routine_name = NULL,
               int lLevel = 0) const; 
  virtual void TSV_Dump(const PgmScope &root, std::ostream &os = std::cout, 
               const char *file_name = NULL, const char *routine_name = NULL,
               int lLevel = 0) const; 
  
private: 
  void AddToProcMap(ProcScope& proc); 
  friend class ProcScope; 

  bool srcIsReadable; 
  String name; // the file name including the path 
  ProcScopeMap* procMap; 
};

// ---------------------------------------------------------
// ProcScopes are children of GroupScope's or FileScope's
// children: GroupScope's, LoopScope's, StmtRangeScope's
// ---------------------------------------------------------
class ProcScope: public CodeInfo {
protected:
  ProcScope(const ProcScope& other) : CodeInfo(other.type) { *this = other; }
  ProcScope& operator=(const ProcScope& other);

public: 
  ProcScope(const char* name, CodeInfo *mom, 
	    int firstLine = UNDEF_LINE, int lastLine = UNDEF_LINE);
  virtual ~ProcScope();
  
  virtual String Name() const       { return name; }; 
  virtual String CodeName() const; 

  virtual ScopeInfo* Clone() { return new ProcScope(*this); }

  virtual String ToString() const; 
  virtual String ToXML() const; 

  // FIXME: deprecated
  // return a line scope from lineMap or a new one if none is found
  LineScope *GetLineScope(suint line);  
  LineScope *CreateLineScope(CodeInfo *mom, suint lineNumber); 

  virtual void CSV_Dump(const PgmScope &root, std::ostream &os = std::cout, 
               const char *file_name = NULL, const char *routine_name = NULL,
               int lLevel = 0) const; 
  virtual void TSV_Dump(const PgmScope &root, std::ostream &os = std::cout, 
               const char *file_name = NULL, const char *routine_name = NULL,
               int lLevel = 0) const; 

private: 
  String name; 
  LineScopeMap *lineMap; 
};

// ---------------------------------------------------------
// LoopScopes are children of GroupScope's, FileScope's, ProcScope's,
//   or LoopScope's.
// children: GroupScope's, LoopScope's, or StmtRangeScope's
// ---------------------------------------------------------
class LoopScope: public CodeInfo {
public: 
  LoopScope(CodeInfo *mom, 
	    suint firstLine = UNDEF_LINE, suint lastLine = UNDEF_LINE);
  virtual ~LoopScope();
  
  virtual String CodeName() const;

  virtual ScopeInfo* Clone() { return new LoopScope(*this); }

  virtual String ToString() const; 
  virtual String ToXML() const; 
  
};

// ---------------------------------------------------------
// StmtRangeScopes are children of GroupScope's, FileScope's,
//   ProcScope's, or LoopScope's.
// children: none
class StmtRangeScope: public CodeInfo {
public: 
  StmtRangeScope(CodeInfo *mom, int firstLine, int lastLine); 
  virtual ~StmtRangeScope();
  
  virtual String CodeName() const;

  virtual ScopeInfo* Clone() { return new StmtRangeScope(*this); }

  virtual String ToString() const;  
  virtual String ToXML() const;  

};

// ----------------------------------------------------------------------
// LineScopes are chldren of ProcScopes
// They are used to describe a line of source code, which may correspond 
// to multiple lines of measured code, iff the source was preprocessed
// ----------------------------------------------------------------------
class LineScope: public CodeInfo {
public: 
  LineScope(CodeInfo *mom, suint srcLine); 
  
  virtual ScopeInfo* Clone() { return new LineScope(*this); }
};

// ----------------------------------------------------------------------
// RefScopes are chldren of LineScopes 
// They are used to describe a reference in source code.
// RefScopes are build only iff we have preprocessing information.
// ----------------------------------------------------------------------
class RefScope: public CodeInfo {
public: 
  RefScope(CodeInfo *mom, int _begPos, int _endPos, const char *refName); 
  // mom->Type() == LINE_SCOPE 
  
  unsigned int BegPos() const { return begPos; }; 
  unsigned int EndPos() const   { return endPos; }; 
  
  virtual String Name() const   { return name; }
  virtual String CodeName() const; 

  virtual ScopeInfo* Clone() { return new RefScope(*this); }

  virtual String ToString() const; 
  virtual String ToXML() const; 

private: 
  void RelocateRef(); 
  unsigned int begPos; 
  unsigned int endPos; 
  String name; 
};


#include "ScopeInfoIterator.hpp"

/************************************************************************/
// testing 
/************************************************************************/
extern void ScopeInfoTester(int argc, const char** argv); 

#endif 
