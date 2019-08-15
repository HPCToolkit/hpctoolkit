#include <set>

#include <ElfHelper.hpp>
#include <CodeSource.h>
#include <CodeObject.h>

#include <lib/binutils/VMAInterval.hpp>

bool
readCubinCFG
(
 const std::string &search_path,
 ElfFile *elfFile,
 Dyninst::SymtabAPI::Symtab *the_symtab, 
 Dyninst::ParseAPI::CodeSource **code_src, 
 Dyninst::ParseAPI::CodeObject **code_obj
);
