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

using namespace hpctoolkit;
using namespace finalizers;

DirectClassification::DirectClassification() {
  elf_version(EV_CURRENT);  // We always assume the current ELF version.
}

void DirectClassification::module(const Module& m, Classification& c) {
  int fd = -1;
  const auto& rpath = m.userdata[sink.resolvedPath()];
  if(!rpath.empty()) fd = open(rpath.c_str(), O_RDONLY);
  else fd = open(m.path().c_str(), O_RDONLY);
  if(fd == -1) return;  // Can't do anything if we can't open it.

  struct stat sbuf;
  fstat(fd, &sbuf);
  if(c.filled({0, (uint64_t)sbuf.st_size-1})) {  // No holes left to fill
    close(fd);
    return;
  }

#ifdef ELF_C_READ_MMAP
  Elf* elf = elf_begin(fd, ELF_C_READ_MMAP, nullptr);
#else
  Elf* elf = elf_begin(fd, ELF_C_READ, nullptr);
#endif
  if(elf == nullptr) {
    close(fd);
    return;  // We only work with ELF files.
  }

  Dwarf* dbg = dwarf_begin_elf(elf, DWARF_C_READ, nullptr);
  if(dbg != nullptr) {
    fullDwarf(dbg, m, c);
    dwarf_end(dbg);
  }

  symtab(elf, m, c);

  elf_end(elf);
  close(fd);
}

void DirectClassification::symtab(void* elf_vp, const Module& m, Classification& c) {
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
    char* dn = hpctoolkit_demangle(name);
    if(dn) {
      func.name = dn;
      std::free(dn);
    } else func.name = name;

    // Add our Classification as usual.
    c.setScope(range, c.addScope(func, Classification::scopenone));
  }
}

// Helper recursive thing
template<class T>
void dwarfwalk(Dwarf_Die die, const std::function<T(Dwarf_Die&, T)>& pre,
               const std::function<void(Dwarf_Die&, T, T)>& post, const T& t) {
  do {
    Dwarf_Die child;
    T subt = pre(die, t);
    if(dwarf_child(&die, &child) == 0)
      dwarfwalk(child, pre, post, subt);
    post(die, subt, t);
  } while(dwarf_siblingof(&die, &die) == 0);
}

void DirectClassification::fullDwarf(void* dbg_vp, const Module& m, Classification& c) try {
  std::unordered_map<const char*, Function*> funcs;
  std::vector<Classification::LineScope> lscopes;

  Dwarf* dbg = (Dwarf*)dbg_vp;

  // Walk through each CU and start setting scopes.
  Dwarf_CU* cu = nullptr;
  Dwarf_Die root;
  while(dwarf_get_units(dbg, cu, &cu, nullptr, nullptr, &root, nullptr) == 0) {
    // Get all the source files and make a map from the DWARF to our structures.
    std::vector<const File*> files;
    {
      Dwarf_Files* dfiles = nullptr;
      std::size_t dfiles_cnt = 0;
      dwarf_getsrcfiles(&root, &dfiles, &dfiles_cnt);

      stdshim::filesystem::path compdir;
      const char* const* dirs = nullptr;
      std::size_t dirs_cnt = 0;
      dwarf_getsrcdirs(dfiles, &dirs, &dirs_cnt);
      if(dirs[0] != nullptr) compdir = std::string(dirs[0]);

      for(std::size_t i = 0; i < dfiles_cnt; i++) {
        stdshim::filesystem::path fn(std::string(dwarf_filesrc(dfiles, i, nullptr, nullptr)));
        if(!fn.is_absolute()) fn = compdir / fn;
        files.emplace_back(&sink.file(fn));
      }
    }

    // Walk the DIEs for the function ranges in this CU
    dwarfwalk<std::size_t>(root,
      [&](Dwarf_Die& die, std::size_t paridx) -> std::size_t {
        // Check that the DIE is a function-like thing. Skip if not.
        int tag = dwarf_tag(&die);
        if(tag != DW_TAG_subprogram && tag != DW_TAG_inlined_subroutine)
          return paridx;

        // Skip any abstract instances. We want only the concrete ones.
        if(dwarf_func_inline(&die)) return paridx;

        // Unwrap this as much as possible to get the actual TAG_subprogram DIE.
        Dwarf_Attribute* attr;
        Dwarf_Attribute attr_mem;
        Dwarf_Die die_mem;
        Dwarf_Die* funcdie = &die;
        while(true) {
          attr = dwarf_attr(funcdie, DW_AT_abstract_origin, &attr_mem);
          if(attr == nullptr)
            attr = dwarf_attr(funcdie, DW_AT_specification, &attr_mem);
          if(attr == nullptr) break;
          funcdie = dwarf_formref_die(attr, &die_mem);
        }

        // Try and get a link-name for the function. Libdw (should) deduplicate
        // the names so we can just use the const char*.
        // If it doesn't have a name, we skip it for now. It wouldn't help us.
        const char* funcname;
        {
          attr = dwarf_attr(funcdie, DW_AT_linkage_name, &attr_mem);
          if(attr == nullptr)
            attr = dwarf_attr(funcdie, DW_AT_name, &attr_mem);
          if(attr == nullptr) return paridx;
          funcname = dwarf_formstring(attr);
        }

        // Get the Function for this, or create one if none exist.
        auto it = funcs.find(funcname);
        if(it == funcs.end())
          it = funcs.emplace(funcname, &c.addFunction(m)).first;
        auto& func = *it->second;

        // Try to fill in as much information about the Function from this DIE.
        {
          Dwarf_Attribute* attr;
          if(func.name.empty()) {
            char* dn = hpctoolkit_demangle(funcname);
            if(dn) {
              func.name = dn;
              std::free(dn);
            } else func.name = funcname;
          }
          if(func.offset == 0) {
            Dwarf_Addr offset;
            if(dwarf_entrypc(funcdie, &offset) == 0) func.offset = offset;
          }
          if(func.file == nullptr) {
            // If there is no file info, whatever we have is good enough.
            attr = dwarf_attr_integrate(&die, DW_AT_decl_file, &attr_mem);
            if(attr != nullptr) {
              Dwarf_Word word;
              dwarf_formudata(attr, &word);
              func.file = files.at(word);
            }
            attr = dwarf_attr_integrate(&die, DW_AT_decl_line, &attr_mem);
            if(attr != nullptr) {
              Dwarf_Word word;
              dwarf_formudata(attr, &word);
              func.line = word;
            }
          } else {
            // If there's already file info, check if ours is a non-declaration.
            Dwarf_Die die_mem2;
            Dwarf_Die* basedie = &die;
            while(true) {
              attr = dwarf_attr(basedie, DW_AT_declaration, &attr_mem);
              bool isdecl = false;
              dwarf_formflag(attr, &isdecl);
              if(isdecl) break;  // Stop unwinding if its a declaration.

              attr = dwarf_attr(basedie, DW_AT_decl_file, &attr_mem);
              if(attr != nullptr) {  // Use this one, its better, probably.
                Dwarf_Word word;
                dwarf_formudata(attr, &word);
                func.file = files.at(word);
                attr = dwarf_attr_integrate(&die, DW_AT_decl_line, &attr_mem);
                if(attr != nullptr) {
                  Dwarf_Word word;
                  dwarf_formudata(attr, &word);
                  func.line = word;
                }
                break;
              }

              attr = dwarf_attr(basedie, DW_AT_abstract_origin, &attr_mem);
              if(attr == nullptr)
                attr = dwarf_attr(basedie, DW_AT_specification, &attr_mem);
              if(attr == nullptr) break;  // Nowhere to go now
              basedie = dwarf_formref_die(attr, &die_mem2);
            }
          }
        }

        // Get the file and line of this inlined function call. If its inlined.
        const File* srcf = nullptr;
        uint64_t srcl = 0;
        {
          Dwarf_Word word;
          Dwarf_Attribute* attr = dwarf_attr(&die, DW_AT_call_file, &attr_mem);
          if(attr != nullptr) {
            dwarf_formudata(attr, &word);
            srcf = files.at(word);
          }
          attr = dwarf_attr(&die, DW_AT_call_line, &attr_mem);
          if(attr != nullptr) {
            dwarf_formudata(attr, &word);
            srcl = word;
          }
        }

        // Build/get the tail ScopeChain for this DIE
        return srcf ? c.addScope(func, *srcf, srcl, paridx)
                    : c.addScope(func, paridx);
    }, [&](Dwarf_Die& die, std::size_t idx, std::size_t paridx) {
        // Mark all remaining ranges as being from this tail
        ptrdiff_t offset = 0;
        Dwarf_Addr base, start, end;
        while((offset = dwarf_ranges(&die, offset, &base, &start, &end)) > 0) {
          if(start != end) end -= 1;
          c.setScope({start, end}, idx);
        }
    }, Classification::scopenone);

    // Now that the scopes are in place for this CU, load in the line info.
    Dwarf_Lines* lines = nullptr;
    std::size_t cnt = 0;
    dwarf_getsrclines(&root, &lines, &cnt);
    for(std::size_t i = 0; i < cnt; i++) {
      Dwarf_Line* line = dwarf_onesrcline(lines, i);
      Dwarf_Addr addr;
      dwarf_lineaddr(line, &addr);
      Dwarf_Files* dfiles;
      std::size_t fidx;
      dwarf_line_file(line, &dfiles, &fidx);
      int lineno;
      dwarf_lineno(line, &lineno);
      lscopes.emplace_back(addr, files[fidx], lineno);
    }
  }

  // Overwrite everything with our new line info.
  c.setLines(std::move(lscopes));
} catch(std::exception& e) {
  util::log::error() << "Exception caught during DWARF parsing for "
    << m.path().filename().string() << ", information may not be complete.\n"
       "  what(): " << e.what() << "\n"
       "  Full path: " << m.path().string();
}
