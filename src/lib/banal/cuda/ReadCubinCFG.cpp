#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <set>


#include <CodeSource.h>
#include <CodeObject.h>
#include <CFGFactory.h>


#include <lib/binutils/VMAInterval.hpp>


#include "CudaCFGFactory.hpp"
#include "CudaFunction.hpp"
#include "CudaBlock.hpp"
#include "CudaCodeSource.hpp"
#include "CFGParser.hpp"
#include "GraphReader.hpp"
#include "ReadCubinCFG.hpp"

using namespace Dyninst;
using namespace ParseAPI;
using namespace SymtabAPI;
using namespace InstructionAPI;


static bool
test_nvdisasm() 
{
  // check whether nvdisasm works
  int retval = system("nvdisasm > /dev/null") == 0;
  if (!retval) {
    std::cout << "WARNING: nvdisasm is not available on your path to analyze control flow in NVIDIA CUBINs" << std::endl;
  }
  return retval;
}


static bool
dumpCubin
(
 const std::string &cubin, 
 ElfFile *elfFile 
) 
{
  int retval = false;
  int fd = open(cubin.c_str(), O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU);
  if (fd != -1) {
    int len = elfFile->getLength();
    retval = write(fd, elfFile->getMemoryOriginal(), len) == len; 
    close(fd);
  }
  return retval;
}


static bool
dumpDot
(
 const std::string &cubin, 
 const std::string &dot,
 Dyninst::SymtabAPI::Symtab *the_symtab,
 std::vector<size_t> valid_function_index
) 
{
  std::string cmd = "nvdisasm -cfg -poff " + cubin + " > " + dot;
  // for all symbols
  std::vector<Symbol *> symbols;
  the_symtab->getAllSymbols(symbols);
  if (system(cmd.c_str()) != 0) {  // failed, maybe caused by some functions
    // test valid symbols
    for (auto *symbol : symbols) {
      if (symbol->getType() == Dyninst::SymtabAPI::Symbol::ST_FUNCTION) {
        auto index = symbol->getIndex();
        cmd = "nvdisasm -fun " +
          std::to_string(index) + " -cfg -poff " + cubin + " > /dev/null";
        if (system(cmd.c_str()) == 0) {
          valid_function_index.push_back(index);
        }
      }
    }
    // only parse valid symbols
    if (!valid_function_index.empty()) {
      cmd = "nvdisasm -fun ";
      for (auto index : valid_function_index) {
        cmd += std::to_string(index) + ",";
      }
      cmd += " -cfg -poff " + cubin + " > " + dot;
      return system(cmd.c_str()) == 0;
    }
    return false;
  }
  return true;
}


static void
parseDotCFG
(
 const std::string &dot_filename, 
 CudaParse::CFGParser &cfg_parser,
 Dyninst::SymtabAPI::Symtab *the_symtab, 
 std::vector<CudaParse::Function *> &functions
) 
{
  CudaParse::GraphReader graph_reader(dot_filename);
  CudaParse::Graph graph;
  graph_reader.read(graph);
  cfg_parser.parse(graph, functions);

  // relocate functions
  std::vector<Symbol *> symbols;
  the_symtab->getAllSymbols(symbols);
  for (auto *symbol : symbols) {
    for (auto *function : functions) {
      if (function->name == symbol->getMangledName()) {
        auto begin_offset = function->blocks[0]->begin_offset;
        for (auto *block : function->blocks) {
          for (auto *inst : block->insts) {
            inst->offset = (inst->offset - begin_offset) + symbol->getOffset();
          }
          block->address = block->insts[0]->offset;
        }
        function->blocks[0]->address = symbol->getOffset();
        function->address = symbol->getOffset();
      }
    }
  }
}


static void
addUnparsedFunctions
(
  CudaParse::CFGParser &cfg_parser,
  Dyninst::SymtabAPI::Symtab *the_symtab, 
  std::vector<size_t> &valid_function_index,
  int cuda_arch,
  std::vector<CudaParse::Function *> &functions
)
{
  // find max function id and max block id
  size_t max_block_id = 0;
  size_t max_function_id = 0;
  for (auto *function : functions) {
    max_function_id = std::max(function->id, max_function_id);
    for (auto *block : function->blocks) {
      max_block_id = std::max(block->id, max_block_id);
    }
  }
  // for functions that cannot be parsed
  std::vector<Symbol *> symbols;
  the_symtab->getAllSymbols(symbols);
  for (auto *symbol : symbols) {
    if (symbol->getType() == Dyninst::SymtabAPI::Symbol::ST_FUNCTION &&
      std::find(valid_function_index.begin(), valid_function_index.end(), symbol->getIndex()) ==
      valid_function_index.end()) {
      auto function_name = symbol->getMangledName();
      auto *function = new CudaParse::Function(max_function_id, std::move(function_name));
      ++max_function_id;
      auto block_name = symbol->getMangledName() + "_block0";
      auto *block = new CudaParse::Block(max_block_id, std::move(block_name));
      ++max_block_id;
      block->begin_offset = 0;
      block->address = symbol->getOffset();
      int len = cuda_arch >= 70 ? 16 : 8;
      // add dummy insts
      for (size_t i = block->address; i < block->address + symbol->getSize(); i += len) {
        block->insts.push_back(new CudaParse::Inst(i));
      }
      function->blocks.push_back(block);
      functions.push_back(function);
    }
  }
  cfg_parser.parse_calls(functions);
}


bool
readCubinCFG
(
 ElfFile *elfFile,
 Dyninst::SymtabAPI::Symtab *the_symtab, 
 Dyninst::ParseAPI::CodeSource **code_src, 
 Dyninst::ParseAPI::CodeObject **code_obj
) 
{
  static bool nvdisasm_usable = test_nvdisasm();
  bool dump_cubin_success = false;
  bool dump_dot_success = false;

  if (nvdisasm_usable) {
    std::string filename = "tmp";
    std::string cubin = filename;
    std::string dot = filename + ".dot";

    dump_cubin_success = dumpCubin(cubin, elfFile);
    if (!dump_cubin_success) {
      std::cout << "WARNING: unable to write a cubin to the file system to analyze its CFG" << std::endl; 
    } else {
      std::vector<size_t> valid_function_index;
      dump_dot_success = dumpDot(cubin, dot, the_symtab, valid_function_index);
      if (!dump_dot_success) {
        std::cout << "WARNING: unable to use nvdisasm to produce a CFG for a cubin" << std::endl; 
      } else {
        // Parse dot cfg
        // relocate instructions according to the 
        // relocated symbols in the_symtab
        std::vector<CudaParse::Function *> functions;
        CudaParse::CFGParser cfg_parser;
        parseDotCFG(dot, cfg_parser, the_symtab, functions);
        addUnparsedFunctions(cfg_parser, the_symtab, valid_function_index, elfFile->getArch(), functions);

        CFGFactory *cfg_fact = new CudaCFGFactory(functions);
        *code_src = new CudaCodeSource(functions, the_symtab); 
        *code_obj = new CodeObject(*code_src, cfg_fact);
        (*code_obj)->parse();
        return true;
      }
    }
  }

  *code_src = new SymtabCodeSource(the_symtab);
  *code_obj = new CodeObject(*code_src);

  return false;
}
