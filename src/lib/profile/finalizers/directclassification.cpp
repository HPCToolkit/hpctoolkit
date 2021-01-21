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

#include "directclassification.hpp"

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

using namespace hpctoolkit;
using namespace finalizers;

DirectClassification::DirectClassification(uintmax_t dt)
  : dwarfThreshold(dt) {
  elf_version(EV_CURRENT);  // We always assume the current ELF version.
}

// Search for an alternative debug file to load. Roughly copied from
// dwarf_getalt, which is a rough copy of GDB's handling.
static stdshim::filesystem::path altfile(const stdshim::filesystem::path& path, Elf* elf) {
  std::error_code ec;
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

#ifdef ELF_C_READ_MMAP
#define HPC_ELF_C_READ ELF_C_READ_MMAP
#else
#define HPC_ELF_C_READ ELF_C_READ
#endif

void DirectClassification::module(const Module& m, Classification& c) noexcept {
  int fd = -1;
  const auto& rpath = m.userdata[sink.resolvedPath()];
  const auto& mpath = rpath.empty() ? m.path() : rpath;
  fd = open(mpath.c_str(), O_RDONLY);
  if(fd == -1) {  // Can't do anything if we can't open it.
    if(c.empty())
      util::log::error{} << "Unable to find binary, no lexical information available for " << mpath.string();
  }

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

  // Process the DWARF, but only if its small enough
  auto baseweight = stdshim::filesystem::file_size(mpath);
  Dwarf* dbg = dwarf_begin_elf(elf, DWARF_C_READ, nullptr);
  if(dbg != nullptr) {
    if(dwarfThreshold == std::numeric_limits<uintmax_t>::max()
       || baseweight < dwarfThreshold) {
      if(!fullDwarf(dbg, m, c))
        util::log::error{} << "Error parsing DWARF for " << mpath.string();
    } else util::log::warning{} << "Skipping DWARF for " << mpath.string() << ","
        " over threshold (" << baseweight << " > " << dwarfThreshold << ")";
    dwarf_end(dbg);
  } else {
    auto altpath = altfile(mpath, elf);
    if(!altpath.empty()) {
      int altfd = -1;
      altfd = open(altpath.c_str(), O_RDONLY);
      if(altfd != -1) {
        auto altweight = stdshim::filesystem::file_size(altpath);
        Dwarf* altdbg = dwarf_begin(altfd, DWARF_C_READ);
        if(altdbg != nullptr) {
          if(dwarfThreshold == std::numeric_limits<uintmax_t>::max()
             || baseweight + altweight < dwarfThreshold)
            fullDwarf(altdbg, m, c);
          else util::log::warning{} << "Skipping DWARF for " << mpath.string()
            << ", over threshold (" << baseweight << " + " << altweight << " ="
            " " << (baseweight + altweight) << " > " << dwarfThreshold << ")";
          dwarf_end(altdbg);
        }
        close(altfd);
      }
    }
  }

  if(!symtab(elf, m, c))
    util::log::error{} << "Error parsing ELF symbols for " << mpath.string();

  elf_end(elf);
  close(fd);
}

template<class Elf_Shdr, class Elf_Sym, auto elf_getshdr>
static bool parse(Elf* elf, const Module& m, Classification& c) {
  // Try our best to find the symbol table.
  Elf_Scn* s_symtab = nullptr;
  Elf_Scn* s_dynsym = nullptr;
  for(Elf_Scn* s = elf_nextscn(elf, nullptr); s != nullptr; s = elf_nextscn(elf, s)) {
    Elf_Shdr* hdr = elf_getshdr(s);
    if(hdr->sh_type == SHT_SYMTAB) { s_symtab = s; break; }
    else if(hdr->sh_type == SHT_DYNSYM) { s_dynsym = s; }
  }
  if(s_symtab == nullptr) s_symtab = s_dynsym;
  if(s_symtab == nullptr) return false;
  Elf_Shdr* sh_symtab = elf_getshdr(s_symtab);

  // Get handles for the contained data, so we can start working
  Elf_Data* symtab = elf_getdata(s_symtab, nullptr);
  if(symtab == nullptr) return false;  // Internal error

  // Walk though the symbols
  for(std::size_t off = 0; off < sh_symtab->sh_size; off += sizeof(Elf_Sym)) {
    Elf_Sym* sym = (Elf_Sym*)&((const char*)symtab->d_buf)[off];
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
  return true;
}

bool DirectClassification::symtab(void* elf_vp, const Module& m, Classification& c) {
  Elf* elf = (Elf*)elf_vp;

  if(elf_kind(elf) != ELF_K_ELF) return false;

  switch(elf_getident(elf, nullptr)[EI_CLASS]) {
  case ELFCLASS32: return parse<Elf32_Shdr, Elf32_Sym, elf32_getshdr>(elf, m, c);
  case ELFCLASS64: return parse<Elf64_Shdr, Elf64_Sym, elf64_getshdr>(elf, m, c);
  default: return false;
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

bool DirectClassification::fullDwarf(void* dbg_vp, const Module& m, Classification& c) try {
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
  return true;
} catch(std::exception& e) {
  const auto& rpath = m.userdata[sink.resolvedPath()];
  util::log::info{} << "Exception caught during DWARF parsing for "
    << m.path().filename().string() << "\n"
       "  what(): " << e.what() << "\n"
       "  Full path: " << (rpath.empty() ? m.path() : rpath).string();
  return false;
}
