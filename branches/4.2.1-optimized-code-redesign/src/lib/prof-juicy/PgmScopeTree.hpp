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

#ifndef PgmScopeTree_H 
#define PgmScopeTree_H

//************************* System Include Files ****************************

#include <iostream>
#include <string>
#include <list> // STL
#include <set>  // STL
#include <map>  // STL
#include <string>
 
//*************************** User Include Files ****************************

#include <include/general.h>

#include "PerfMetric.hpp"

#include <lib/binutils/VMAInterval.hpp>

#include <lib/support/Unique.hpp>
#include <lib/support/NonUniformDegreeTree.hpp>
#include <lib/support/Files.hpp>
#include <lib/support/Nan.h>

//*************************** Forward Declarations **************************

class ScopeInfo;

// Some possibly useful containers
typedef std::list<ScopeInfo*> ScopeInfoList;
typedef std::set<ScopeInfo*> ScopeInfoSet;

//*************************** Forward Declarations **************************

const suint UNDEF_LINE = 0;

class DoubleVector;

class GroupScopeMap;
class LoadModScopeMap;
class FileScopeMap;
class ProcScopeMap;
class StmtRangeScopeMap;


//***************************************************************************
// PgmScopeTree
//***************************************************************************

class PgmScope;

class PgmScopeTree : public Unique {
public:
  enum {
    // User-level bit flags
    XML_FALSE =	(0 << 0),	/* No XML format */
    XML_TRUE  =	(1 << 0),	/* XML format */

    COMPRESSED_OUTPUT = (1 << 1),  /* Use compressed output format */

    DUMP_LEAF_METRICS = (1 << 2),  /* Dump only leaf metrics */

    // Not-generally-user-level bit flags
    XML_NO_ESC_CHARS = (1 << 10), /* don't substitute XML escape characters */

    // Private bit flags
    XML_EMPTY_TAG    = (1 << 15)  /* this is an empty XML tag */
    
  };

public:
  // Constructor/Destructor
  PgmScopeTree(const char* name, PgmScope* _root = NULL);
  PgmScopeTree(const std::string& name, PgmScope* _root = NULL)
    { PgmScopeTree(name.c_str(), _root); }

  virtual ~PgmScopeTree();

  // Tree data
  PgmScope* GetRoot() const { return root; }
  void SetRoot(PgmScope* x) { root = x; }
  bool IsEmpty() const { return (root == NULL); }
  
  void CollectCrossReferences();

  virtual void xml_dump(std::ostream& os = std::cerr, 
			int dmpFlag = XML_TRUE) const;


  // Dump contents for inspection (use flags from ScopeInfo)
  virtual void dump(std::ostream& os = std::cerr, 
		    int dmpFlag = XML_TRUE) const;
  virtual void ddump() const;
 
private:
  PgmScope* root;
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
class AlienScope;
class LoopScope;
class StmtRangeScope;
class RefScope;

// ---------------------------------------------------------
// ScopeInfo: The base node for a program scope tree
// ---------------------------------------------------------
class ScopeInfo: public NonUniformDegreeTreeNode {
public:
  enum ScopeType {
    PGM = 0,
    GROUP,
    LM,
    FILE,
    PROC,
    ALIEN,
    LOOP,
    STMT_RANGE,
    REF,
    ANY,
    NUMBER_OF_SCOPES
  };

  static const std::string& ScopeTypeToName(ScopeType tp);
  static const std::string& ScopeTypeToXMLelement(ScopeType tp);
  static ScopeType     IntToScopeType(long i);

protected:
  ScopeInfo(const ScopeInfo& x) { *this = x; }
  ScopeInfo& operator=(const ScopeInfo& x);

private:
  static const std::string ScopeNames[NUMBER_OF_SCOPES];
  
public:
  ScopeInfo(ScopeType type, ScopeInfo* parent = NULL);
  virtual ~ScopeInfo();
  
  // --------------------------------------------------------
  // General Interface to fields 
  // --------------------------------------------------------
  ScopeType     Type() const         { return type; }
  unsigned int  UniqueId() const     { return uid; }

  // name() is overridden by some scopes
  virtual const std::string& name() const { return ScopeTypeToName(Type()); }

  void CollectCrossReferences();
  int NoteHeight();
  void NoteDepth();

  int ScopeHeight() const { return height; }
  int ScopeDepth() const { return depth; }

  bool   HasPerfData(int i) const;     // checks whether PerfData(i) is set
  double PerfData(int i) const;        // returns NaN iff !HasPerfData(i) 
  void   SetPerfData(int i, double d); // asserts out iff HasPerfData(i) 
  
  // --------------------------------------------------------
  // Parent
  // --------------------------------------------------------
  ScopeInfo *Parent() const 
  { return (ScopeInfo*) NonUniformDegreeTreeNode::Parent(); }
  
  CodeInfo* CodeInfoParent() const;  // return dyn_cast<CodeInfo*>(Parent())
  
  // --------------------------------------------------------
  // Ancestor: find first ScopeInfo in path from this to root with given type
  // (Note: We assume that a node *can* be an ancestor of itself.)
  // --------------------------------------------------------
  ScopeInfo* Ancestor(ScopeType type) const;
  ScopeInfo* Ancestor(ScopeType tp1, ScopeType tp2) const;
  
  PgmScope*       Pgm() const;           // return Ancestor(PGM)
  GroupScope*     Group() const;         // return Ancestor(GROUP)
  LoadModScope*   LoadMod() const;       // return Ancestor(LM)
  FileScope*      File() const;          // return Ancestor(FILE)
  ProcScope*      Proc() const;          // return Ancestor(PROC)
  AlienScope*     Alien() const;         // return Ancestor(ALIEN)
  LoopScope*      Loop() const;          // return Ancestor(LOOP)
  StmtRangeScope* StmtRange() const;     // return Ancestor(STMT_RANGE)

  // LeastCommonAncestor: Given two ScopeInfo nodes, return the least
  // common ancestor (deepest nested common ancestor) or NULL.
  static ScopeInfo* LeastCommonAncestor(ScopeInfo* n1, ScopeInfo* n2);

  // --------------------------------------------------------
  // Tree navigation 
  //   1) all ScopeInfos contain CodeInfos as children 
  //   2) PgmRoot is the only ScopeInfo type that is not also a CodeInfo;
  //      since PgmScopes have no siblings, it is safe to make Next/PrevScope 
  //      return CodeInfo pointers 
  // --------------------------------------------------------
  CodeInfo* FirstEnclScope() const;      // return  FirstChild()
  CodeInfo* LastEnclScope()  const;      // return  LastChild()
  CodeInfo* NextScope()      const;      // return  NULL or NextSibling()
  CodeInfo* PrevScope()      const;      // return  NULL or PrevSibling()
  bool      IsLeaf()         const       { return  FirstEnclScope() == NULL; }

  // --------------------------------------------------------
  // Paths and Merging
  // --------------------------------------------------------

  // Distance: Given two ScopeInfo nodes, a node and some ancestor,
  // return the distance of the path between the two.  The distance
  // between a node and its direct ancestor is 1.  If there is no path
  // between the two nodes, returns a negative number; if the two
  // nodes are equal, returns 0.
  static int Distance(ScopeInfo* ancestor, ScopeInfo* descendent);

  // ArePathsOverlapping: Given two nodes and their least common
  // ancestor, lca, returns whether the paths from the nodes to lca
  // overlap.
  //
  // Let d1 and d2 be two nodes descended from their least common
  // ancestor, lca.  Furthermore, let the path p1 from d1 to lca be as
  // long or longer than the path p2 from d2 to lca.  (Thus, d1 is
  // nested as deep or more deeply than d2.)  If the paths p1 and p2 are
  // overlapping then d2 will be somewhere on the path between d1 and
  // lca.
  //
  // Examples: 
  // 1. Overlapping: lca --- d2 --- ... --- d1
  //
  // 2. Divergent:   lca --- d1
  //                    \--- d2
  //
  // 3. Divergent:   lca ---...--- d1
  //                    \---...--- d2
  static bool ArePathsOverlapping(ScopeInfo* lca, ScopeInfo* desc1, 
				  ScopeInfo* desc2);
  
  // MergePaths: Given divergent paths (as defined above), merges the path
  // from 'toDesc' into 'fromDesc'. If a merge takes place returns true.
  static bool MergePaths(ScopeInfo* lca, 
			 ScopeInfo* toDesc, ScopeInfo* fromDesc);
  
  // Merge: Given two nodes, 'fromNode' and 'toNode', merges the
  // former into the latter, if possible.  If the merge takes place,
  // deletes 'fromNode' and returns true; otherwise returns false.
  static bool Merge(ScopeInfo* toNode, ScopeInfo* fromNode);

  // IsMergable: Returns whether 'fromNode' is capable of being merged
  // into 'toNode'
  static bool IsMergable(ScopeInfo* toNode, ScopeInfo* fromNode);
  
  // --------------------------------------------------------
  // cloning
  // --------------------------------------------------------
  
  // Clone: return a shallow copy, unlinked from the tree
  virtual ScopeInfo* Clone() { return new ScopeInfo(*this); }
  
  // --------------------------------------------------------
  // XML output
  // --------------------------------------------------------

  std::string toStringXML(int dmpFlag = 0, const char* pre = "") const;
  
  virtual std::string toXML(int dmpFlag = 0) const;

  bool XML_DumpSelfBefore(std::ostream& os = std::cout,
		int dmpFlag = 0, const char* prefix = "") const;
  void XML_DumpSelfAfter (std::ostream& os = std::cout,
		int dmpFlag = 0, const char* prefix = "") const;
  void XML_dump(std::ostream& os = std::cout,
		int dmpFlag = 0, const char* pre = "") const;
  
  virtual void XML_DumpLineSorted(std::ostream& os = std::cout,
				  int dmpFlag = 0,
				  const char* pre = "") const;

  void xml_ddump() const;

  // --------------------------------------------------------
  // Other output
  // --------------------------------------------------------

  void CSV_DumpSelf(const PgmScope &root, std::ostream& os = std::cout) const;
  virtual void CSV_dump(const PgmScope &root, std::ostream& os = std::cout, 
			const char* file_name = NULL, 
			const char* proc_name = NULL,
			int lLevel = 0) const;

  void TSV_DumpSelf(const PgmScope &root, std::ostream& os = std::cout) const;
  virtual void TSV_dump(const PgmScope &root, std::ostream& os = std::cout, 
			const char* file_name = NULL, 
			const char* proc_name = NULL,
			int lLevel = 0) const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------

  virtual std::string Types() const; // instance's base and derived types 

  std::string toString(int dmpFlag = 0, const char* pre = "") const;

  std::string toString_id(int dmpFlag = 0) const;
  std::string toString_me(int dmpFlag = 0, const char* pre = "") const;

  // dump
  std::ostream& dump(std::ostream& os = std::cerr, 
		     int dmpFlag = 0, const char* pre = "") const;
  
  void ddump() const;

  virtual std::ostream& dumpme(std::ostream& os = std::cerr, 
			       int dmpFlag = 0,
			       const char* pre = "") const;

protected:
  ScopeType type;
  unsigned int uid;
  int height; // cross reference information
  int depth;
  DoubleVector* perfData;
};

// --------------------------------------------------------------------------
// CodeInfo is a base class for all scopes other than PGM and LM.
// Describes some kind of code, i.e. Files, Procedures, Loops...
// --------------------------------------------------------------------------
class CodeInfo : public ScopeInfo {
protected: 
  CodeInfo(ScopeType t, ScopeInfo* mom = NULL, 
	   suint begLn = UNDEF_LINE, suint endLn = UNDEF_LINE,
	   VMA begVMA = 0, VMA endVMA = 0);
  CodeInfo(const CodeInfo& x) : ScopeInfo(x.type) { *this = x; }
  CodeInfo& operator=(const CodeInfo& x);

public: 
  virtual ~CodeInfo();

  // Line range in source code
  const suint begLine() const { return mbegLine; }
  suint&      begLine()       { return mbegLine; }

  const suint endLine() const { return mendLine; }
  suint&      endLine()       { return mendLine; }

  void SetLineRange(suint begLn, suint endLn, int propagate = 1);

  void LinkAndSetLineRange(CodeInfo* parent);


  // A set of *unrelocated* VMAs associated with this scope
  VMAIntervalSet&       vmaSet()       { return mvmaSet; }
  const VMAIntervalSet& vmaSet() const { return mvmaSet; }
  
  // containsLine: returns true if this scope contains line number
  //   'ln'.  A non-zero beg_epsilon and end_epsilon allows fuzzy
  //   matches by expanding the interval of the scope.
  //
  // containsInterval: returns true if this scope fully contains the
  //   interval specified by [begLn...endLn].  A non-zero beg_epsilon
  //   and end_epsilon allows fuzzy matches by expanding the interval of
  //   the scope.
  //
  // Note: We assume that it makes no sense to compare against UNDEF_LINE.
  bool containsLine(suint ln) const
    { return (mbegLine != UNDEF_LINE && (mbegLine <= ln && ln <= mendLine)); }
  bool containsLine(suint ln, int beg_epsilon, int end_epsilon) const;
  bool containsInterval(suint begLn, suint endLn) const
    { return (containsLine(begLn) && containsLine(endLn)); }
  bool containsInterval(suint begLn, suint endLn,
			int beg_epsilon, int end_epsilon) const
    { return (containsLine(begLn, beg_epsilon, end_epsilon) 
	      && containsLine(endLn, beg_epsilon, end_epsilon)); }

  CodeInfo* CodeInfoWithLine(suint ln) const;

  // returns a string of the form: 
  //   File()->name() + ":" + <Line-Range> 
  //
  // where Line-Range is either: 
  //                     begLine() + "-" + endLine()      or simply 
  //                     begLine() 
  virtual std::string CodeName() const;
  virtual std::string LineRange() const;

  static std::string CodeLineName(suint line);

  virtual ScopeInfo* Clone() { return new CodeInfo(*this); }

  CodeInfo* GetFirst() const { return first; } 
  CodeInfo* GetLast() const { return last; } 

  // --------------------------------------------------------
  // XML output
  // --------------------------------------------------------

  virtual std::string toXML(int dmpFlag = 0) const;

  virtual std::string XMLLineRange(int dmpFlag) const;
  virtual std::string XMLVMAIntervals(int dmpFlag) const;

  virtual void CSV_dump(const PgmScope &root, std::ostream& os = std::cout, 
               const char* file_name = NULL, const char* proc_name = NULL,
               int lLevel = 0) const;

  virtual void TSV_dump(const PgmScope &root, std::ostream& os = std::cout, 
               const char* file_name = NULL, const char* proc_name = NULL,
               int lLevel = 0) const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------
  
  virtual std::ostream& dumpme(std::ostream& os = std::cerr, 
			       int dmpFlag = 0,
			       const char* pre = "") const;

protected: 
  void Relocate();

protected:
  suint mbegLine;
  suint mendLine;
  VMAIntervalSet mvmaSet;

  CodeInfo* first; // FIXME: this seems to duplicate NonUniformDegreeTree...
  CodeInfo* last;
  friend void ScopeInfo::CollectCrossReferences();
};


// - if x < y; 0 if x == y; + otherwise
// N.B.: in the case that x == y, break ties using VMAIntervalSet and
// then by name attributes.
int CodeInfoLineComp(const CodeInfo* x, const CodeInfo* y);


//***************************************************************************
// PgmScope, GroupScope, LoadModScope, FileScope, ProcScope, LoopScope,
// StmtRangeScope
//***************************************************************************

// --------------------------------------------------------------------------
// PgmScope is root of the scope tree
// --------------------------------------------------------------------------
class PgmScope: public ScopeInfo {
protected:
  PgmScope(const PgmScope& x) : ScopeInfo(x.type) { *this = x; }
  PgmScope& operator=(const PgmScope& x);

public: 
  PgmScope(const char* nm);
  PgmScope(const std::string& nm);

  virtual ~PgmScope();

  const std::string& name() const { return m_name; }
  void               SetName(const char* n) { m_name = n; }
  void               SetName(const std::string& n) { m_name = n; }

  LoadModScope* FindLoadMod(const char* nm) const; // find by 'realpath'
  LoadModScope* FindLoadMod(const std::string& nm) const 
    { return FindLoadMod(nm.c_str()); }

  // FIXME: probably better moved to LoadModule
  FileScope*    FindFile(const char* nm) const;    // find by 'realpath'
  FileScope*    FindFile(const std::string& nm) const
    { return FindFile(nm.c_str()); }

  GroupScope*   FindGroup(const char* nm) const;
  GroupScope*   FindGroup(const std::string& nm) const
    { return FindGroup(nm.c_str()); }

  void Freeze() { frozen = true;} // disallow additions to/deletions from tree
  bool IsFrozen() const { return frozen; }

  virtual ScopeInfo* Clone() { return new PgmScope(*this); }

  // --------------------------------------------------------
  // XML output
  // --------------------------------------------------------

  virtual std::string toXML(int dmpFlag = 0) const;

  virtual void XML_DumpLineSorted(std::ostream& os = std::cout, 
				  int dmpFlag = 0, 
				  const char* pre = "") const;
  void CSV_TreeDump(std::ostream& os = std::cout) const;
  void TSV_TreeDump(std::ostream& os = std::cout) const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------
  
  virtual std::ostream& dumpme(std::ostream& os = std::cerr, 
			       int dmpFlag = 0,
			       const char* pre = "") const;
   
protected: 
private: 
  void Ctor(const char* nm);

  void AddToGroupMap(GroupScope& grp);
  void AddToLoadModMap(LoadModScope& lm);
  void AddToFileMap(FileScope& file);

  friend class GroupScope;
  friend class LoadModScope;
  friend class FileScope;

private:
  bool frozen;
  std::string m_name; // the program name

  GroupScopeMap*     groupMap;
  LoadModScopeMap*   lmMap;     // mapped by 'realpath'
  FileScopeMap*      fileMap;   // mapped by 'realpath'
};


// --------------------------------------------------------------------------
// GroupScopes are children of PgmScope's, GroupScope's, LoadModScopes's, 
//   FileScope's, ProcScope's, LoopScope's
// children: GroupScope's, LoadModScope's, FileScope's, ProcScope's,
//   LoopScope's, StmtRangeScopes,
// They may be used to describe several different types of scopes
//   (including user-defined ones)
// --------------------------------------------------------------------------
class GroupScope: public CodeInfo {
public: 
  GroupScope(const char* nm, ScopeInfo* mom,
	     int begLn = UNDEF_LINE, int endLn = UNDEF_LINE);
  GroupScope(const std::string& nm, ScopeInfo* mom,
	     int begLn = UNDEF_LINE, int endLn = UNDEF_LINE);
  virtual ~GroupScope();
  
  const std::string& name() const { return m_name; } // same as grpName

  virtual std::string CodeName() const;

  virtual ScopeInfo* Clone() { return new GroupScope(*this); }

  virtual std::string toXML(int dmpFlag = 0) const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------
  
  virtual std::ostream& dumpme(std::ostream& os = std::cerr, 
			       int dmpFlag = 0,
			       const char* pre = "") const;

private:
  void Ctor(const char* nm, ScopeInfo* mom);

private: 
  std::string m_name;
};


// --------------------------------------------------------------------------
// LoadModScopes are children of PgmScope's or GroupScope's
// children: GroupScope's, FileScope's
// --------------------------------------------------------------------------
// FIXME: See note about LoadModScope above.
class LoadModScope: public CodeInfo {
protected:
  LoadModScope& operator=(const LoadModScope& x);

public: 
  LoadModScope(const char* nm, ScopeInfo* mom);
  LoadModScope(const std::string& nm, ScopeInfo* mom);
  virtual ~LoadModScope();

  virtual std::string BaseName() const  { return BaseFileName(m_name); }

  const std::string& name() const { return m_name; }

  virtual std::string CodeName() const;

  virtual ScopeInfo* Clone() { return new LoadModScope(*this); }

  // find scope by *unrelocated* VMA
  CodeInfo* findByVMA(VMA vma);
  
  ProcScope* findProc(VMA vma) 
  {
    VMAInterval toFind(vma, vma+1); // [vma, vma+1)
    VMAIntervalMap<ProcScope*>::iterator it = procMap->find(toFind);
    return (it != procMap->end()) ? it->second : NULL;
  }

  StmtRangeScope* findStmtRange(VMA vma)
  {
    VMAInterval toFind(vma, vma+1); // [vma, vma+1)
    VMAIntervalMap<StmtRangeScope*>::iterator it = stmtMap->find(toFind);
    return (it != stmtMap->end()) ? it->second : NULL;
  }

  virtual std::string toXML(int dmpFlag = 0) const;

  virtual void XML_DumpLineSorted(std::ostream& os = std::cout, 
				  int dmpFlag = 0, 
				  const char* pre = "") const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------
  
  virtual std::ostream& dumpme(std::ostream& os = std::cerr, 
			       int dmpFlag = 0,
			       const char* pre = "") const;
  void dumpmaps() const;

  bool verifyStmtMap() const;

public:
  typedef VMAIntervalMap<ProcScope*>      VMAToProcMap;
  typedef VMAIntervalMap<StmtRangeScope*> VMAToStmtRangeMap;

protected: 
  void Ctor(const char* nm, ScopeInfo* mom);

  template<typename T> 
  void buildMap(VMAIntervalMap<T>*& m, ScopeInfo::ScopeType ty) const;

  template<typename T>
  static bool 
  verifyMap(VMAIntervalMap<T>* m, const char* map_nm);

private: 
  std::string m_name; // the load module name
  
  VMAToProcMap*      procMap;
  VMAToStmtRangeMap* stmtMap;
};


// --------------------------------------------------------------------------
// FileScopes are children of PgmScope's, GroupScope's and LoadModScope's.
// children: GroupScope's, ProcScope's, LoopScope's, or StmtRangeScope's.
// FileScopes may refer to an unreadable file
// --------------------------------------------------------------------------
class FileScope: public CodeInfo {
protected:
  FileScope(const FileScope& x) : CodeInfo(x.type) { *this = x; }
  FileScope& operator=(const FileScope& x);

public: 
  // fileNameWithPath/mom must not be NULL
  // srcIsReadable == fopen(fileNameWithPath, "r") works 
  FileScope(const char* fileNameWithPath, bool srcIsReadable_, 
	    ScopeInfo *mom, 
	    suint begLn = UNDEF_LINE, suint endLn = UNDEF_LINE);
  FileScope(const std::string& fileNameWithPath, bool srcIsReadable_, 
	    ScopeInfo *mom, 
	    suint begLn = UNDEF_LINE, suint endLn = UNDEF_LINE);
  virtual ~FileScope();


  static FileScope* 
  findOrCreate(LoadModScope* lmScope, const std::string& filenm);


 // fileNameWithPath from constructor 
  const std::string& name() const { return m_name; }

  // FindProc: Attempt to find the procedure within the multimap.  If
  // 'lnm' is provided, require that link names match.
  ProcScope* FindProc(const char* nm, const char* lnm = NULL) const;
  ProcScope* FindProc(const std::string& nm, const std::string& lnm = "") const
    { 
      const char* x = lnm.c_str();
      return FindProc(nm.c_str(), (x[0] == '\0') ? NULL : x);
    }
  
                                        
  void SetName(const char* fname) { m_name = fname; }
  void SetName(const std::string& fname) { m_name = fname; }
    
  virtual std::string BaseName() const  { return BaseFileName(m_name); }
  virtual std::string CodeName() const;

  bool HasSourceFile() const { return srcIsReadable; } // srcIsReadable

  virtual ScopeInfo* Clone() { return new FileScope(*this); }

  virtual std::string toXML(int dmpFlag = 0) const;

  virtual void CSV_dump(const PgmScope &root, std::ostream& os = std::cout, 
			const char* file_name = NULL, 
			const char* proc_name = NULL,
			int lLevel = 0) const;
  virtual void TSV_dump(const PgmScope &root, std::ostream& os = std::cout, 
			const char* file_name = NULL, 
			const char* proc_name = NULL,
			int lLevel = 0) const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------
  
  virtual std::ostream& dumpme(std::ostream& os = std::cerr, 
			       int dmpFlag = 0,
			       const char* pre = "") const;

private: 
  void Ctor(const char* srcFileWithPath, bool srcIsReadble_, 
	    ScopeInfo* mom);

  void AddToProcMap(ProcScope& proc);
  friend class ProcScope;

private:
  bool srcIsReadable;
  std::string m_name; // the file name including the path 
  ProcScopeMap* procMap;
};


// --------------------------------------------------------------------------
// ProcScopes are children of GroupScope's or FileScope's
// children: GroupScope's, LoopScope's, StmtRangeScope's
// 
//   (begLn == 0) <==> (endLn == 0)
//   (begLn != 0) <==> (endLn != 0)
// --------------------------------------------------------------------------
class ProcScope: public CodeInfo {
protected:
  ProcScope(const ProcScope& x) : CodeInfo(x.type) { *this = x; }
  ProcScope& operator=(const ProcScope& x);

public: 
  ProcScope(const char* name, CodeInfo* mom, 
	    const char* linkname,
	    suint begLn = UNDEF_LINE, suint endLn = UNDEF_LINE);
  ProcScope(const std::string& name, CodeInfo* mom, 
	    const std::string& linkname,
	    suint begLn = UNDEF_LINE, suint endLn = UNDEF_LINE);

  virtual ~ProcScope();
  
  
  static ProcScope*
  findOrCreate(FileScope* fScope, const std::string& procnm, suint line);
  
  
  virtual const std::string& name() const     { return m_name; }
  virtual const std::string& LinkName() const { return m_linkname; }

  virtual       std::string CodeName() const;

  virtual ScopeInfo* Clone() { return new ProcScope(*this); }

  StmtRangeScope* FindStmtRange(suint line);  

  virtual std::string toXML(int dmpFlag = 0) const;

  virtual void CSV_dump(const PgmScope &root, std::ostream& os = std::cout, 
			const char* file_name = NULL, 
			const char* proc_name = NULL,
			int lLevel = 0) const;
  virtual void TSV_dump(const PgmScope &root, std::ostream& os = std::cout, 
			const char* file_name = NULL, 
			const char* proc_name = NULL,
			int lLevel = 0) const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------
  
  virtual std::ostream& dumpme(std::ostream& os = std::cerr, 
			       int dmpFlag = 0,
			       const char* pre = "") const;

private:
  void Ctor(const char* n, CodeInfo* mom, const char* ln);

  void AddToStmtMap(StmtRangeScope& stmt);

  friend class StmtRangeScope;

private:
  std::string m_name;
  std::string m_linkname;
  StmtRangeScopeMap* stmtMap;
};


// --------------------------------------------------------------------------
// AlienScope: represents an alien context (e.g. inlined procedure, macro).
//
// AlienScopes are children of GroupScope's, AlienScope's, ProcScope's
//   or LoopScope's
// children: GroupScope's, AlienScope's, LoopScope's, StmtRangeScope's
// 
//   (begLn == 0) <==> (endLn == 0)
//   (begLn != 0) <==> (endLn != 0)
// --------------------------------------------------------------------------
class AlienScope: public CodeInfo {
protected:
  AlienScope(const AlienScope& x) : CodeInfo(x.type) { *this = x; }
  AlienScope& operator=(const AlienScope& x);

public: 
  AlienScope(CodeInfo* mom, const char* filenm, const char* procnm,
	     suint begLn = UNDEF_LINE, suint endLn = UNDEF_LINE);
  AlienScope(CodeInfo* mom, 
	     const std::string& filenm, const std::string& procnm,
	     suint begLn = UNDEF_LINE, suint endLn = UNDEF_LINE);
  virtual ~AlienScope();
  
  const std::string& fileName() const { return m_filenm; }
  virtual const std::string& name() const { return m_name; }
  
  virtual       std::string CodeName() const;

  virtual ScopeInfo* Clone() { return new AlienScope(*this); }

  virtual std::string toXML(int dmpFlag = 0) const;

  virtual void CSV_dump(const PgmScope &root, std::ostream& os = std::cout, 
			const char* file_name = NULL, 
			const char* proc_name = NULL,
			int lLevel = 0) const;
  virtual void TSV_dump(const PgmScope &root, std::ostream& os = std::cout, 
			const char* file_name = NULL, 
			const char* proc_name = NULL,
			int lLevel = 0) const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------
  
  virtual std::ostream& dumpme(std::ostream& os = std::cerr, 
			       int dmpFlag = 0,
			       const char* pre = "") const;

private:
  void Ctor(CodeInfo* mom, const char* filenm, const char* procnm);

private:
  std::string m_filenm;
  std::string m_name;
};


// --------------------------------------------------------------------------
// LoopScopes are children of GroupScope's, FileScope's, ProcScope's,
//   or LoopScope's.
// children: GroupScope's, LoopScope's, or StmtRangeScope's
// --------------------------------------------------------------------------
class LoopScope: public CodeInfo {
public: 
  LoopScope(CodeInfo* mom, 
	    suint begLn = UNDEF_LINE, suint endLn = UNDEF_LINE);
  virtual ~LoopScope();
  
  virtual std::string CodeName() const;

  virtual ScopeInfo* Clone() { return new LoopScope(*this); }

  virtual std::string toXML(int dmpFlag = 0) const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------
  
  virtual std::ostream& dumpme(std::ostream& os = std::cerr, 
			       int dmpFlag = 0,
			       const char* pre = "") const;

};


// --------------------------------------------------------------------------
// StmtRangeScopes are children of GroupScope's, FileScope's,
//   ProcScope's, or LoopScope's.
// children: none
// --------------------------------------------------------------------------
class StmtRangeScope: public CodeInfo {
public: 
  StmtRangeScope(CodeInfo* mom, suint begLn, suint endLn,
		 VMA begVMA = 0, VMA endVMA = 0);
  virtual ~StmtRangeScope();
  
  virtual std::string CodeName() const;

  virtual ScopeInfo* Clone() { return new StmtRangeScope(*this); }

  virtual std::string toXML(int dmpFlag = 0) const;  

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------
  
  virtual std::ostream& dumpme(std::ostream& os = std::cerr, 
			       int dmpFlag = 0,
			       const char* pre = "") const;
};

// ----------------------------------------------------------------------
// RefScopes are chldren of LineScopes 
// They are used to describe a reference in source code.
// RefScopes are build only iff we have preprocessing information.
// ----------------------------------------------------------------------
class RefScope: public CodeInfo {
public: 
  RefScope(CodeInfo* mom, int _begPos, int _endPos, const char* refName);
  // mom->Type() == STMT_RANGE_SCOPE 
  
  unsigned int BegPos() const { return begPos; };
  unsigned int EndPos() const   { return endPos; };
  
  virtual const std::string& name() const { return m_name; }

  virtual std::string CodeName() const;

  virtual ScopeInfo* Clone() { return new RefScope(*this); }

  virtual std::string toXML(int dmpFlag = 0) const;

  // --------------------------------------------------------
  // debugging
  // --------------------------------------------------------
  
  virtual std::ostream& dumpme(std::ostream& os = std::cerr, 
			       int dmpFlag = 0,
			       const char* pre = "") const;

private: 
  void RelocateRef();
  unsigned int begPos;
  unsigned int endPos;
  std::string m_name;
};

/************************************************************************/
// Iterators
/************************************************************************/

#include "PgmScopeTreeIterator.hpp" 

/************************************************************************/
// testing 
/************************************************************************/
extern void ScopeInfoTester(int argc, const char** argv);

#endif 
