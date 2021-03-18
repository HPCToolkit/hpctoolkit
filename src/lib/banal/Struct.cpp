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
// Copyright ((c)) 2002-2021, Rice University
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

// This file builds and prints an hpcstruct file with loops and inline
// sequences based on input from the ParseAPI Control-Flow Graph
// support in Dyninst.  We no longer use Open Analysis or the
// Prof::Struct classes.
//
// The internal representation of the structure info is defined in two
// files:
//
//   Struct-Skel.hpp -- defines the top-level skeleton classes for
//   Files, Groups and Procedures (Functions).
//
//   Struct-Inline.hpp -- defines the Inline Tree classes for Loops,
//   Statements (Instructions) and inline sequences.
//
// Printing the internal objects in the hpcstruct XML format is
// handled in Struct-Output.cpp.

//***************************************************************************

#include <sys/types.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <include/uint.h>

#if ENABLE_VG_ANNOTATIONS == 1
#include <valgrind/helgrind.h>
#include <valgrind/drd.h>
#else
#define ANNOTATE_HAPPENS_BEFORE(X)
#define ANNOTATE_HAPPENS_AFTER(X)
#endif

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <mutex>
#include <ostream>
#include <sstream>

#include <lib/binutils/BinUtils.hpp>
#include <lib/binutils/VMAInterval.hpp>
#include <lib/binutils/ElfHelper.hpp>
#include <lib/binutils/InputFile.hpp>
#include <lib/support/FileNameMap.hpp>
#include <lib/support/FileUtil.hpp>
#include <lib/support/RealPathMgr.hpp>
#include <lib/support/StringTable.hpp>
#include <lib/support/dictionary.h>

#include <boost/atomic.hpp>

#include <CFG.h>
#include <CodeObject.h>
#include <CodeSource.h>
#include <Function.h>
#include <Instruction.h>
#include <Module.h>
#include <Region.h>
#include <Symtab.h>

#include <include/hpctoolkit-config.h>

#include "Struct.hpp"
#include "Struct-Inline.hpp"
#include "Struct-Output.hpp"
#include "Struct-Skel.hpp"

#include "gpu/ReadCudaCFG.hpp"

#ifdef ENABLE_IGC
#include "gpu/ReadIntelCFG.hpp"
#endif // ENABLE_IGC

#ifdef ENABLE_OPENMP
#include <omp.h>
#endif

using namespace Dyninst;
using namespace Inline;
using namespace InstructionAPI;
using namespace SymtabAPI;
using namespace ParseAPI;
using namespace std;


//******************************************************************************
// macros
//******************************************************************************

#ifdef DYNINST_USE_CUDA
#define SYMTAB_ARCH_CUDA(symtab) \
  ((symtab)->getArchitecture() == Dyninst::Arch_cuda)
#else
#define SYMTAB_ARCH_CUDA(symtab) 0
#endif

#define DEBUG_CFG_SOURCE  0
#define DEBUG_MAKE_SKEL   0
#define DEBUG_SHOW_GAPS   0
#define DEBUG_SKEL_SUMMARY  0

#if DEBUG_CFG_SOURCE || DEBUG_MAKE_SKEL || DEBUG_SHOW_GAPS
#define DEBUG_ANY_ON  1
#else
#define DEBUG_ANY_ON  0
#endif

#define CUDA_PROC_SEARCH_LEN 32
#define WORK_LIST_PCT  0.05

static int merge_irred_loops = 1;


//******************************************************************************
// variables
//******************************************************************************

// Copied from lib/support/dictionary.h
static const string & unknown_file = UNKNOWN_FILE;
static const string & unknown_proc = UNKNOWN_PROC;
static const string & unknown_link = UNKNOWN_LINK;

// FIXME: temporary until the line map problems are resolved
static Symtab * the_symtab = NULL;
static int cuda_arch = 0;
static int intel_gpu_arch = 0;
// We relocate the symbols and line maps of cubins to 'original_offset+cubin_size'
// to handle the cases in which relocated offsets conflicts with original information
static size_t cubin_size = 0;


static BAnal::Struct::Options opts;

//----------------------------------------------------------------------

namespace BAnal {
namespace Struct {

class HeaderInfo;
class WorkEnv;
class WorkItem;
class LineMapCache;

typedef map <Block *, bool> BlockSet;
typedef map <VMA, HeaderInfo> HeaderList;
typedef map <VMA, Region *> RegionMap;
typedef vector <Statement::Ptr> StatementVector;
typedef vector <WorkItem *> WorkList;

static FileMap *
makeSkeleton(CodeObject *, const string &);

static void
doWorkItem(WorkItem *, string &, bool, bool);

static void
makeWorkList(FileMap *, WorkList &, WorkList &);

static void
printWorkList(WorkList &, uint &, ostream *, ostream *, string &);

static void
doFunctionList(WorkEnv &, FileInfo *, GroupInfo *, bool);

static LoopList *
doLoopTree(WorkEnv &, FileInfo *, GroupInfo *, ParseAPI::Function *,
	   BlockSet &, LoopTreeNode *);

static TreeNode *
doLoopLate(WorkEnv &, GroupInfo *, ParseAPI::Function *,
	   BlockSet &, Loop *, const string &);

static void
doBlock(WorkEnv &, GroupInfo *, ParseAPI::Function *,
	BlockSet &, Block *, TreeNode *);

static void
addGaps(WorkEnv & env, FileInfo *, GroupInfo *);

static void
getStatement(StatementVector &, Offset, SymtabAPI::Function *);

static LoopInfo *
findLoopHeader(WorkEnv & env, FileInfo *, GroupInfo *, ParseAPI::Function *,
	       TreeNode *, Loop *, const string &);

static TreeNode *
deleteInlinePrefix(WorkEnv &, TreeNode *, Inline::InlineSeqn);

static void
mergeIrredLoops(WorkEnv &, LoopInfo *);

static void
computeGaps(VMAIntervalSet &, VMAIntervalSet &, VMA, VMA);

static void
doUnparsableFunctionList(WorkEnv & env, FileInfo * finfo, GroupInfo * ginfo);

static void 
doUnparsableFunction(WorkEnv & env, GroupInfo * ginfo, ParseAPI::Function * func,
		     TreeNode * root);

//----------------------------------------------------------------------

#if DEBUG_ANY_ON

#define DEBUG_ANY(expr)  std::cout << expr

static string
debugPrettyName(const string &);

static void
debugElfHeader(ElfFile *);

static void
debugFuncHeader(FileInfo *, ProcInfo *, long, long, string = "");

static void
debugStmt(VMA, int, string &, SrcFile::ln, RealPathMgr *);

static void
debugEdges(Block * block);

static void
debugLoop(GroupInfo *, ParseAPI::Function *, Loop *, const string &,
	  vector <Edge *> &, HeaderList &, RealPathMgr *);

static void
debugInlineTree(TreeNode *, LoopInfo *, HPC::StringTable &, int, bool);

#else  // ! DEBUG_ANY_ON

#define DEBUG_ANY(expr)

#endif

#if DEBUG_CFG_SOURCE
#define DEBUG_CFG(expr)  std::cout << expr
#else
#define DEBUG_CFG(expr)
#endif

#if DEBUG_MAKE_SKEL
#define DEBUG_SKEL(expr)  std::cout << expr
#else
#define DEBUG_SKEL(expr)
#endif

#if DEBUG_SHOW_GAPS
#define DEBUG_GAPS(expr)  std::cout << expr
#else
#define DEBUG_GAPS(expr)
#endif

//----------------------------------------------------------------------

// Info on candidates for loop header.
class HeaderInfo {
public:
  Block * block;
  bool  is_src;
  bool  is_excl;
  int   depth;
  long  file_index;
  long  base_index;
  long  line_num;

  HeaderInfo(Block * blk = NULL)
  {
    block = blk;
    is_src = false;
    is_excl = false;
    depth = 0;
    file_index = 0;
    base_index = 0;
    line_num = 0;
  }
};

//----------------------------------------------------------------------

// The environment for interpreting paths, strings, etc.  We allocate
// one environ per thread to avoid lock contention.
//
class WorkEnv {
public:
  HPC::StringTable * strTab;
  RealPathMgr * realPath;

  WorkEnv()
  {
    strTab = NULL;
    realPath = NULL;
  }
};

// One item in the work list for openmp parallel region.
class WorkItem {
public:
  FileInfo * finfo;
  GroupInfo * ginfo;
  WorkEnv env;
  double cost;
  bool first_proc;
  bool last_proc;
  bool promote;
  boost::atomic <bool> is_done;

  WorkItem(FileInfo * fi, GroupInfo * gi, bool first, bool last, double cst)
  {
    finfo = fi;
    ginfo = gi;
    cost = cst;
    first_proc = first;
    last_proc = last;
    promote = false;
    is_done.store(false);
  }
};

//----------------------------------------------------------------------

// A simple cache of getStatement() that stores one line range.  This
// saves extra calls to getSourceLines() if we don't need them.
//
class LineMapCache {
private:
  SymtabAPI::Function * sym_func;
  RealPathMgr * realPath;
  string  cache_filenm;
  uint    cache_line;
  VMA  start;
  VMA  end;

public:
  LineMapCache(SymtabAPI::Function * sf, RealPathMgr * rp)
  {
    sym_func = sf;
    realPath = rp;
    cache_filenm = "";
    cache_line = 0;
    start = 1;
    end = 0;
  }

  bool
  getLineInfo(VMA vma, string & filenm, uint & line)
  {
    // try cache first
    if (start <= vma && vma < end) {
      filenm = cache_filenm;
      line = cache_line;
      return true;
    }

    // lookup with getStatement() and getSourceLines()
    StatementVector svec;
    getStatement(svec, vma, sym_func);

    if (! svec.empty()) {
      filenm = svec[0]->getFile();
      line = svec[0]->getLine();
      realPath->realpath(filenm);

      cache_filenm = filenm;
      cache_line = line;
      start = svec[0]->startAddr();
      end = svec[0]->endAddr();

      return true;
    }

    // no line info available
    filenm = "";
    line = 0;
    return false;
  }
};

//----------------------------------------------------------------------

// Comparison functions to sort blocks, edges, loops for more
// deterministic output.

// Sort Blocks by start address, low to high.
static bool
BlockLessThan(Block * b1, Block * b2)
{
  return b1->start() < b2->start();
}

// Sort Edges first by target address (usually all the same), and then
// by source address.
static bool
EdgeLessThan(Edge * e1, Edge * e2)
{
  return (e1->trg()->start() < e2->trg()->start())
      || (e1->trg()->start() == e2->trg()->start()
	  && e1->src()->last() < e2->src()->last());
}

// Sort Work Items by the expected 'cost' of their proc group, largest
// to smallest.
static bool
WorkItemGreaterThan(WorkItem * w1, WorkItem * w2)
{
  return w1->cost > w2->cost;
}

// Returns: the min entry VMA for the loop, or else 0 if the loop is
// somehow invalid.  Irreducible loops have more than one entry
// address.
static VMA
LoopMinEntryAddr(Loop * loop)
{
  if (loop == NULL) {
    return 0;
  }

  vector <Block *> entBlocks;
  int num_ents = loop->getLoopEntries(entBlocks);

  if (num_ents < 1) {
    return 0;
  }

  VMA ans = VMA_MAX;
  for (int i = 0; i < num_ents; i++) {
    ans = std::min(ans, entBlocks[i]->start());
  }

  return ans;
}

// Sort Loops (from their LoopTreeNodes) by min entry VMAs.
static bool
LoopTreeLessThan(LoopTreeNode * n1, LoopTreeNode * n2)
{
  return LoopMinEntryAddr(n1->loop) < LoopMinEntryAddr(n2->loop);
}

// Sort LoopInfo objects by min entry VMAs.
static bool
LoopInfoLessThan(LoopInfo * l1, LoopInfo * l2)
{
  return l1->entry_vma < l2->entry_vma;
}

//----------------------------------------------------------------------

// Line map info from SymtabAPI.  Try the Module associated with the
// Symtab Function as a hint first, else look for other modules that
// might contain vma.
//
static void
getStatement(StatementVector & svec, Offset vma, SymtabAPI::Function * sym_func)
{
  svec.clear();

  // try the Module in sym_func first as a hint
  if (sym_func != NULL) {
    Module * mod = sym_func->getModule();

    if (mod != NULL) {
      mod->getSourceLines(svec, vma + cubin_size);
    }
  }

  // else look for other modules
  if (svec.empty()) {
    set <Module *> modSet;
    the_symtab->findModuleByOffset(modSet, vma);

    for (auto mit = modSet.begin(); mit != modSet.end(); ++mit) {
      (*mit)->getSourceLines(svec, vma + cubin_size);
      if (! svec.empty()) {
        break;
      }
    }
  }

  // a known file and unknown line is now legal, but we require that
  // any line map must contain a file name
  if (! svec.empty() && svec[0]->getFile() == "") {
    svec.clear();
  }
}

//----------------------------------------------------------------------

//
// Display time and space usage per phase in makeStructure.
//
static void
printTime(const char *label, struct timeval *tv_prev, struct rusage *ru_prev,
	  struct timeval *tv_now, struct rusage *ru_now)
{
  gettimeofday(tv_now, NULL);
  getrusage(RUSAGE_SELF, ru_now);

  float delta = (float)(tv_now->tv_sec - tv_prev->tv_sec)
    + ((float)(tv_now->tv_usec - tv_prev->tv_usec))/1000000.0;

  printf("%s  %8.1f sec  %8ld meg  %8ld meg", label, delta,
	 (ru_now->ru_maxrss - ru_prev->ru_maxrss)/1024,
	 ru_now->ru_maxrss/1024);

  cout << endl;
}


//
// makeStructure -- the main entry point for hpcstruct realmain().
//
// Read the binutils load module and the parseapi code object, iterate
// over functions, loops and blocks, make an internal inline tree and
// write an hpcstruct file to 'outFile'.
//
// Fixme: may want to rethink the split between tool/hpcstruct and
// lib/banal.
//
void
makeStructure(string filename,
	      ostream * outFile,
	      ostream * gapsFile,
	      string gaps_filenm,
	      string search_path,
	      Struct::Options & structOpts)
{
  struct timeval tv_init, tv_symtab, tv_parse, tv_fini;
  struct rusage  ru_init, ru_symtab, ru_parse, ru_fini;

  opts = structOpts;

#ifdef ENABLE_OPENMP
  omp_set_num_threads(opts.jobs_symtab);
#endif

  InputFile inputFile;

  // failure throws an error up the call chain
  inputFile.openFile(filename, InputFileError_Error);
  ElfFileVector * elfFileVector = inputFile.fileVector();
  string & sfilename = inputFile.fileName();
  const char * cfilename = inputFile.CfileName();

  if (elfFileVector == NULL || elfFileVector->empty()) {
    return;
  }

  Output::printStructFileBegin(outFile, gapsFile, sfilename);
	
  for (uint i = 0; i < elfFileVector->size(); i++) {
    bool parsable = true;
    ElfFile *elfFile = (*elfFileVector)[i];

    if (opts.show_time) {
      cout << "file:  " << elfFile->getFileName() << "\n"
	   << "symtab threads: " << opts.jobs_symtab
	   << "  parse: " << opts.jobs_parse
	   << "  struct: " << opts.jobs_struct << "\n\n";
      printTime("init:  ", &tv_init, &ru_init, &tv_init, &ru_init);
    }

#if DEBUG_ANY_ON
    debugElfHeader(elfFile);
#endif

#ifdef ENABLE_OPENMP
    omp_set_num_threads(opts.jobs_symtab);
#endif

    Symtab * symtab = Inline::openSymtab(elfFile);
    if (symtab == NULL) {
      continue;
    }
    the_symtab = symtab;
    bool cuda_file = SYMTAB_ARCH_CUDA(symtab);

    // pre-compute line map info
    vector <Module *> modVec;
    the_symtab->getAllModules(modVec);

#pragma omp parallel  shared(modVec)
    {
#pragma omp for  schedule(dynamic, 1)
      for (uint i = 0; i < modVec.size(); i++) {
        Module * mod = modVec[i];
        mod->parseLineInformation();
      }
    }  // end parallel

    if (opts.show_time) {
      printTime("symtab:", &tv_init, &ru_init, &tv_symtab, &ru_symtab);
    }

    CodeSource *code_src = NULL;
    CodeObject *code_obj = NULL;

#ifdef ENABLE_OPENMP
    omp_set_num_threads(opts.jobs_parse);
#endif

    bool intel_file = elfFile->isIntelGPUFile();

    if (cuda_file) { // don't run parseapi on cuda binary
      cuda_arch = elfFile->getArch();
      cubin_size = elfFile->getLength();
      parsable = readCudaCFG(search_path, elfFile, the_symtab,
        structOpts.compute_gpu_cfg, &code_src, &code_obj);
    } else if (intel_file) { // don't run parseapi on intel binary
      // TODO(Aaron): determine which generation of intel gpu it is
      intel_gpu_arch = 1;

      #ifdef ENABLE_IGC
      parsable = readIntelCFG(search_path, elfFile, the_symtab,
        structOpts.compute_gpu_cfg, &code_src, &code_obj);
      #endif // ENABLE_IGC
    } else {
      code_src = new SymtabCodeSource(symtab);
      code_obj = new CodeObject(code_src);
      code_obj->parse();
      intel_gpu_arch = 0;
      cuda_arch = 0;
      cubin_size = 0;
    }

    if (opts.show_time) {
      printTime("parse: ", &tv_symtab, &ru_symtab, &tv_parse, &ru_parse);
    }

#ifdef ENABLE_OPENMP
    omp_set_num_threads(opts.jobs_struct);
#endif

    string basename = FileUtil::basename(cfilename);
    FileMap * fileMap = makeSkeleton(code_obj, basename);

    //
    // make two work lists:
    //  wlPrint -- the output order in the struct file as determined
    //    by files and procs from makeSkeleton(),
    //  wlLaunch -- the order we launch doWorkItem(), mostly print
    //    order but with a few, very large funcs moved to the front of
    //    the list.
    //
    WorkList wlPrint;
    WorkList wlLaunch;
    uint num_done = 0;
    mutex output_mtx;

    makeWorkList(fileMap, wlPrint, wlLaunch);
		
    Output::printLoadModuleBegin(outFile, elfFile->getFileName());

#pragma omp parallel  default(none)				\
    shared(wlPrint, wlLaunch, num_done, output_mtx)		\
    firstprivate(outFile, gapsFile, search_path, gaps_filenm, parsable)
    {
#pragma omp for  schedule(dynamic, 1)
      for (uint i = 0; i < wlLaunch.size(); i++) {
	doWorkItem(wlLaunch[i], search_path, parsable, gapsFile != NULL);

	// the printing must be single threaded
	if (output_mtx.try_lock()) {
	  printWorkList(wlPrint, num_done, outFile, gapsFile, gaps_filenm);
	  output_mtx.unlock();
	}
      }
    }  // end parallel

    // with try_lock(), there are interleavings where not all items
    // have been printed.
    printWorkList(wlPrint, num_done, outFile, gapsFile, gaps_filenm);

    Output::printLoadModuleEnd(outFile);

    if (opts.show_time) {
      printTime("struct:", &tv_parse, &ru_parse, &tv_fini, &ru_fini);
      printTime("total: ", &tv_init, &ru_init, &tv_fini, &ru_fini);
      cout << "\nnum funcs: " << wlPrint.size() << "\n" << endl;
    }

    // if this is the last (or only) elf file, then don't bother with
    // piecemeal cleanup.
    if (i + 1 < elfFileVector->size()) {
      for (uint i = 0; i < wlPrint.size(); i++) {
	delete wlPrint[i];
      }

      delete code_obj;
#if 0
      // FIXME: CodeSource::~CodeSource needs to be public
      delete code_src;
#endif
      Inline::closeSymtab();
    }
  }

  Output::printStructFileEnd(outFile, gapsFile);
}

//----------------------------------------------------------------------

//
// Make the inline tree for funcs in one proc group.  This much can
// run concurrently.
//
static void
doWorkItem(WorkItem * witem, string & search_path, bool parsable,
	   bool fullGaps)
{
  FileInfo * finfo = witem->finfo;
  GroupInfo * ginfo = witem->ginfo;

  // each work item gets its own string table and path manager to
  // avoid lock contention.
  HPC::StringTable * strTab = new HPC::StringTable;
  strTab->str2index("");

  PathFindMgr * pathFind = new PathFindMgr;
  PathReplacementMgr * pathReplace = new PathReplacementMgr;
  RealPathMgr * realPath = new RealPathMgr(pathFind, pathReplace);
  realPath->searchPaths(search_path);

  witem->env.strTab = strTab;
  witem->env.realPath = realPath;

  if (parsable) {
    doFunctionList(witem->env, finfo, ginfo, fullGaps);
  } else {
    doUnparsableFunctionList(witem->env, finfo, ginfo);
  }

  ANNOTATE_HAPPENS_BEFORE(&witem->is_done);
  witem->is_done.exchange(true);
}

//----------------------------------------------------------------------

//
// Make work lists for the print order and parallel launch order from
// the skeleton file map.  The launch order is mostly the print order
// with a few, very large funcs moved to the front.
//
// There are two extremes to avoid: (1) we don't want the printing to
// be blocked by one early func that was started late, and (2) we
// don't want one large func to run at the end when all the other
// threads are idle.
//
static void
makeWorkList(FileMap * fileMap, WorkList & wlPrint, WorkList & wlLaunch)
{
  double total_cost = 0.0;

  wlPrint.clear();
  wlLaunch.clear();

  // the print order is determined by the hierarchy of files and
  // groups in the skeleton.
  for (auto fit = fileMap->begin(); fit != fileMap->end(); ++fit) {
    FileInfo * finfo = fit->second;
    auto group_begin = finfo->groupMap.begin();
    auto group_end = finfo->groupMap.end();

    for (auto git = group_begin; git != group_end; ++git) {
      GroupInfo * ginfo = git->second;
      auto next_git = git;  ++next_git;

      // the estimated time is non-linear in the size of the region
      double cost = ginfo->end - ginfo->start;
      cost *= cost;
      total_cost += cost;

      WorkItem * witem =
	new WorkItem(finfo, ginfo, (git == group_begin), (next_git == group_end), cost);

      wlPrint.push_back(witem);
    }
  }

  // if single-threaded, then order doesn't matter
  if (opts.jobs_struct == 1) {
    wlLaunch = wlPrint;
    return;
  }

  //
  // if the expected cost of one function is more than 5% of the ideal
  // parallel run time, then promote it to start early.
  //
  double threshold = WORK_LIST_PCT * total_cost / ((double) opts.jobs_struct);

  for (auto wit = wlPrint.begin(); wit != wlPrint.end(); ++wit) {
    WorkItem * witem = *wit;

    if (witem->cost > threshold) {
      wlLaunch.push_back(witem);
      witem->promote = true;
    }
  }
  std::sort(wlLaunch.begin(), wlLaunch.end(), WorkItemGreaterThan);

  // add the small items in print order
  for (auto wit = wlPrint.begin(); wit != wlPrint.end(); ++wit) {
    WorkItem * witem = *wit;

    if (! witem->promote) {
      wlLaunch.push_back(witem);
    }
  }
}

//----------------------------------------------------------------------

//
// Scan the work list from num_done to end for items that are ready to
// be printed.  The output order is always work list order, regardless
// of order finished.
//
// Note: the output functions have state (index number), so this must
// be called locked or else single threaded.
//
static void
printWorkList(WorkList & workList, uint & num_done, ostream * outFile,
	      ostream * gapsFile, string & gaps_filenm)
{
  while (num_done < workList.size() && workList[num_done]->is_done.load()) {
    ANNOTATE_HAPPENS_AFTER(&workList[num_done]->is_done);
    WorkItem * witem = workList[num_done];
    FileInfo * finfo = witem->finfo;
    GroupInfo * ginfo = witem->ginfo;
    HPC::StringTable * strTab = witem->env.strTab;

    if (witem->first_proc) {
      Output::printFileBegin(outFile, finfo);
    }

    for (auto pit = ginfo->procMap.begin(); pit != ginfo->procMap.end(); ++pit) {
      ProcInfo * pinfo = pit->second;

      if (! pinfo->gap_only) {
	Output::printProc(outFile, gapsFile, gaps_filenm, finfo, ginfo, pinfo, *strTab);
      }
      delete pinfo->root;
      pinfo->root = NULL;
    }

    if (witem->last_proc) {
      Output::printFileEnd(outFile, finfo);
    }

    // delete the work environment
    delete strTab;
    witem->env.strTab = NULL;

    delete witem->env.realPath;
    witem->env.realPath = NULL;

    num_done++;
  }
}

//----------------------------------------------------------------------

// codeMap is a map of all code regions from start vma to Region *.
// Used to find the region containing a vma and thus the region's end.
//
static void
makeCodeMap(RegionMap & codeMap)
{
  DEBUG_SKEL("\n");

  vector <Region *> regVec;
  the_symtab->getCodeRegions(regVec);

  codeMap.clear();

  for (auto it = regVec.begin(); it != regVec.end(); ++it) {
    Region * reg = *it;
    VMA start = reg->getMemOffset();

    codeMap[start] = reg;

    DEBUG_SKEL("code region:  0x" << hex << start
	       << "--0x" << (start + reg->getMemSize()) << dec
	       << "  " << reg->getRegionName() << "\n");
  }

#if DEBUG_MAKE_SKEL
  // print the list of modules
  vector <Module *> modVec;
  the_symtab->getAllModules(modVec);

  cout << "\n";
  for (auto mit = modVec.begin(); mit != modVec.end(); ++mit) {
    Module * mod = *mit;

    if (mod != NULL) {
      cout << "module:  0x" << hex << mod->addr() << dec
	   << "  (lang " << mod->language() << ")"
	   << "  " << mod->fullName() << "\n";
    }
  }
#endif
}

// Note: normally, code regions don't overlap, but if they do, then we
// find the Region with the highest start address that contains vma.
//
// Returns: the Region containing vma, or else NULL.
//
static Region *
findCodeRegion(RegionMap & codeMap, VMA vma)
{
  auto it = codeMap.upper_bound(vma);

  // invariant: vma is not in range it...end
  while (it != codeMap.begin()) {
    --it;

    Region * reg = it->second;
    VMA start = reg->getMemOffset();
    VMA end = start + reg->getMemSize();

    if (start <= vma && vma < end) {
      return reg;
    }
  }

  return NULL;
}

//----------------------------------------------------------------------

// getProcLineMap -- helper for makeSkeleton() to find line map info
// for the start of a Procedure.
//
// Some compilers, especially for CUDA binaries, and sometimes dyninst
// or libdwarf, omit or fail to read line map info for the first few
// bytes of a function.  Rather than returning 'unknown file', we try
// scanning the first few hundred bytes in the proc.
//
static void
getProcLineMap(StatementVector & svec, Offset vma, Offset end,
	       SymtabAPI::Function * sym_func)
{
  svec.clear();

  if (cuda_arch > 0) {
    // TODO(Keren): Use the same method below and remove magic numbers for instruction length
    // mod->getSourceLines(svec, next + cubin_size);
    int len = (cuda_arch >= 70) ? 16 : 8;

    StatementVector tmp;

    // find the minimal line only for the first few instructions
    for (size_t i = vma; i < end && i < vma + len * CUDA_PROC_SEARCH_LEN; i += len) {
      getStatement(tmp, i, sym_func);
      if (!tmp.empty()) {
        if (svec.empty()) {
          svec.push_back(tmp[0]);
        } else if (tmp[0]->getFile() == svec[0]->getFile() &&
          tmp[0]->getLine() < svec[0]->getLine()) {
          svec[0] = tmp[0];
        }
      }
    }

    if (!svec.empty()) {
      return;
    }

    // if no line mapping information found, iterating the whole function until find one
    for (size_t i = vma + len * CUDA_PROC_SEARCH_LEN; i < end; i += len) {
      getStatement(tmp, i, sym_func);
      if (!tmp.empty()) {
        if (svec.empty()) {
          svec.push_back(tmp[0]);
          return;
        }
      }
    }

    return;
  }

  // try a full module lookup first
  getStatement(svec, vma, sym_func);
  if (! svec.empty()) {
    return;
  }

  // confine further searches to the primary module
  if (sym_func == NULL) {
    return;
  }
  Module * mod = sym_func->getModule();

  if (mod == NULL) {
    return;
  }

  // retry every 'step' bytes, but double the step every 8 tries to
  // make a logarithmic search
  const int init_step = 2;
  const int max_tries = 8;
  int step = init_step;
  int num_tries = 0;
  end = std::min(end, vma + 800);

  for (;;) {
    // invariant: vma does not have line info, but next might
    Offset next = vma + step;
    if (next >= end) {
      break;
    }

    mod->getSourceLines(svec, next + cubin_size);
    num_tries++;

    if (! svec.empty()) {
      // rescan the range [vma, next) but start over with a small step
      if (step <= init_step) {
        break;
      }
      svec.clear();
      step = init_step;
      num_tries = 0;
    }
    else {
      // advance vma and double the step after 8 tries
      vma = next;
      if (num_tries >= max_tries) {
        step *= 2;
        num_tries = 0;
      }
    }
  }
}

// addProc -- helper for makeSkeleton() to locate and add one ProcInfo
// object into the global file map.
//
static void
addProc(FileMap * fileMap, ProcInfo * pinfo, string & filenm,
	SymtabAPI::Function * sym_func, VMA start, VMA end, bool alt_file = false)
{
  // locate in file map, or else insert
  FileInfo * finfo = NULL;

  auto fit = fileMap->find(filenm);
  if (fit != fileMap->end()) {
    finfo = fit->second;
  }
  else {
    finfo = new FileInfo(filenm);
    (*fileMap)[filenm] = finfo;
  }

  // locate in group map, or else insert
  GroupInfo * ginfo = NULL;

  auto git = finfo->groupMap.find(start);
  if (git != finfo->groupMap.end()) {
    ginfo = git->second;
  }
  else {
    ginfo = new GroupInfo(sym_func, start, end, alt_file);
    finfo->groupMap[start] = ginfo;
  }

  ginfo->procMap[pinfo->entry_vma] = pinfo;

#if DEBUG_MAKE_SKEL
  cout << "link:    " << pinfo->linkName << "\n"
       << "pretty:  " << pinfo->prettyName << "\n"
       << (alt_file ? "alt-file:  " : "file:    ") << finfo->fileName << "\n"
       << "group:   0x" << hex << ginfo->start << "--0x" << ginfo->end << dec << "\n";
#endif
}

//----------------------------------------------------------------------

// getFuncNames -- helper for makeSkeleton() to select the pretty
// (demangled) and link (mangled) names for a SymtabAPI::Function.
//
// Some functions have multiple symbols with different names for the
// same address (global and weak syms).  We sort the symbol names to
// make the choice deterministic.  We could prefer a global symbol,
// but global is not always unique.
//
// Note: prettynm and linknm are passed in with default (unknown proc)
// values, so don't overwrite them unless there is at least one valid
// name.
//
// Note: we now use symtab only for mangled names and do all the
// demangling ourselves.
//
static void
getFuncNames(SymtabAPI::Function * sym_func, string & prettynm, string & linknm)
{
  auto mangled_begin = sym_func->mangled_names_begin();
  auto mangled_end = sym_func->mangled_names_end();

  for (auto mit = mangled_begin; mit != mangled_end; ++mit) {
    string new_mangled = *mit;

    // sort by: longer mangled name (more info), or else by
    // alphabetically lower name (arbitrary).
    if (mit == mangled_begin
	|| (new_mangled.length() > linknm.length())
	|| (new_mangled.length() == linknm.length()
	    && new_mangled.compare(linknm) < 0))
    {
      linknm = new_mangled;
      prettynm = BinUtil::demangleProcName(linknm);
    }
  }
}

//----------------------------------------------------------------------

// makeSkeleton -- the new buildLMSkeleton
//
// In the ParseAPI version, we iterate over the ParseAPI list of
// functions and match them up with the SymtabAPI functions by entry
// address.  We now use Symtab for the line map info.
//
// Note: several parseapi functions (targ410aa7) may map to the same
// symtab proc, so we make a func list (group).
//
static FileMap *
makeSkeleton(CodeObject * code_obj, const string & basename)
{
  FileMap * fileMap = new FileMap;
  string unknown_base = unknown_file + " [" + basename + "]";
  bool is_shared = ! (the_symtab->isExec());

  // map of code regions to find end of region
  RegionMap codeMap;
  makeCodeMap(codeMap);

  // iterate over the ParseAPI Functions, order by vma
  const CodeObject::funclist & funcList = code_obj->funcs();
  map <VMA, ParseAPI::Function *> funcMap;

  for (auto flit = funcList.begin(); flit != funcList.end(); ++flit) {
    ParseAPI::Function * func = *flit;
    funcMap[func->addr()] = func;
  }

  for (auto fmit = funcMap.begin(); fmit != funcMap.end(); ++fmit) {
    ParseAPI::Function * func = fmit->second;
    SymtabAPI::Function * sym_func = NULL;
    VMA  vma = func->addr();

    auto next_it = fmit;  ++next_it;
    VMA  next_vma = (next_it != funcMap.end()) ? next_it->second->addr() : 0;

    stringstream buf;
    buf << "0x" << hex << vma << dec;
    string  vma_str = buf.str();

    DEBUG_SKEL("\nskel:    " << vma_str << "  " << func->name() << "\n");

    // see if entry vma lies within a valid symtab function
    bool found = the_symtab->getContainingFunction(vma, sym_func);
    VMA  sym_start = 0;
    VMA  sym_end = 0;

    Region * region = NULL;
    VMA  reg_start = 0;
    VMA  reg_end = 0;

    if (found && sym_func != NULL) {
      sym_start = sym_func->getOffset();
      sym_end = sym_start + sym_func->getSize();

      region = sym_func->getRegion();
      if (region != NULL) {
        reg_start = region->getMemOffset();
        reg_end = reg_start + region->getMemSize();
      }
    }

    DEBUG_SKEL("symbol:  0x" << hex << sym_start << "--0x" << sym_end
	       << "  next:  0x" << next_vma
	       << "  region:  0x" << reg_start << "--0x" << reg_end << dec << "\n");

    // symtab doesn't recognize plt funcs and puts them in the wrong
    // region.  to be a valid symbol, the func entry must lie within
    // the symbol's region.
    if (found && sym_func != NULL && region != NULL
	&& reg_start <= vma && vma < reg_end)
    {
      string filenm = unknown_base;
      string linknm = unknown_link + vma_str;
      string prettynm = unknown_proc + " " + vma_str + " [" + basename + "]";
      SrcFile::ln line = 0;

      // symtab lets some funcs (_init) spill into the next region
      if (sym_start < reg_end && reg_end < sym_end) {
        sym_end = reg_end;
      }

      // line map for symtab func
      vector <Statement::Ptr> svec;
      getProcLineMap(svec, sym_start, sym_end, sym_func);

      if (! svec.empty()) {
        filenm = svec[0]->getFile();
        line = svec[0]->getLine();
        RealPathMgr::singleton().realpath(filenm);
      }

      if (vma == sym_start) {
	//
	// case 1 -- group leader of a valid symtab func.  take proc
	// names from symtab func.  this is the normal case (but other
	// cases are also valid).
	//
	DEBUG_SKEL("(case 1)\n");

	getFuncNames(sym_func, prettynm, linknm);
	if (is_shared) {
	  prettynm += " [" + basename + "]";
	}

	ProcInfo * pinfo = new ProcInfo(func, NULL, linknm, prettynm, line,
					sym_func->getFirstSymbol()->getIndex());
	addProc(fileMap, pinfo, filenm, sym_func, sym_start, sym_end);
      }
      else {
	// outline func -- see if parseapi and symtab file names match
	string parse_filenm = unknown_base;
	SrcFile::ln parse_line = 0;

	// line map for parseapi func
	vector <Statement::Ptr> pvec;
	getProcLineMap(pvec, vma, sym_end, sym_func);

	if (! pvec.empty()) {
	  parse_filenm = pvec[0]->getFile();
	  parse_line = pvec[0]->getLine();
	  RealPathMgr::singleton().realpath(parse_filenm);
	}

	string parse_base = FileUtil::basename(parse_filenm.c_str());
	stringstream buf;
	buf << "outline " << parse_base << ":" << parse_line << " (" << vma_str << ")";
	if (is_shared) {
	  buf << " [" << basename << "]";
	}

	linknm = func->name();
	prettynm = buf.str();

	if (filenm == parse_filenm) {
	  //
	  // case 2 -- outline func inside symtab func with same file
	  // name.  use 'outline 0xxxxxx' proc name.
	  //
	  DEBUG_SKEL("(case 2)\n");

	  ProcInfo * pinfo = new ProcInfo(func, NULL, linknm, prettynm, parse_line);
	  addProc(fileMap, pinfo, filenm, sym_func, sym_start, sym_end);
	}
	else {
	  //
	  // case 3 -- outline func but from a different file name.
	  // add proc info to both files: outline file for full parse
	  // (but no gaps), and symtab file for gap only.
	  //
	  DEBUG_SKEL("(case 3)\n");

	  ProcInfo * pinfo = new ProcInfo(func, NULL, linknm, prettynm, parse_line);
	  addProc(fileMap, pinfo, parse_filenm, sym_func, sym_start, sym_end, true);

	  pinfo = new ProcInfo(func, NULL, "", "", 0, 0, true);
	  addProc(fileMap, pinfo, filenm, sym_func, sym_start, sym_end);
	}
      }
    }
    else {
      //
      // case 4 -- no symtab symbol claiming this vma (and thus no
      // line map).  make a fake group at the parseapi entry vma.
      // this normally only happens for plt funcs.
      //
      string linknm = func->name();
      string prettynm = BinUtil::demangleProcName(linknm);
      VMA end = 0;

      // symtab doesn't offer any guidance on demangling in this case
      if (linknm != prettynm
	  && prettynm.find_first_of("()<>") == string::npos) {
        prettynm = linknm;
      }

      region = findCodeRegion(codeMap, vma);
      reg_start = (region != NULL) ? region->getMemOffset() : 0;
      reg_end = (region != NULL) ? (reg_start + region->getMemSize()) : 0;

      DEBUG_SKEL("region:  0x" << hex << reg_start << "--0x" << reg_end << dec << "\n");
      DEBUG_SKEL("(case 4)\n");

      if (next_it != funcMap.end()) {
        end = next_vma;
        if (region != NULL && vma < reg_end && reg_end < end) {
          end = reg_end;
        }
      }
      else if (region != NULL && vma < reg_end) {
        end = reg_end;
      }
      else {
        end = vma + 20;
      }

      // treat short parseapi functions with no symtab symbol as a plt
      // stub.  not an ideal test, but I (krentel) can't find any
      // counter examples.
      if (end - vma <= 32) {
	int len = linknm.length();
	const char *str = linknm.c_str();

	if (len < 5 || strncasecmp(&str[len-4], "@plt", 4) != 0) {
	  linknm += "@plt";
	  prettynm += "@plt";
	}
      }
      if (is_shared) {
	prettynm += " [" + basename + "]";
      }

      ProcInfo * pinfo = new ProcInfo(func, NULL, linknm, prettynm, 0);
      addProc(fileMap, pinfo, unknown_base, NULL, vma, end);
    }
  }

#if DEBUG_SKEL_SUMMARY
  // print the skeleton map
  cout << "\n------------------------------------------------------------\n";

  for (auto fit = fileMap->begin(); fit != fileMap->end(); ++fit) {
    auto finfo = fit->second;

    for (auto git = finfo->groupMap.begin(); git != finfo->groupMap.end(); ++git) {
      auto ginfo = git->second;
      long size = ginfo->procMap.size();
      long num = 0;

      for (auto pit = ginfo->procMap.begin(); pit != ginfo->procMap.end(); ++pit) {
        auto pinfo = pit->second;
        num++;

        cout << "\nentry:   0x" << hex << pinfo->entry_vma << dec
	     << "  (" << num << "/" << size << ")\n"
	     << "group:   0x" << hex << ginfo->start
	     << "--0x" << ginfo->end << dec << "\n"
	     << "file:    " << finfo->fileName << "\n"
	     << "link:    " << pinfo->linkName << "\n"
	     << "pretty:  " << pinfo->prettyName << "\n"
	     << "parse:   " << pinfo->func->name() << "\n"
	     << "line:    " << pinfo->line_num << "\n";
      }
    }
  }
  cout << "\n";
#endif

  return fileMap;
}

//****************************************************************************
// ParseAPI code for functions, loops and blocks
//****************************************************************************

// Remaining TO-DO items:
//
// 4. Irreducible loops -- study entry blocks, loop header, adjacent
// nested loops.  Some nested irreducible loops share entry blocks and
// maybe should be merged.
//
// 5. Compute line ranges for loops and procs to help decide what is
// alien code when the symtab inline info is not available.
//
// 6. Handle code movement wrt loops: loop fusion, fission, moving
// code in/out of loops.
//
// 10. Decide how to handle basic blocks that belong to multiple
// functions.
//
// 11. Improve demangle proc names -- the problem is more if/when to
// demangle rather than how.  Maybe keep a map of symtab mangled to
// pretty names.

//----------------------------------------------------------------------

// Process the functions in one binutils group.  For each ParseAPI
// function in the group, create the inline tree and fill in the
// TreeNode ptr in ProcInfo.
//
// One binutils proc may contain multiple embedded parseapi functions.
// In that case, we create new proc/file scope nodes for each function
// and strip the inline prefix at the call source from the embed
// function.  This often happens with openmp parallel pragmas.
//
static void
doFunctionList(WorkEnv & env, FileInfo * finfo, GroupInfo * ginfo, bool fullGaps)
{
  long num_funcs = ginfo->procMap.size();
  set <Address> coveredFuncs;
  VMAIntervalSet covered;

  // make a map of internal call edges (from target to source) across
  // all funcs in this group.  we use this to strip the inline seqn at
  // the call source from the target func.
  //
  map <VMA, VMA> callMap;

  if (ginfo->sym_func != NULL && !ginfo->alt_file && num_funcs > 1) {
    for (auto pit = ginfo->procMap.begin(); pit != ginfo->procMap.end(); ++pit) {
      ParseAPI::Function * func = pit->second->func;
      const ParseAPI::Function::edgelist & elist = func->callEdges();

      for (auto eit = elist.begin(); eit != elist.end(); ++eit) {
        VMA src = (*eit)->src()->last();
        VMA targ = (*eit)->trg()->start();
        callMap[targ] = src;
      }
    }
  }

  // one binutils proc may contain several parseapi funcs
  long num = 0;
  for (auto pit = ginfo->procMap.begin(); pit != ginfo->procMap.end(); ++pit) {
    ProcInfo * pinfo = pit->second;
    ParseAPI::Function * func = pinfo->func;
    Address entry_addr = func->addr();
    num++;

    // only used for cuda functions
    pinfo->symbol_index = 0;

    // compute the inline seqn for the call site for this func, if
    // there is one.
    Inline::InlineSeqn prefix;
    auto call_it = callMap.find(entry_addr);

    if (call_it != callMap.end()) {
      analyzeAddr(prefix, call_it->second, env.realPath);
    }

#if DEBUG_CFG_SOURCE
    debugFuncHeader(finfo, pinfo, num, num_funcs);

    if (call_it != callMap.end()) {
      cout << "\ncall site prefix:  0x" << hex << call_it->second
	   << " -> 0x" << call_it->first << dec << "\n";
      for (auto pit = prefix.begin(); pit != prefix.end(); ++pit) {
        cout << "inline:  l=" << pit->getLineNum()
	     << "  f='" << pit->getFileName()
	     << "'  p='" << debugPrettyName(pit->getPrettyName()) << "'\n";
      }
    }
#endif

    // see if this function is entirely contained within another
    // function (as determined by its entry block).  if yes, then we
    // don't parse the func.  but we still use its blocks for gaps,
    // unless we've already added the containing func.
    //
    int num_contain = func->entry()->containingFuncs();
    bool add_blocks = true;

    if (num_contain > 1) {
      vector <ParseAPI::Function *> funcVec;
      func->entry()->getFuncs(funcVec);

      // see if we've already added the containing func
      for (auto fit = funcVec.begin(); fit != funcVec.end(); ++fit) {
        Address entry = (*fit)->addr();

        if (coveredFuncs.find(entry) != coveredFuncs.end()) {
          add_blocks = false;
          break;
        }
      }
    }

    // skip duplicated function, blocks already added
    if (! add_blocks) {
      DEBUG_CFG("\nskipping duplicated function:  '" << func->name() << "'\n");
      continue;
    }

    // basic blocks for this function.  put into a vector and sort by
    // start VMA for deterministic output.
    vector <Block *> bvec;
    BlockSet visited;

    const ParseAPI::Function::blocklist & blist = func->blocks();

    for (auto bit = blist.begin(); bit != blist.end(); ++bit) {
      Block * block = *bit;
      bvec.push_back(block);
      visited[block] = false;
    }

    std::sort(bvec.begin(), bvec.end(), BlockLessThan);

    // add to the group's set of covered blocks
    if (! ginfo->alt_file) {
      for (auto bit = bvec.begin(); bit != bvec.end(); ++bit) {
        Block * block = *bit;
	covered.insert(block->start(), block->end());
      }
      coveredFuncs.insert(entry_addr);
    }

    // skip duplicated function, blocks added just now
    if (num_contain > 1) {
      DEBUG_CFG("\nskipping duplicated function:  '" << func->name() << "'\n");
      continue;
    }

    // if the outline func file name does not match the enclosing
    // symtab func, then do the full parse in the outline file
    // (alt-file) and use the symtab file for gaps only.
    if (pinfo->gap_only) {
      DEBUG_CFG("\nskipping full parse (gap only) for function:  '"
		<< func->name() << "'\n");
      continue;
    }

    TreeNode * root = new TreeNode;

    // traverse the loop (Tarjan) tree
    LoopList *llist =
      doLoopTree(env, finfo, ginfo, func, visited, func->getLoopTree());

    DEBUG_CFG("\nnon-loop blocks:\n");

    // process any blocks not in a loop
    for (auto bit = bvec.begin(); bit != bvec.end(); ++bit) {
      Block * block = *bit;
      if (! visited[block]) {
	doBlock(env, ginfo, func, visited, block, root);
      }
    }

    // merge the loops into the proc's inline tree
    FLPSeqn empty;

    for (auto it = llist->begin(); it != llist->end(); ++it) {
      mergeInlineLoop(root, empty, *it);
    }

    // delete the inline prefix from this func, if non-empty
    if (! prefix.empty()) {
      root = deleteInlinePrefix(env, root, prefix);
    }

    // add inline tree to proc info
    pinfo->root = root;

#if DEBUG_CFG_SOURCE
    cout << "\nfinal inline tree:  (" << num << "/" << num_funcs << ")"
	 << "  link='" << pinfo->linkName << "'\n"
	 << "parse:  '" << func->name() << "'\n";

    if (call_it != callMap.end()) {
      cout << "\ncall site prefix:  0x" << hex << call_it->second
	   << " -> 0x" << call_it->first << dec << "\n";
      for (auto pit = prefix.begin(); pit != prefix.end(); ++pit) {
        cout << "inline:  l=" << pit->getLineNum()
	     << "  f='" << pit->getFileName()
	     << "'  p='" << debugPrettyName(pit->getPrettyName()) << "'\n";
      }
    }
    cout << "\n";
    debugInlineTree(root, NULL, *(env.strTab), 0, true);
    cout << "\nend proc:  (" << num << "/" << num_funcs << ")"
	 << "  link='" << pinfo->linkName << "'\n"
	 << "parse:  '" << func->name() << "'\n";
#endif
  }

  // computeGaps relies Block::start() and Block::end() to have
  // the correct address range for a block to detect gaps.
  // We have subclass GPUBlock inheriting ParseAPI::Block.
  // However, currently subclass of ParseAPI::Block cannot update
  // block end, which leads to Block::end() == Block::start() for
  // GPUBlock objects.
  // Therefore, we cannot invoke gap computation for GPU binaries
  if (cuda_arch == 0 && intel_gpu_arch == 0) {
    // add unclaimed regions (gaps) to the group leader, but skip groups
    // in an alternate file (handled in orig file).
    if (! ginfo->alt_file) {
      computeGaps(covered, ginfo->gapSet, ginfo->start, ginfo->end);

      if (! fullGaps) {
        addGaps(env, finfo, ginfo);
      }
    }
  }

#if DEBUG_SHOW_GAPS
  auto pit = ginfo->procMap.begin();
  ProcInfo * pinfo = pit->second;

  cout << "\nfunc:  0x" << hex << pinfo->entry_vma << dec
       << "  (1/" << num_funcs << ")\n"
       << "link:   " << pinfo->linkName << "\n"
       << "parse:  " << pinfo->func->name() << "\n"
       << "0x" << hex << ginfo->start
       << "--0x" << ginfo->end << dec << "\n";

  if (! ginfo->alt_file) {
    cout << "\ncovered:\n"
	 << covered.toString() << "\n"
	 << "\ngaps:\n"
	 << ginfo->gapSet.toString() << "\n";
  }
  else {
    cout << "\ngaps: alt-file\n";
  }
#endif
}

//----------------------------------------------------------------------

// Returns: list of LoopInfo objects.
//
// If the loop at this node is non-null (internal node), then the list
// contains one element for that loop.  If the loop is null (root),
// then the list contains one element for each subtree.
//
static LoopList *
doLoopTree(WorkEnv & env, FileInfo * finfo, GroupInfo * ginfo,
	   ParseAPI::Function * func, BlockSet & visited, LoopTreeNode * ltnode)
{
  LoopList * myList = new LoopList;

  if (ltnode == NULL) {
    return myList;
  }
  Loop *loop = ltnode->loop;

  // process the children of the loop tree
  vector <LoopTreeNode *> clist = ltnode->children;

  std::sort(clist.begin(), clist.end(), LoopTreeLessThan);

  for (uint i = 0; i < clist.size(); i++) {
    LoopList *subList =
      doLoopTree(env, finfo, ginfo, func, visited, clist[i]);

    for (auto sit = subList->begin(); sit != subList->end(); ++sit) {
      myList->push_back(*sit);
    }
    delete subList;
  }

  // if no loop at this node (root node), then return the list of
  // children.
  if (loop == NULL) {
    return myList;
  }

  // otherwise, finish this loop, put into LoopInfo format, insert the
  // subloops and return a list of one.
  string loopName = ltnode->name();
  FLPSeqn empty;

  TreeNode * myLoop =
    doLoopLate(env, ginfo, func, visited, loop, loopName);

  for (auto it = myList->begin(); it != myList->end(); ++it) {
    mergeInlineLoop(myLoop, empty, *it);
  }

  // reparent the tree and put into LoopInfo format
  LoopInfo * myInfo =
    findLoopHeader(env, finfo, ginfo, func, myLoop, loop, loopName);

  if (merge_irred_loops) {
    mergeIrredLoops(env, myInfo);
  }

  myList->clear();
  myList->push_back(myInfo);

  return myList;
}

//----------------------------------------------------------------------

// Post-order process for one loop, after the subloops.  Add any
// leftover inclusive blocks, select the loop header, reparent as
// needed, remove the top inline spine, and put into the LoopInfo
// format.
//
// Returns: the raw inline tree for this loop.
//
static TreeNode *
doLoopLate(WorkEnv & env, GroupInfo * ginfo, ParseAPI::Function * func,
	   BlockSet & visited, Loop * loop, const string & loopName)
{
  TreeNode * root = new TreeNode;

  DEBUG_CFG("\nbegin loop:  " << loopName << "  '" << func->name() << "'\n");

  // add the inclusive blocks not contained in a subloop
  vector <Block *> bvec;
  loop->getLoopBasicBlocks(bvec);

  std::sort(bvec.begin(), bvec.end(), BlockLessThan);

  for (uint i = 0; i < bvec.size(); i++) {
    if (! visited[bvec[i]]) {
      doBlock(env, ginfo, func, visited, bvec[i], root);
    }
  }

  return root;
}

//----------------------------------------------------------------------

// Process one basic block.
//
static void
doBlock(WorkEnv & env, GroupInfo * ginfo, ParseAPI::Function * func,
	BlockSet & visited, Block * block, TreeNode * root)
{
  if (block == NULL || visited[block]) {
    return;
  }
  visited[block] = true;

  DEBUG_CFG("\nblock:\n");

  // see if this block ends with a call edge
  const Block::edgelist & outEdges = block->targets();
  bool is_call = false;
  bool is_sink = false;
  VMA  target = 0;

  for (auto eit = outEdges.begin(); eit != outEdges.end(); ++eit) {
    Edge * edge = *eit;

    if (edge->type() == ParseAPI::CALL) {
      is_call = true;
      is_sink = edge->sinkEdge();
      target = edge->trg()->start();
      break;
    }
  }

  LineMapCache lmcache (ginfo->sym_func, env.realPath);

  // iterate through the instructions in this block
  map <Offset, Instruction> imap;
  block->getInsns(imap);

  int len = 0;
  string device;

  if (cuda_arch > 0) {
    device = "NVIDIA sm_" + std::to_string(cuda_arch);
  } else if (intel_gpu_arch > 0) {
    device = "INTEL GPU";
  }
  
  for (auto iit = imap.begin(); iit != imap.end(); ++iit) {
    auto next_it = iit;  next_it++;
    Offset vma = iit->first;
    string filenm = "";
    uint line = 0;

    len = iit->second.size();

    lmcache.getLineInfo(vma, filenm, line);

#if DEBUG_CFG_SOURCE
    debugStmt(vma, len, filenm, line, env.realPath);
#endif

    // a call must be the last instruction in the block
    if (next_it == imap.end() && is_call) {
      addStmtToTree(root, *(env.strTab), env.realPath, vma, len, filenm, line,
		    device, is_call, is_sink, target);
    }
    else {
      addStmtToTree(root, *(env.strTab), env.realPath, vma, len, filenm, line, device);
    }
  }

#if DEBUG_CFG_SOURCE
  debugEdges(block);
#endif
}

//---------------------------------------------------------------------- 

// Unparsable functions 
// 
static void 
doUnparsableFunctionList(WorkEnv & env, FileInfo * finfo, GroupInfo * ginfo)
{ 
  // not sure if cuda generates multiple functions, but we'll handle 
  // this case until proven otherwise. 
  long num = 0; 
 
  for (auto pit = ginfo->procMap.begin(); pit != ginfo->procMap.end(); ++pit) { 
    ProcInfo * pinfo = pit->second; 
    ParseAPI::Function * func = pinfo->func; 
    num++; 
 
#if DEBUG_CFG_SOURCE 
    long num_funcs = ginfo->procMap.size(); 
    debugFuncHeader(finfo, pinfo, num, num_funcs, "cuda"); 
#endif 
 
    TreeNode * root = new TreeNode; 
 
    doUnparsableFunction(env, ginfo, func, root); 
    pinfo->root = root; 
  } 
} 

static void 
doUnparsableFunction(WorkEnv & env, GroupInfo * ginfo, ParseAPI::Function * func,
		     TreeNode * root)
{ 
  LineMapCache lmcache (ginfo->sym_func, env.realPath);
 
  int len = 4; 
  for (Offset vma = ginfo->start; vma < ginfo->end; vma += len) { 
    string filenm = ""; 
    uint line = 0; 
 
    lmcache.getLineInfo(vma, filenm, line); 
    string device;
    addStmtToTree(root, *(env.strTab), env.realPath, vma, len, filenm, line, device);
  }
}

//----------------------------------------------------------------------

// Add unclaimed regions (gaps) to the group leader.
//
// This is the basic version -- if line map exists, add a top-level
// stmt to the func for each line map range, else assign to the first
// line of func.  The full version is handled in Struct-Output.cpp.
//
static void
addGaps(WorkEnv & env, FileInfo * finfo, GroupInfo * ginfo)
{
  if (ginfo->procMap.begin() == ginfo->procMap.end()) {
    return;
  }

  ProcInfo * pinfo = ginfo->procMap.begin()->second;

  // if one function is entirely contained within another function,
  // then it's possible that ProcInfo has no statements and a NULL
  // TreeNode.
  if (pinfo->root == NULL) {
    pinfo->root = new TreeNode;
  }
  TreeNode * root = pinfo->root;

  for (auto git = ginfo->gapSet.begin(); git != ginfo->gapSet.end(); ++git) {
    VMA vma = git->beg();
    VMA end_gap = git->end();

    while (vma < end_gap) {
      StatementVector svec;
      getStatement(svec, vma, ginfo->sym_func);

      if (! svec.empty()) {
        string filenm = svec[0]->getFile();
        SrcFile::ln line = svec[0]->getLine();
        VMA end = std::min(((VMA) svec[0]->endAddr()), end_gap);

        string device;
        addStmtToTree(root, *(env.strTab), env.realPath, vma, end - vma,
		      filenm, line, device);
        vma = end;
      }
      else {
        // fixme: could be better at finding end of range
        VMA end = std::min(vma + 4, end_gap);

        string device;
        addStmtToTree(root, *(env.strTab), env.realPath, vma, end - vma,
		      finfo->fileName, pinfo->line_num, device);
        vma = end;
      }
    }
  }
}

//****************************************************************************
// Support functions
//****************************************************************************

// New heuristic for identifying loop header inside inline tree.
// Start at the root, descend the inline tree and try to find where
// the loop begins.  This is the central problem of all hpcstruct:
// where to locate a loop inside an inline sequence.
//
// Note: loop "headers" are no longer tied to a specific VMA and
// machine instruction.  They are strictly file and line number.
// (For some loops, there is no right VMA.)
//
// Returns: detached LoopInfo object.
//
static LoopInfo *
findLoopHeader(WorkEnv & env, FileInfo * finfo, GroupInfo * ginfo,
	       ParseAPI::Function * func, TreeNode * root, Loop * loop,
	       const string & loopName)
{
  HPC::StringTable * strTab = env.strTab;
  long empty_index = strTab->str2index("");

  //------------------------------------------------------------
  // Step 1 -- build the list of loop exit conditions
  //------------------------------------------------------------

  vector <Block *> inclBlocks;
  set <Block *> bset;
  HeaderList clist;

  loop->getLoopBasicBlocks(inclBlocks);
  for (auto bit = inclBlocks.begin(); bit != inclBlocks.end(); ++bit) {
    bset.insert(*bit);
  }

  // a stmt is a loop exit condition if it has outgoing edges to
  // blocks both inside and outside the loop.  but don't include call
  // edges.  (don't need to sort blocks and edges here because the
  // real answer is clist and clist is a map.)
  //
  for (auto bit = inclBlocks.begin(); bit != inclBlocks.end(); ++bit) {
    Block * block = *bit;
    const Block::edgelist & outEdges = block->targets();
    VMA src_vma = block->last();
    bool in_loop = false, out_loop = false;

    for (auto eit = outEdges.begin(); eit != outEdges.end(); ++eit) {
      Block *dest = (*eit)->trg();
      int type = (*eit)->type();

      if (type != ParseAPI::CALL && type != ParseAPI::CALL_FT) {
        if (bset.find(dest) != bset.end()) { in_loop = true; }
        else { out_loop = true; }
      }
    }

    if (in_loop && out_loop) {
      string filenm = "";
      SrcFile::ln line = 0;

      StatementVector svec;
      getStatement(svec, src_vma, ginfo->sym_func);

      if (! svec.empty()) {
        filenm = svec[0]->getFile();
        line = svec[0]->getLine();
	env.realPath->realpath(filenm);
      }

      InlineSeqn seqn;
      analyzeAddr(seqn, src_vma, env.realPath);

      clist[src_vma] = HeaderInfo(block);
      clist[src_vma].is_excl = loop->hasBlockExclusive(block);
      clist[src_vma].depth = seqn.size();
      clist[src_vma].file_index = strTab->str2index(filenm);
      clist[src_vma].base_index = strTab->str2index(FileUtil::basename(filenm));
      clist[src_vma].line_num = line;
    }
  }

  // see if stmt is also a back edge source.  (we sort the back edges
  // only for debug output.)
  vector <Edge *> backEdges;
  loop->getBackEdges(backEdges);

  std::sort(backEdges.begin(), backEdges.end(), EdgeLessThan);

  for (auto eit = backEdges.begin(); eit != backEdges.end(); ++eit) {
    VMA src_vma = (*eit)->src()->last();

    auto it = clist.find(src_vma);
    if (it != clist.end()) {
      it->second.is_src = true;
    }
  }

#if DEBUG_CFG_SOURCE
  cout << "\nraw inline tree:  " << loopName
       << "  '" << func->name() << "'\n"
       << "file:  '" << finfo->fileName << "'\n\n";
  debugInlineTree(root, NULL, *strTab, 0, false);
  debugLoop(ginfo, func, loop, loopName, backEdges, clist, env.realPath);
  cout << "\nsearching inline tree:\n";
#endif

  //------------------------------------------------------------
  // Step 2 -- find the right inline depth
  //------------------------------------------------------------

  // start at the root, descend the inline tree and try to find the
  // right level for the loop.  an inline branch or subloop is an
  // absolute stopping point.  the hard case is one inline subtree
  // plus statements.  we stop if there is a loop condition, else
  // continue and reparent the stmts.  always reparent any stmt in a
  // different file from the inline callsite.

  FLPSeqn  path;
  StmtMap  stmts;
  int  depth_root = 0;

  while (root->nodeMap.size() == 1 && root->loopList.size() == 0) {
    FLPIndex flp = root->nodeMap.begin()->first;

    // look for loop cond at this level
    for (auto cit = clist.begin(); cit != clist.end(); ++cit) {
      VMA vma = cit->first;

      if (root->stmtMap.member(vma)) {
        goto found_level;
      }

      // reparented stmts must also match file name
      StmtInfo * sinfo = stmts.findStmt(vma);
      if (sinfo != NULL && sinfo->base_index == flp.base_index) {
        goto found_level;
      }
    }

    // reparent the stmts and proceed to the next level
    for (auto sit = root->stmtMap.begin(); sit != root->stmtMap.end(); ++sit) {
      stmts.insert(sit->second);
    }
    root->stmtMap.clear();

    TreeNode *subtree = root->nodeMap.begin()->second;
    root->nodeMap.clear();
    delete root;
    root = subtree;
    path.push_back(flp);
    depth_root++;

    DEBUG_CFG("inline:  l=" << flp.line_num
	      << "  f='" << strTab->index2str(flp.file_index)
	      << "'  p='" << debugPrettyName(strTab->index2str(flp.pretty_index))
	      << "'\n");
  }
found_level:

#if DEBUG_CFG_SOURCE
  if (root->nodeMap.size() > 0) {
    cout << "\nremaining inline subtrees:\n";

    for (auto nit = root->nodeMap.begin(); nit != root->nodeMap.end(); ++nit) {
      FLPIndex flp = nit->first;
      cout << "inline:  l=" << flp.line_num
	   << "  f='" << strTab->index2str(flp.file_index)
	   << "'  p='" << debugPrettyName(strTab->index2str(flp.pretty_index))
	   << "'\n";
    }
  }
#endif

  //------------------------------------------------------------
  // Step 3 -- reattach stmts into this level
  //------------------------------------------------------------

  // fixme: want to attach some stmts below this level

  for (auto sit = stmts.begin(); sit != stmts.end(); ++sit) {
    root->stmtMap.insert(sit->second);
  }
  stmts.clear();

  //------------------------------------------------------------
  // Step 4 -- choose a loop header file/line at this level
  //------------------------------------------------------------

  // choose loop header file based on exit conditions, and then min
  // line within that file.  ideally, we want a file that is both an
  // exit cond and matches some other anchor: either the enclosing
  // func (if top-level), or else an inline subtree.

  long proc_file = strTab->str2index(finfo->fileName);
  long proc_base = strTab->str2index(FileUtil::basename(finfo->fileName));
  long file_ans = empty_index;
  long base_ans = empty_index;
  long line_ans = 0;

  // first choice is the file for the enclosing func that matches some
  // exit cond at top-level, but only if no inline steps
  if (depth_root == 0) {
    for (auto cit = clist.begin(); cit != clist.end(); ++cit) {
      HeaderInfo * info = &(cit->second);

      if (info->depth == depth_root && info->base_index != empty_index
	  && info->base_index == proc_base) {
        file_ans = proc_file;
        base_ans = proc_base;
        goto found_file;
      }
    }
  }

  // also good is an inline subtree that matches an exit cond
  for (auto nit = root->nodeMap.begin(); nit != root->nodeMap.end(); ++nit) {
    FLPIndex flp = nit->first;

    for (auto cit = clist.begin(); cit != clist.end(); ++cit) {
      HeaderInfo * info = &(cit->second);

      if (info->depth == depth_root && info->base_index != empty_index
	  && info->base_index == flp.base_index) {
        file_ans = flp.file_index;
        base_ans = flp.base_index;
        goto found_file;
      }
    }
  }

  // settle for any exit cond at root level.  this is the last of the
  // good answers.
  for (auto cit = clist.begin(); cit != clist.end(); ++cit) {
    HeaderInfo * info = &(cit->second);

    if (info->depth == depth_root && info->base_index != empty_index) {
      file_ans = info->file_index;
      base_ans = info->base_index;
      goto found_file;
    }
  }

  // enclosing func file, but without exit cond
  if (depth_root == 0 && proc_file != empty_index) {
    file_ans = proc_file;
    base_ans = proc_base;
    goto found_file;
  }

  // inline subtree, but without exit cond
  for (auto nit = root->nodeMap.begin(); nit != root->nodeMap.end(); ++nit) {
    FLPIndex flp = nit->first;

    if (flp.file_index != empty_index) {
      file_ans = flp.file_index;
      base_ans = flp.base_index;
      goto found_file;
    }
  }

  // subloop at root level
  for (auto lit = root->loopList.begin(); lit != root->loopList.end(); ++lit) {
    LoopInfo * linfo = *lit;

    if (linfo->file_index != empty_index) {
      file_ans = linfo->file_index;
      base_ans = linfo->base_index;
      goto found_file;
    }
  }

  // any stmt at root level
  for (auto sit = root->stmtMap.begin(); sit != root->stmtMap.end(); ++sit) {
    StmtInfo * sinfo = sit->second;

    if (sinfo->file_index != empty_index) {
      file_ans = sinfo->file_index;
      base_ans = sinfo->base_index;
      line_ans = sinfo->line_num;
      break;
    }
  }
found_file:

  // min line of inline callsites
  // fixme: code motion breaks this
#if 0
  for (auto nit = root->nodeMap.begin(); nit != root->nodeMap.end(); ++nit) {
    FLPIndex flp = nit->first;

    if (flp.base_index == base_ans
	&& (line_ans == 0 || (flp.line_num > 0 && flp.line_num < line_ans))) {
      line_ans = flp.line_num;
    }
  }
#endif

  // min line of subloops
  for (auto lit = root->loopList.begin(); lit != root->loopList.end(); ++lit) {
    LoopInfo * linfo = *lit;

    if (linfo->base_index == base_ans
	&& (line_ans == 0 || (linfo->line_num > 0 && linfo->line_num < line_ans))) {
      line_ans = linfo->line_num;
    }
  }

  // min line of exit cond stmts at root level
  for (auto cit = clist.begin(); cit != clist.end(); ++cit) {
    HeaderInfo * info = &(cit->second);

    if (info->depth == depth_root
	&& (line_ans == 0 || (info->line_num > 0 && info->line_num < line_ans))) {
      line_ans = info->line_num;
    }
  }

  DEBUG_CFG("\nheader:  l=" << line_ans << "  f='"
	    << strTab->index2str(file_ans) << "'\n");

  vector <Block *> entryBlocks;
  loop->getLoopEntries(entryBlocks);
  VMA entry_vma = VMA_MAX;

  for (auto bit = entryBlocks.begin(); bit != entryBlocks.end(); ++bit) {
    entry_vma = std::min(entry_vma, (*bit)->start());
  }

  LoopInfo *info = new LoopInfo(root, path, loopName, entry_vma,
				file_ans, base_ans, line_ans,
				entryBlocks.size() > 1);

#if DEBUG_CFG_SOURCE
  cout << "\nreparented inline tree:  " << loopName
       << "  '" << func->name() << "'\n\n";
  debugInlineTree(root, info, *strTab, 0, false);
#endif

  return info;
}

//----------------------------------------------------------------------

// Delete the inline sequence 'prefix' from root's tree and reparent
// any statements or loops.  We expect there to be no statements,
// loops or subtrees along the deleted spine, but if there are, move
// them to the subtree.
//
// Returns: the subtree at the end of prefix.
//
static TreeNode *
deleteInlinePrefix(WorkEnv & env, TreeNode * root, Inline::InlineSeqn prefix)
{
  StmtMap  stmts;
  LoopList loops;

  // walk the prefix and collect any stmts or loops
  for (auto pit = prefix.begin(); pit != prefix.end(); ++pit)
  {
    FLPIndex flp(*(env.strTab), *pit);
    auto nit = root->nodeMap.find(flp);

    if (nit != root->nodeMap.end()) {
      TreeNode * subtree = nit->second;

      // statements
      for (auto sit = root->stmtMap.begin(); sit != root->stmtMap.end(); ++sit) {
	stmts.insert(sit->second);
      }
      root->stmtMap.clear();

      // loops
      for (auto lit = root->loopList.begin(); lit != root->loopList.end(); ++lit) {
	loops.push_back(*lit);
      }
      root->loopList.clear();

      // subtrees
      for (auto it = root->nodeMap.begin(); it != root->nodeMap.end(); ++it) {
	TreeNode * node = it->second;
	if (node != subtree) {
	  mergeInlineEdge(subtree, it->first, node);
	}
      }
      root->nodeMap.clear();
      delete root;

      root = subtree;
    }
  }

  // reattach the stmts and loops
  for (auto sit = stmts.begin(); sit != stmts.end(); ++sit) {
    root->stmtMap.insert(sit->second);
  }
  stmts.clear();

  for (auto lit = loops.begin(); lit != loops.end(); ++lit) {
    root->loopList.push_back(*lit);
  }
  loops.clear();

  return root;
}

//----------------------------------------------------------------------

// Merge irreducible loops.  If L1 is an irreducible loop and L2 is an
// irreducible subloop and L1 and L2 have the same file and line
// attribution, then it's likely there is really only one loop in the
// source code, so merge L2 into L1.
//
// This only applies to irreducible loops.  Natural (reducible) loops
// are assumed to be correct.
//
static void
mergeIrredLoops(WorkEnv & env, LoopInfo * L1)
{
  // if L1 is reducible, then do nothing
  if (L1 == NULL || ! L1->irred) {
    return;
  }

  TreeNode * node1 = L1->node;
  LoopList & list1 = node1->loopList;

  // if there are no irreducible subloops, then do nothing
  bool has_irred = false;
  for (auto it = list1.begin(); it != list1.end(); ++it) {
    LoopInfo * L2 = *it;
    if (L2->irred) { has_irred = true; break; }
  }
  if (! has_irred) {
    return;
  }

  DEBUG_CFG("\n");

  // iterate through L1's subloops and merge L2 into L1 when they
  // match.  merging requires moving L2's subloops onto L1's list, the
  // very list we are iterating through.  to maintain correctness, put
  // the loops on a new list and copy them back.

  vector <LoopInfo *> newList;

  for (auto it1 = list1.begin(); it1 != list1.end(); ++it1) {
    LoopInfo * L2 = *it1;
    TreeNode * node2 = L2->node;
    LoopList & list2 = node2->loopList;

    if (L2->irred && L1->base_index == L2->base_index && L1->line_num == L2->line_num)
    {
      // merge L2 into L1 and put L2's subloops on the new list
      DEBUG_CFG("merge: " << L2->name << " into " << L1->name << "\n");

      for (auto it2 = list2.begin(); it2 != list2.end(); ++it2) {
	newList.push_back(*it2);
      }
      list2.clear();
      mergeInlineTree(node1, node2);
      delete L2;
    }
    else {
      // do not merge, keep entire L2 intact on new list
      newList.push_back(L2);
    }
  }

  std::sort(newList.begin(), newList.end(), LoopInfoLessThan);

  // copy new list back to L1
  list1.clear();
  for (auto nit = newList.begin(); nit != newList.end(); ++nit) {
    list1.push_back(*nit);
  }
  newList.clear();

#if DEBUG_CFG_SOURCE
  cout << "\n";
  debugInlineTree(node1, L1, *(env.strTab), 0, false);
#endif
}

//----------------------------------------------------------------------

// Compute gaps = the set of intervals in [start, end) that are not
// covered in vset.
//
static void
computeGaps(VMAIntervalSet & vset, VMAIntervalSet & gaps, VMA start, VMA end)
{
  gaps.clear();

  auto it = vset.begin();

  while (start < end)
  {
    if (it == vset.end()) {
      gaps.insert(start, end);
      break;
    }
    else if (it->end() <= start) {
      // it entirely left of start
      ++it;
    }
    else if (it->beg() <= start) {
      // it contains start
      start = it->end();
      ++it;
    }
    else {
      // it entirely right of start
      VMA gap_end = std::min(it->beg(), end);
      gaps.insert(start, gap_end);
      start = it->end();
      ++it;
    }
  }
}

//****************************************************************************
// Debug functions
//****************************************************************************

#if DEBUG_ANY_ON

// Debug functions to display the raw input data from ParseAPI for
// loops, blocks, stmts, file names, proc names and line numbers.

#define INDENT   "   "

// Cleanup and shorten the proc name: demangle plus collapse nested
// <...> and (...) to just <> and ().  For example:
//
//   std::map<int,long>::myfunc(int) --> std::map<>::myfunc()
//
// This is only for more compact debug output.  Internal decisions are
// always made on the full string.
//
static string
debugPrettyName(const string & procnm)
{
  string str = procnm;
  string ans = "";
  size_t str_len = str.size();
  size_t pos = 0;

  while (pos < str_len) {
    size_t next = str.find_first_of("<(", pos);
    char open, close;

    if (next == string::npos) {
      ans += str.substr(pos);
      break;
    }
    if (str[next] == '<') { open = '<';  close = '>'; }
    else { open = '(';  close = ')'; }

    ans += str.substr(pos, next - pos) + open + close;

    int depth = 1;
    for (pos = next + 1; pos < str_len && depth > 0; pos++) {
      if (str[pos] == open) { depth++; }
      else if (str[pos] == close) { depth--; }
    }
  }

  return ans;
}

//----------------------------------------------------------------------

static void
debugElfHeader(ElfFile * elfFile)
{
  size_t len = elfFile->getLength();

  cout << "\n============================================================\n"
       << "Elf File:  " << elfFile->getFileName() << "\n"
       << "length:    0x" << hex << len << dec << "  (" << len << ")\n"
       << "============================================================\n";
}

//----------------------------------------------------------------------

static void
debugFuncHeader(FileInfo * finfo, ProcInfo * pinfo, long num, long num_funcs,
		string label)
{
  ParseAPI::Function * func = pinfo->func;
  Address entry_addr = func->addr();

  cout << "\n------------------------------------------------------------\n";

  if (label != "") { cout << label << " "; }

  cout << "func:  0x" << hex << entry_addr << dec
       << "  (" << num << "/" << num_funcs << ")"
       << "  link='" << pinfo->linkName << "'\n"
       << "parse:  '" << func->name() << "'\n"
       << "file:   '" << finfo->fileName << "'\n";
}

//----------------------------------------------------------------------

static void
debugStmt(VMA vma, int len, string & filenm, SrcFile::ln line,
	  RealPathMgr * realPath)

{
  cout << "stmt:  0x" << hex << vma << dec << " (" << len << ")"
       << "  l=" << line << "  f='" << filenm << "'\n";

  Inline::InlineSeqn nodeList;
  Inline::analyzeAddr(nodeList, vma, realPath);

  // list is outermost to innermost
  for (auto nit = nodeList.begin(); nit != nodeList.end(); ++nit) {
    cout << INDENT << "inline:  l=" << nit->getLineNum()
	 << "  f='" << nit->getFileName()
	 << "'  p='" << debugPrettyName(nit->getPrettyName()) << "'\n";
  }
}

//----------------------------------------------------------------------

static string
edgeType(int type)
{
  if (type == ParseAPI::CALL)           { return "call"; }
  if (type == ParseAPI::COND_TAKEN)     { return "cond-take"; }
  if (type == ParseAPI::COND_NOT_TAKEN) { return "cond-not"; }
  if (type == ParseAPI::INDIRECT)       { return "indirect"; }
  if (type == ParseAPI::DIRECT)         { return "direct"; }
  if (type == ParseAPI::FALLTHROUGH)    { return "fallthr"; }
  if (type == ParseAPI::CATCH)          { return "catch"; }
  if (type == ParseAPI::CALL_FT)        { return "call-ft"; }
  if (type == ParseAPI::RET)            { return "return"; }
  return "unknown";
}

static void
debugEdges(Block * block)
{
  const Block::edgelist & outEdges = block->targets();
  vector <Edge *> edgeVec;

  for (auto eit = outEdges.begin(); eit != outEdges.end(); ++eit) {
    edgeVec.push_back(*eit);
  }
  std::sort(edgeVec.begin(), edgeVec.end(), EdgeLessThan);

  cout << "out edges:" << hex;

  for (auto eit = edgeVec.begin(); eit != edgeVec.end(); ++eit) {
    Edge * edge = *eit;

    cout << "  0x" << edge->trg()->start()
	 << " (" << edgeType(edge->type());

    if (edge->interproc()) {
      cout << ", interproc";
    }
    cout << ")";
  }

  cout << dec << "\n";
}

//----------------------------------------------------------------------

static void
debugAddr(GroupInfo * ginfo, VMA vma, RealPathMgr * realPath)
{
  StatementVector svec;
  string filenm = "";
  SrcFile::ln line = 0;

  getStatement(svec, vma, ginfo->sym_func);

  if (! svec.empty()) {
    filenm = svec[0]->getFile();
    line = svec[0]->getLine();
    realPath->realpath(filenm);
  }

  cout << "0x" << hex << vma << dec
       << "  l=" << line << "  f='" << filenm << "'\n";
}

//----------------------------------------------------------------------

static void
debugLoop(GroupInfo * ginfo, ParseAPI::Function * func,
	  Loop * loop, const string & loopName,
	  vector <Edge *> & backEdges, HeaderList & clist,
	  RealPathMgr * realPath)
{
  vector <Block *> entBlocks;
  int num_ents = loop->getLoopEntries(entBlocks);

  std::sort(entBlocks.begin(), entBlocks.end(), BlockLessThan);

  cout << "\nheader info:  " << loopName
       << ((num_ents == 1) ? "  (reducible)" : "  (irreducible)")
       << "  '" << func->name() << "'\n";

  cout << "\nfunc header:\n";
  debugAddr(ginfo, func->addr(), realPath);

  cout << "\nentry blocks:" << hex;
  for (auto bit = entBlocks.begin(); bit != entBlocks.end(); ++bit) {
    cout << "  0x" << (*bit)->start();
  }

  cout << "\nback edge sources:";
  for (auto eit = backEdges.begin(); eit != backEdges.end(); ++eit) {
    cout << "  0x" << (*eit)->src()->last();
  }

  cout << "\nback edge targets:";
  for (auto eit = backEdges.begin(); eit != backEdges.end(); ++eit) {
    cout << "  0x" << (*eit)->trg()->start();
  }
  cout << dec;

  cout << "\n\nexit conditions:\n";
  for (auto cit = clist.begin(); cit != clist.end(); ++cit) {
    VMA vma = cit->first;
    HeaderInfo * info = &(cit->second);
    SrcFile::ln line = 0;
    string filenm = "";

    InlineSeqn seqn;
    analyzeAddr(seqn, vma, realPath);

    StatementVector svec;
    getStatement(svec, vma, ginfo->sym_func);

    if (! svec.empty()) {
      filenm = svec[0]->getFile();
      line = svec[0]->getLine();
      realPath->realpath(filenm);
    }

    cout << "0x" << hex << vma << dec
	 << "  " << (info->is_src ? "src " : "cond")
	 << "  excl: " << info->is_excl
	 << "  depth: " << info->depth
	 << "  l=" << line
	 << "  f='" << filenm << "'\n";
  }
}

//----------------------------------------------------------------------

// If LoopInfo is non-null, then treat 'node' as a detached loop and
// prepend the FLP seqn from 'info' above the tree.  Else, 'node' is
// the tree.
//
static void
debugInlineTree(TreeNode * node, LoopInfo * info, HPC::StringTable & strTab,
		int depth, bool expand_loops)
{
  // treat node as a detached loop with FLP seqn above it.
  if (info != NULL) {
    depth = 0;
    for (auto pit = info->path.begin(); pit != info->path.end(); ++pit) {
      for (int i = 1; i <= depth; i++) {
        cout << INDENT;
      }
      FLPIndex flp = *pit;

      cout << "inline:  l=" << flp.line_num
	   << "  f='" << strTab.index2str(flp.file_index)
	   << "'  p='" << debugPrettyName(strTab.index2str(flp.pretty_index))
	   << "'\n";
      depth++;
    }

    for (int i = 1; i <= depth; i++) {
      cout << INDENT;
    }
    cout << "loop:  " << info->name
	 << (info->irred ? "  (irred)" : "")
	 << "  l=" << info->line_num
	 << "  f='" << strTab.index2str(info->file_index) << "'\n";
    depth++;
  }

  // print the terminal statements
  for (auto sit = node->stmtMap.begin(); sit != node->stmtMap.end(); ++sit) {
    StmtInfo *sinfo = sit->second;

    for (int i = 1; i <= depth; i++) {
      cout << INDENT;
    }
    cout << "stmt:  0x" << hex << sinfo->vma << dec
	 << " (" << sinfo->len << (sinfo->is_call ? "/c" : "") << ")"
	 << "  l=" << sinfo->line_num
	 << "  f='" << strTab.index2str(sinfo->file_index) << "'\n";
  }

  // recur on the subtrees
  for (auto nit = node->nodeMap.begin(); nit != node->nodeMap.end(); ++nit) {
    FLPIndex flp = nit->first;

    for (int i = 1; i <= depth; i++) {
      cout << INDENT;
    }
    cout << "inline:  l=" << flp.line_num
	 << "  f='"  << strTab.index2str(flp.file_index)
	 << "'  p='" << debugPrettyName(strTab.index2str(flp.pretty_index))
	 << "'\n";

    debugInlineTree(nit->second, NULL, strTab, depth + 1, expand_loops);
  }

  // recur on the loops
  for (auto lit = node->loopList.begin(); lit != node->loopList.end(); ++lit) {
    LoopInfo *info = *lit;

    for (int i = 1; i <= depth; i++) {
      cout << INDENT;
    }

    cout << "loop:  " << info->name
	 << (info->irred ? "  (irred)" : "")
	 << "  l=" << info->line_num
	 << "  f='" << strTab.index2str(info->file_index) << "'\n";

    if (expand_loops) {
      debugInlineTree(info->node, NULL, strTab, depth + 1, expand_loops);
    }
  }
}

#endif  // DEBUG_CFG_SOURCE

}  // namespace Struct
}  // namespace BAnal
