#include <set>

#include <ElfHelper.hpp>
#include <CodeSource.h>
#include <CodeObject.h>

#include <lib/binutils/VMAInterval.hpp>

void
readCubinCFG
(
 ElfFile *elfFile,
 Dyninst::SymtabAPI::Symtab *the_symtab, 
 Dyninst::ParseAPI::CodeSource **code_src, 
 Dyninst::ParseAPI::CodeObject **code_obj,
 std::set<VMA> &parsable
);
