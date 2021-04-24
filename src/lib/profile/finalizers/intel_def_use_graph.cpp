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
// Copyright ((c)) 2019-2020, Rice University
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

#include "intel_def_use_graph.hpp"

#include <lib/banal/gpu/DotCFG.hpp>                            // GPUParse
#include <CodeObject.h>
#include <Symtab.h>
#include <slicing.h>          // Slicer
#include <Graph.h>            // printDOT
#include <CFGFactory.h>
#include <lib/binutils/ElfHelper.hpp>               // ElfFile
#include <lib/binutils/InputFile.hpp>               // InputFile
#include <lib/banal/Struct-Inline.hpp>              // Inline
#include <lib/banal/gpu/GPUCFGFactory.hpp>          // GPUCFGFactory
#include <lib/banal/gpu/GPUCodeSource.hpp>          // GPUCodeSource
#include <lib/banal/gpu/ReadIntelCFG.hpp>           // parseIntelCFG, addCustomFunctionObject
#include <lib/banal/gpu/GPUBlock.hpp>

#include "lib/support-lean/demangle.h"
#include "pipeline.hpp"

#include <elfutils/libdw.h>
#include <elfutils/libdwelf.h>
#include <dwarf.h>
#include <libelf.h>

#include <stdexcept>
#include <unordered_map>
#include <limits>
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sstream>
#include <iomanip>

//*****************************************************************************
// macros
//*****************************************************************************

//#define LINE_WIDTH 130
//#define MAX_STR_SIZE 1024
#define INSTRUCTION_ANALYZER_DEBUG 0
//#define FAST_SLICING 0



//*****************************************************************************
// local definitions
//*****************************************************************************
static int TRACK_LIMIT = 8;



using namespace hpctoolkit;
using namespace finalizers;

IntelDefUseGraphClassification::IntelDefUseGraphClassification() {
  elf_version(EV_CURRENT);  // We always assume the current ELF version.
}

#if 0
// Search for an alternative debug file to load. Roughly copied from
// dwarf_getalt, which is a rough copy of GDB's handling.
static stdshim::filesystem::path altfile(const stdshim::filesystem::path& path, Elf* elf) {
  stdshim::filesystemx::error_code ec;
  path.lexically_normal();

  // Attempt 1: build-id
  const void* build_id;
  ssize_t build_id_len = dwelf_elf_gnu_build_id(elf, &build_id);
  if(build_id_len >= 2) {  // We need at least 2 bytes to make the filename
    const auto* bytes = (const std::uint8_t*)build_id;
    std::ostringstream ss;

    stdshim::filesystem::path p = "/usr/lib/debug/.build-id";
    ss << std::hex << std::setw(2) << std::setfill('0') << (int)bytes[0];
    p /= ss.str();
    ss.str("");
    for(ssize_t i = 1; i < build_id_len; i++)
      ss << std::hex << std::setw(2) << std::setfill('0') << (int)bytes[i];
    ss << ".debug";
    p /= std::move(ss).str();

    if(stdshim::filesystem::exists(p, ec))
      return p;
  }

  // Attempt 2: gnu_debuglink
  GElf_Word crc;
  const char* debug_link = dwelf_elf_gnu_debuglink(elf, &crc);
  if(debug_link != nullptr) {
    stdshim::filesystem::path dpath = std::string(debug_link);
    if(dpath.is_absolute()) {
      if(stdshim::filesystem::exists(dpath, ec)) return dpath;
    } else {
      auto parent = path.lexically_normal().parent_path();
      stdshim::filesystem::path tpath = parent / dpath;
      if(stdshim::filesystem::exists(tpath, ec)) return tpath;
      tpath = parent / ".debug" / dpath;
      if(stdshim::filesystem::exists(tpath, ec)) return tpath;
      tpath = stdshim::filesystem::path("/usr/lib/debug")
        / parent.root_name() / parent.relative_path() / dpath;
      if(stdshim::filesystem::exists(tpath, ec)) return tpath;
    }
  }

  // Failed, just return
  return "";
}
#endif

#ifdef ELF_C_READ_MMAP
#define HPC_ELF_C_READ ELF_C_READ_MMAP
#else
#define HPC_ELF_C_READ ELF_C_READ
#endif

class FirstMatchPred : public Dyninst::Slicer::Predicates {
 public:
  virtual bool endAtPoint(Dyninst::Assignment::Ptr ap) { return true; }
};


class IgnoreRegPred : public Dyninst::Slicer::Predicates {
 public:
  IgnoreRegPred(std::vector<Dyninst::AbsRegion> &rhs) : _rhs(rhs) {}

  virtual bool modifyCurrentFrame(Dyninst::Slicer::SliceFrame &slice_frame,
                                  Dyninst::GraphPtr graph_ptr, Dyninst::Slicer *slicer) {
    std::vector<Dyninst::AbsRegion> delete_abs_regions;

    for (auto &active_iter : slice_frame.active) {
      // Filter unmatched regs
      auto &abs_region = active_iter.first;
      bool find = false;
      for (auto &rhs_abs_region : _rhs) {
        if (abs_region.absloc().reg() == rhs_abs_region.absloc().reg()) {
          find = true;
          break;
        }
      }
      if (find == false) {
        delete_abs_regions.push_back(abs_region);
      }
    }

    for (auto &abs_region : delete_abs_regions) {
      slice_frame.active.erase(abs_region);
    }

    return true;
  }

 private:
  std::vector<Dyninst::AbsRegion> _rhs;
};


static void
trackDependency
(
 const std::map<int, GPUParse::InstructionStat *> &inst_stat_map,
 Dyninst::Address inst_addr,
 Dyninst::Address func_addr,
 std::map<int, int> &predicate_map,
 Dyninst::NodeIterator exit_node_iter,
 GPUParse::InstructionStat *inst_stat,
 int barriers,
 int step
)
{
  if (step >= TRACK_LIMIT) {
    return;
  }
  Dyninst::NodeIterator in_begin, in_end;
  (*exit_node_iter)->ins(in_begin, in_end);
  for (; in_begin != in_end; ++in_begin) {
    auto slice_node = boost::dynamic_pointer_cast<Dyninst::SliceNode>(*in_begin);
    auto addr = slice_node->addr();
    auto *slice_inst = inst_stat_map.at(addr);

    if (INSTRUCTION_ANALYZER_DEBUG) {
      std::cout << "find inst_addr " << inst_addr - func_addr << " <- addr: " << addr - func_addr;
    }

    Dyninst::Assignment::Ptr aptr = slice_node->assign();
    auto reg = aptr->out().absloc().reg();
    auto reg_id = reg.val() & 0xFF;

    for (size_t i = 0; i < inst_stat->srcs.size(); ++i) {
      if (reg_id == inst_stat->srcs[i]) {
        auto beg = inst_stat->assign_pcs[reg_id].begin();
        auto end = inst_stat->assign_pcs[reg_id].end();
        if (std::find(beg, end, addr - func_addr) == end) {
          inst_stat->assign_pcs[reg_id].push_back(addr - func_addr);
        }
        break;
      }
    }

    if (INSTRUCTION_ANALYZER_DEBUG) {
      std::cout << " reg " << reg_id << std::endl;
    }

    if (slice_inst->predicate_flag == GPUParse::InstructionStat::PREDICATE_NONE && barriers == -1) {
      // 1. No predicate, stop immediately
    } else if (inst_stat->predicate == slice_inst->predicate &&
        inst_stat->predicate_flag == slice_inst->predicate_flag && barriers == -1) {
      // 2. Find an exact match, stop immediately
    } else {
      if (((slice_inst->predicate_flag == GPUParse::InstructionStat::PREDICATE_TRUE &&
              predicate_map[-(slice_inst->predicate + 1)] > 0) ||
            (slice_inst->predicate_flag == GPUParse::InstructionStat::PREDICATE_FALSE &&
             predicate_map[(slice_inst->predicate + 1)] > 0)) && barriers == -1) {
        // 3. Stop if find both !@PI and @PI=
        // add one to avoid P0
      } else {
        // 4. Continue search
        if (slice_inst->predicate_flag == GPUParse::InstructionStat::PREDICATE_TRUE) {
          predicate_map[slice_inst->predicate + 1]++;
        } else {
          predicate_map[-(slice_inst->predicate + 1)]++;
        }

        trackDependency(inst_stat_map, inst_addr, func_addr, predicate_map, in_begin, inst_stat,
            barriers, step + 1);

        // Clear
        if (slice_inst->predicate_flag == GPUParse::InstructionStat::PREDICATE_TRUE) {
          predicate_map[slice_inst->predicate + 1]--;
        } else {
          predicate_map[-(slice_inst->predicate + 1)]--;
        }
      }
    }
  }
}


static void
sliceIntelInstructions
(
 const Dyninst::ParseAPI::CodeObject::funclist &func_set,
 std::vector<GPUParse::Function *> &functions,
 std::string function_name
)
{
  // Build a instruction map
  std::map<int, GPUParse::InstructionStat *> inst_stat_map;
  std::map<int, GPUParse::Block*> inst_block_map;
  for (auto *function : functions) {
    for (auto *block : function->blocks) {
      for (auto *inst : block->insts) {
        if (inst->inst_stat) {
          auto *inst_stat = inst->inst_stat;
          inst_stat_map[inst->offset] = inst_stat;
          inst_block_map[inst->offset] = block;
        }    
      }    
    }    
  }
  std::vector<std::pair<Dyninst::ParseAPI::GPUBlock *, Dyninst::ParseAPI::Function *>> block_vec;
  for (auto dyn_func : func_set) {
    for (auto *dyn_block : dyn_func->blocks()) {
      block_vec.emplace_back(static_cast<Dyninst::ParseAPI::GPUBlock*>(dyn_block), dyn_func);
    }
  }

  // Prepare pass: create instruction cache for slicing
  Dyninst::AssignmentConverter ac(true, false);
  Dyninst::Slicer::InsnCache dyn_inst_cache;
  int threads = 16; // what is the best value to set, and the right way to set it

#pragma omp parallel for schedule(dynamic) firstprivate(ac, dyn_inst_cache) num_threads(threads)
  for (size_t i = 0; i < block_vec.size(); ++i) {
    ParseAPI::GPUBlock *dyn_block = block_vec[i].first;
    auto *dyn_func = block_vec[i].second;
    auto func_addr = dyn_func->addr();

    Dyninst::ParseAPI::Block::Insns insns;
    dyn_block->enable_latency_blame();
    dyn_block->getInsns(insns);

    for (auto &inst_iter : insns) {
      auto &inst = inst_iter.second;
      auto inst_addr = inst_iter.first;
      auto *inst_stat = inst_stat_map.at(inst_addr);

      if (INSTRUCTION_ANALYZER_DEBUG) {
        std::cout << "try to find inst_addr " << inst_addr - func_addr << std::endl;
      }

      std::vector<Dyninst::Assignment::Ptr> assignments;
      ac.convert(inst, inst_addr, dyn_func, dyn_block, assignments);

      for (auto a : assignments) {
#ifdef FAST_SLICING
        FirstMatchPred p;
#else
        IgnoreRegPred p(a->inputs());
#endif
        Dyninst::Slicer s(a, dyn_block, dyn_func, &ac, &dyn_inst_cache);
        Dyninst::GraphPtr g = s.backwardSlice(p);
        //bool status = g->printDOT(function_name + ".dot");
        Dyninst::NodeIterator exit_begin, exit_end;
        g->exitNodes(exit_begin, exit_end);

        for (; exit_begin != exit_end; ++exit_begin) {
          std::map<int, int> predicate_map;
          // DFS to iterate the whole dependency graph
          if (inst_stat->predicate_flag == GPUParse::InstructionStat::PredicateFlag::PREDICATE_TRUE) {
            predicate_map[inst_stat->predicate + 1]++;
          } else if (inst_stat->predicate_flag == GPUParse::InstructionStat::PredicateFlag::PREDICATE_TRUE) {
            predicate_map[-(inst_stat->predicate + 1)]++;
          }
#ifdef FAST_SLICING
          TRACK_LIMIT = 1;
#endif
          auto barrier_threshold = inst_stat->barrier_threshold;
          trackDependency(inst_stat_map, inst_addr, func_addr, predicate_map, exit_begin, inst_stat,
                          barrier_threshold, 0);
        }
      }
    }
  }
}


void IntelDefUseGraphClassification::createDefUseEdges
(
 std::vector<GPUParse::Function *> functions,
 Classification &c
)
{
  for (auto *function : functions) {
    for (auto *block : function->blocks) {
      for (auto *_inst : block->insts) {
        auto *inst = _inst->inst_stat;
        int to = inst->pc;
        for (auto reg_vector: inst->assign_pcs) {
          for (int from: reg_vector.second) {
            uint32_t path_length = 0;
            if (c._def_use_graph.find(from) == c._def_use_graph.end()) {
              path_length = 1;
            } else {
              std::map<uint64_t, uint32_t> &from_incoming_edges = c._def_use_graph[from];
              for (auto edge: from_incoming_edges) {
                if (edge.second > path_length) {
                  path_length = edge.second;
                }
              }
            }
            c._def_use_graph[to][from] = path_length + 1;
          }
        }
      }
    }
  }
  std::cout << "def-use graph begin" << std::endl;
  for (auto iter: c._def_use_graph) {
    int to = iter.first;
    for (auto from: iter.second) {
      std::cout << from.first << " -> " << to << std::endl;
    }
  }
  std::cout << "def-use graph end" << std::endl;
}


void IntelDefUseGraphClassification::createBackwardSlicingInput
(
 ElfFile *elfFile,
 char *text_section,
 size_t text_section_size,
 Classification &c
)
{
  Dyninst::SymtabAPI::Symtab *symtab = Inline::openSymtab(elfFile);
  if (symtab == NULL) {
    return;
  }
  enable_latency_blame();
  auto function_name = elfFile->getGPUKernelName();
  addCustomFunctionObject(function_name, symtab); //adds a dummy function object
  GPUParse::Function function(0, function_name);
  parseIntelCFG(text_section, text_section_size, function);
  std::vector<GPUParse::Function *> functions = {&function};

  Dyninst::ParseAPI::CFGFactory *cfg_fact = new Dyninst::ParseAPI::GPUCFGFactory(functions);
  Dyninst::ParseAPI::CodeSource *code_src = new Dyninst::ParseAPI::GPUCodeSource(functions, symtab); 
  Dyninst::ParseAPI::CodeObject *code_obj = new Dyninst::ParseAPI::CodeObject(code_src, cfg_fact);
  code_obj->parse();
  
  sliceIntelInstructions(code_obj->funcs(), functions, function_name);
  createDefUseEdges(functions, c);
}


void IntelDefUseGraphClassification::module(const Module& m, Classification& c) noexcept {
  const auto& rpath = m.userdata[sink.resolvedPath()];
  const auto& mpath = rpath.empty() ? m.path() : rpath;
  std::string mpath_str = mpath.string();
  if (strstr(mpath_str.c_str(), "gpubins")) {
#if 0
    int fd = -1;
    fd = open(mpath.c_str(), O_RDONLY);
    if(fd == -1) return;  // Can't do anything if we can't open it.

    struct stat sbuf;
    fstat(fd, &sbuf);
    if(c.filled({0, (uint64_t)sbuf.st_size-1})) {  // No holes left to fill
      close(fd);
      return;
    }

    Elf* elf = elf_begin(fd, HPC_ELF_C_READ, nullptr);
    if(elf == nullptr) {
      close(fd);
      return;  // We only work with ELF files.
    }
    symtab(elf, m, c);

    elf_end(elf);
    close(fd);
#endif

    const char *delimiter1 = "n=\"";
    const char *delimiter2 = ".gpubin";
    std::cout << mpath_str << std::endl;
    //size_t delim1_loc = mpath_str.find(delimiter1);
    size_t delim2_loc = mpath_str.find(delimiter2);
    std::string filePath = mpath_str.substr(0, delim2_loc + 7);
    std::cout << "filePath: " << filePath << std::endl;

    InputFile inputFile;

    // failure throws an error up the call chain
    inputFile.openFile(filePath, InputFileError_Error);
    ElfFileVector * elfFileVector = inputFile.fileVector();
    if (elfFileVector == NULL || elfFileVector->empty()) {
      printf("elfFileVector empty\n");
      return;
    }
    ElfFile *elfFile = (*elfFileVector)[0];

    char *text_section = NULL;
    auto text_section_size = elfFile->getTextSection(&text_section);
    //std::map<int, std::pair<int, int>> kernel_latency_frequency_map = getLatencyFrequencyDataForKernel(filePath);
    //processFrequencyValues(kernel_latency_frequency_map);
    createBackwardSlicingInput(elfFile, text_section, text_section_size, c);
  }
}

#if 0
void IntelDefUseGraphClassification::symtab(void* elf_vp, const Module& m, Classification& c) {
  Elf* elf = (Elf*)elf_vp;

  if(elf_kind(elf) != ELF_K_ELF) return;
  // TODO: Eventually accept 32-bit files too.
  if(elf_getident(elf, nullptr)[EI_CLASS] != ELFCLASS64) {
    util::log::warning()
      << "Skipping symbol table extraction of non-64-bit ELF file "
      << m.path().string() << ", excessive unknown procedures may ensue.\n";
    return;
  }

  // Try our best to find the symbol table.
  Elf_Scn* s_symtab = nullptr;
  Elf_Scn* s_dynsym = nullptr;
  for(Elf_Scn* s = elf_nextscn(elf, nullptr); s != nullptr; s = elf_nextscn(elf, s)) {
    Elf64_Shdr* hdr = elf64_getshdr(s);
    if(hdr->sh_type == SHT_SYMTAB) { s_symtab = s; break; }
    else if(hdr->sh_type == SHT_DYNSYM) { s_dynsym = s; }
  }
  if(!s_symtab) s_symtab = s_dynsym;
  if(!s_symtab) return;
  Elf64_Shdr* sh_symtab = elf64_getshdr(s_symtab);

  // Get handles for the contained data, so we can start working
  Elf_Data* symtab = elf_getdata(s_symtab, nullptr);
  if(symtab == nullptr) return;  // Internal error

  // Walk though the symbols
  for(std::size_t off = 0; off < sh_symtab->sh_size; off += sizeof(Elf64_Sym)) {
    Elf64_Sym* sym = (Elf64_Sym*)&((const char*)symtab->d_buf)[off];
    if(sym->st_size == 0) continue;  // Just skip over weird symbols
    Classification::Interval range(sym->st_value, sym->st_value + sym->st_size - 1);  // Maybe?
    if(c.filled(range)) continue;  // Skip over previously filled symbols

    // Create a Function to stuff all our data in.
    auto& func = c.addFunction(m);
    func.offset = range.lo;

    // Handle the name, the usual way
    const char* name = elf_strptr(elf, sh_symtab->sh_link, sym->st_name);
    if(name != nullptr) {
      char* dn = hpctoolkit_demangle(name);
      if(dn) {
        func.name = dn;
        std::free(dn);
      } else func.name = name;
    }

    // Add our Classification as usual.
    c.setScope(range, c.addScope(func));
  }
}

// Helper recursive thing
template<class T>
void dwarfwalk(Dwarf_Die die, const std::function<T(Dwarf_Die&, T)>& pre,
               const std::function<void(Dwarf_Die&, T, T)>& post, const T& t) {
  do {
    Dwarf_Die child;
    T subt = pre(die, t);

    bool recurse = false;
    switch(dwarf_tag(&die)) {
    case DW_TAG_compile_unit:
    case DW_TAG_module:
    case DW_TAG_lexical_block:
    case DW_TAG_with_stmt:
    case DW_TAG_catch_block:
    case DW_TAG_try_block:
    case DW_TAG_entry_point:
    case DW_TAG_inlined_subroutine:
    case DW_TAG_subprogram:
    case DW_TAG_namespace:
    case DW_TAG_class_type:
    case DW_TAG_structure_type:
      recurse = true;
      break;
    }

    if(recurse && dwarf_child(&die, &child) == 0)
      dwarfwalk(child, pre, post, subt);
    post(die, subt, t);
  } while(dwarf_siblingof(&die, &die) == 0);
}

void IntelDefUseGraphClassification::fullDwarf(void* dbg_vp, const Module& m, Classification& c) try {
  std::unordered_map<Dwarf_Off, std::reference_wrapper<Function>> funcs;
  std::vector<Classification::LineScope> lscopes;

  Dwarf* dbg = (Dwarf*)dbg_vp;

  // Cache Files so that we don't hammer the maps too much
  const File* unknownFile = nullptr;
  std::unordered_map<Dwarf_Files*, std::vector<const File*>> fileCache;
  auto getFile = [&](Dwarf_Files* files, Dwarf_Die* cudie, size_t idx, bool allowUnknown) -> const File* {
    if(files != nullptr && idx > 0) {
      auto& fvec = fileCache.emplace(files, std::vector<const File*>{}).first->second;
      if(idx < fvec.size() && fvec[idx] != nullptr) return fvec[idx];
      if(idx >= fvec.size()) fvec.resize(idx+1, nullptr);

      const char* path_cstr = dwarf_filesrc(files, idx, nullptr, nullptr);
      if(path_cstr != nullptr) {
        stdshim::filesystem::path path = std::string(path_cstr);
        if(!path.is_absolute()) {
          Dwarf_Attribute attr_mem;
          const char* comp_dir_cstr = dwarf_formstring(
            dwarf_attr(cudie, DW_AT_comp_dir, &attr_mem));
          if(comp_dir_cstr != nullptr)
            path = stdshim::filesystem::path(std::string(comp_dir_cstr)) / path;
        }
        fvec[idx] = &sink.file(std::move(path));
        return fvec[idx];
      }
    }
    if(allowUnknown) {
      if(unknownFile == nullptr) unknownFile = &sink.file("???");
      return unknownFile;
    }
    return nullptr;
  };
  auto getFileDie = [&](Dwarf_Die* cudie, size_t idx, bool allowUnknown) -> const File* {
    Dwarf_Files* files;
    size_t nfiles;
    if(dwarf_getsrcfiles(cudie, &files, &nfiles) == 0 && idx < nfiles)
      return getFile(files, cudie, idx, allowUnknown);
    return getFile(nullptr, cudie, 0, allowUnknown);
  };

  // Process the bits one CU at a time
  Dwarf_CU* cu = nullptr;
  Dwarf_Die root;
  while(dwarf_get_units(dbg, cu, &cu, nullptr, nullptr, &root, nullptr) == 0) {
    dwarfwalk<Classification::Block*>(root,
      [&](Dwarf_Die& die, Classification::Block* par) -> Classification::Block* {
        // Check that the DIE is a function-like thing. Skip if not.
        int tag = dwarf_tag(&die);
        if(tag != DW_TAG_subprogram && tag != DW_TAG_inlined_subroutine)
          return par;

        // Skip declarations. They'll be added when we find them.
        if(dwarf_hasattr(&die, DW_AT_declaration)) return par;

        // Skip any abstract instances. We want only the concrete ones.
        if(dwarf_func_inline(&die)) return par;

        Dwarf_Attribute attr_mem;
        Dwarf_Attribute* attr;

        // If this is an inlining, we work from that Function.
        Dwarf_Off offsetid = dwarf_dieoffset(&die);
        if((attr = dwarf_attr(&die, DW_AT_abstract_origin, &attr_mem)) != nullptr) {
          Dwarf_Die fdie_mem;
          Dwarf_Die* fdie = dwarf_formref_die(attr, &fdie_mem);
          if(fdie != nullptr) offsetid = dwarf_dieoffset(fdie);
        }
        auto funcit = funcs.find(offsetid);
        if(funcit == funcs.end())
          funcit = funcs.emplace(offsetid, c.addFunction(m)).first;
        Function& func = funcit->second.get();

        // Try to fill in any missing data in the function
        if(func.name.empty()) {
          attr = dwarf_attr_integrate(&die, DW_AT_linkage_name, &attr_mem);
          if(attr == nullptr)
            attr = dwarf_attr_integrate(&die, DW_AT_name, &attr_mem);
          if(attr != nullptr) {
            const char* name = dwarf_formstring(attr);
            char* dn = hpctoolkit_demangle(name);
            func.name = dn == nullptr ? name : dn;
            if(dn != nullptr) std::free(dn);
          }
        }
        if(func.offset == 0) {
          Dwarf_Addr offset;
          if(dwarf_entrypc(&die, &offset) == 0) func.offset = offset;
        }
        if(func.file == nullptr) {
          Dwarf_Word idx = 0;
          if(dwarf_formudata(dwarf_attr_integrate(&die, DW_AT_decl_file,
                             &attr_mem), &idx) == 0 && idx > 0) {
            Dwarf_Die cu_mem;
            func.file = getFileDie(dwarf_diecu(&die, &cu_mem, nullptr, nullptr),
                                   idx, false);
            int linenum;
            if(dwarf_decl_line(&die, &linenum) == 0) func.line = linenum;
          }
        }

        // If this is not an inlined call, just emit as a normal Scope
        if(tag != DW_TAG_inlined_subroutine)
          return c.addScope(func, par);

        // Try to find the file for this inlined call.
        const File* srcf = nullptr;
        {
          Dwarf_Word idx = 0;
          if(dwarf_formudata(dwarf_attr_integrate(&die, DW_AT_call_file,
                             &attr_mem), &idx) == 0 && idx > 0) {
            srcf = getFileDie(&root, idx, true);
          } else srcf = getFile(nullptr, &root, 0, true);
        }

        uint64_t linenum = 0;
        if((attr = dwarf_attr(&die, DW_AT_call_line, &attr_mem)) != nullptr) {
          Dwarf_Word word;
          if(dwarf_formudata(attr, &word) == 0) linenum = word;
        }

        return c.addScope(func, *srcf, linenum, par);
    }, [&](Dwarf_Die& die, Classification::Block* here, Classification::Block* par) {
        // Mark all remaining ranges as being from this tail
        ptrdiff_t offset = 0;
        Dwarf_Addr base, start, end;
        while((offset = dwarf_ranges(&die, offset, &base, &start, &end)) > 0) {
          if(start != end) end -= 1;
          c.setScope({start, end}, here);
        }
    }, nullptr);

    // Now that the scopes are in place for this CU, load in the line info.
    Dwarf_Lines* lines = nullptr;
    std::size_t cnt = 0;
    dwarf_getsrclines(&root, &lines, &cnt);
    for(std::size_t i = 0; i < cnt; i++) {
      Dwarf_Line* line = dwarf_onesrcline(lines, i);
      Dwarf_Addr addr = 0;
      dwarf_lineaddr(line, &addr);

      Dwarf_Files* dfiles;
      std::size_t fidx;
      dwarf_line_file(line, &dfiles, &fidx);
      const File* file = getFile(dfiles, &root, fidx, true);

      int lineno = 0;
      dwarf_lineno(line, &lineno);

      lscopes.emplace_back(addr, file, lineno);
    }
  }

  // Overwrite everything with our new line info.
  c.setLines(std::move(lscopes));
} catch(std::exception& e) {
  const auto& rpath = m.userdata[sink.resolvedPath()];
  util::log::error() << "Exception caught during DWARF parsing for "
    << m.path().filename().string() << ", information may not be complete.\n"
       "  what(): " << e.what() << "\n"
       "  Full path: " << (rpath.empty() ? m.path() : rpath).string();
}
#endif
