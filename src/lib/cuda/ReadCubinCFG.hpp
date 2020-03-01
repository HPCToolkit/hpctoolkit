#include <set>

#include <lib/binutils/VMAInterval.hpp>
#include <lib/binutils/ElfHelper.hpp>

#include <CodeSource.h>
#include <CodeObject.h>

bool
readCubinCFG
(
 const std::string &search_path,
 ElfFile *elfFile,
 Dyninst::SymtabAPI::Symtab *the_symtab, 
 bool cfg_wanted,
 Dyninst::ParseAPI::CodeSource **code_src, 
 Dyninst::ParseAPI::CodeObject **code_obj,
 bool dump_insts = true,
 bool control = true,
 bool slice = true,
 bool livenes = false
);
