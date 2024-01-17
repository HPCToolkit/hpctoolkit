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
// Copyright ((c)) 2002-2023, Rice University
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

//***************************************************************************
// system includes
//***************************************************************************

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
#include <sstream>



//***************************************************************************
// Dyninst includes
//***************************************************************************

#include <CodeSource.h>
#include <CodeObject.h>
#include <CFGFactory.h>



//***************************************************************************
// HPCToolkit includes
//***************************************************************************

#include "../../binutils/VMAInterval.hpp"
#include "../../support/FileUtil.hpp"

#include "GPUCFGFactory.hpp"
#include "GPUFunction.hpp"
#include "GPUBlock.hpp"
#include "GPUCodeSource.hpp"
#include "CudaCFGParser.hpp"
#include "GraphReader.hpp"
#include "GPUCFG_Cuda.hpp"



//***************************************************************************
// macros
//***************************************************************************

#define DEBUG_CFG_PARSE  0



//***************************************************************************
// import namespaces
//***************************************************************************

using namespace Dyninst;
using namespace ParseAPI;
using namespace SymtabAPI;
using namespace InstructionAPI;


//***************************************************************************
// private operations
//***************************************************************************

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

static void
splitFunctionName
(
  const std::unordered_map<std::string, std::vector<Symbol*> > &symbols_by_name,
  const std::string& name,
  std::string& symbol_name,
  int& suffix
)
{
  auto iter = symbols_by_name.find(name);
  // If the function name can be found in the symbol table,
  // then we assume it is a function name without a suffix
  symbol_name = name;
  suffix = 0;
  if (iter == symbols_by_name.end()) {
    auto pos = name.rfind("__");
    if (pos != std::string::npos) {
      suffix = stoi(name.substr(pos + 2));
      symbol_name = name.substr(0, pos);
    }
  }
}

// Iterate all the functions in the symbol table.
// Parse the ones that can be dumped by nvdisasm in sequence;
// construct a dummy block for others
static void
parseDotCFG
(
 const std::string &search_path,
 const std::string &elf_filename,
 const std::string &dot_filename,
 const std::string &cubin,
 int cuda_arch,
 Dyninst::SymtabAPI::Symtab *the_symtab,
 std::vector<GPUParse::Function *> &functions
)
{
  GPUParse::CudaCFGParser cfg_parser;
  // Step 1: parse all function symbols
  std::vector<Symbol *> symbols;
  the_symtab->getAllDefinedSymbols(symbols); // skipping undefined symbols

  // Ensure that the symbols are processed in its symbol table index order
  sort(symbols.begin(), symbols.end(),
    [] (Symbol* a, Symbol *b) {
      return a->getIndex() < b->getIndex();
    }
  );

  if (DEBUG_CFG_PARSE) {
    std::cout << "Debug symbols: " << std::endl;
    for (auto *symbol : symbols) {
      if (symbol->getType() == Dyninst::SymtabAPI::Symbol::ST_FUNCTION) {
        std::cout << "Function: " << symbol->getMangledName() << " offset: " <<
          symbol->getOffset() << " size: " << symbol->getSize() << std::endl;
      }
    }
  }

  // Store functions that are not parsed by nvdisasm
  std::vector<Symbol *> unparsable_function_symbols;
  // Store functions that are parsed by nvdisasm
  std::vector<Symbol *> parsed_function_symbols;
  // nvdisasm add suffix such as "__1" and "__3" to function names
  // that are shared by multiple functions.
  // Here we split such name into its symbol name and its suffix
  // and use the pair as the key for the function map
  std::map< std::pair<std::string, int>, GPUParse::Function *> function_map;
  // cubin may have multiple symbols that share the same name
  std::unordered_map<std::string, std::vector<Symbol*> > symbols_by_name;

  for (auto *symbol : symbols) {
    if (symbol->getType() == Dyninst::SymtabAPI::Symbol::ST_FUNCTION) {
      symbols_by_name[symbol->getMangledName()].emplace_back(symbol);
    }
  }

  // Test valid symbols
  for (auto *symbol : symbols) {
    if (symbol->getType() == Dyninst::SymtabAPI::Symbol::ST_FUNCTION) {
      auto index = symbol->getIndex();
      const std::string cmd = CUDA_NVDISASM_PATH " -fun " +
        std::to_string(index) + " -cfg -poff " + cubin + " > " + dot_filename;
      if (system(cmd.c_str()) == 0) {
        parsed_function_symbols.push_back(symbol);
        // Only parse valid symbols
        GPUParse::GraphReader graph_reader(dot_filename);
        GPUParse::Graph graph;
        std::vector<GPUParse::Function *> funcs;
        graph_reader.read(graph);
        cfg_parser.parse(graph, funcs);
        // Local functions inside a global function cannot be independently parsed
        for (auto *func : funcs) {
          std::string symbol_name;
          int nvdisasm_suffix;
          splitFunctionName(symbols_by_name, func->name, symbol_name, nvdisasm_suffix);
          function_map.emplace(std::make_pair(symbol_name, nvdisasm_suffix), func);
        }
      } else {
        unparsable_function_symbols.push_back(symbol);
        std::cout << "WARNING: unable to parse function: " << symbol->getMangledName() << std::endl;
      }
    }
  }

  std::unordered_map<GPUParse::Function *, Symbol*> parsed_func_symbol_map;
  std::unordered_map<std::string, int> symbol_name_counter;
  // Function objects are sorted based on the pair of symbol name and suffix.
  // The suffix is incremented in the symbol table index order
  for (auto &iter : function_map) {
    functions.push_back(iter.second);
    const std::string& symbol_name = iter.first.first;
    size_t counter = symbol_name_counter[symbol_name]++;
    if (counter < symbols_by_name[symbol_name].size()) {
      parsed_func_symbol_map[iter.second] = symbols_by_name[symbol_name][counter];
    }
  }

  // Step 2: Relocate functions
  for (auto &iter : parsed_func_symbol_map) {
    auto symbol = iter.second;
    auto function = iter.first;
    auto begin_offset = function->blocks[0]->begin_offset;
    for (auto *block : function->blocks) {
      for (auto *inst : block->insts) {
        inst->offset = (inst->offset - begin_offset) + symbol->getOffset();
        inst->size = cuda_arch >= 70 ? 16 : 8;
      }
      block->address = block->insts[0]->offset;
    }
    // Allow gaps between a function beginning and the first block?
    //function->blocks[0]->address = symbol->getOffset();
    function->address = symbol->getOffset();
  }

  if (DEBUG_CFG_PARSE) {
    std::cout << "Debug functions before adding unparsable" << std::endl;
    for (auto *function : functions) {
      std::cout << "Function: " << function->name << std::endl;
      for (auto *block : function->blocks) {
        std::cout << "Block: " << block->name << " address: 0x" << std::hex << block->address << std::dec << std::endl;;
      }
      std::cout << std::endl;
    }
  }

  // Step 3: add unparsable functions
  // Rename function and block ids
  size_t max_block_id = 0;
  size_t max_function_id = 0;
  for (auto *function : functions) {
    function->id = max_function_id++;
    for (auto *block : function->blocks) {
      block->id = max_block_id++;
    }
  }

  // For functions that cannot be parsed
  for (auto *symbol : unparsable_function_symbols) {
    auto function_name = symbol->getMangledName();
    auto *function = new GPUParse::Function(max_function_id++, std::move(function_name), 0);
    function->address = symbol->getOffset();
    auto block_name = symbol->getMangledName() + "_0";
    auto *block = new GPUParse::Block(max_block_id++, std::move(block_name));
    block->begin_offset = cuda_arch >= 70 ? 0 : 8;
    block->address = symbol->getOffset() + block->begin_offset;
    int len = cuda_arch >= 70 ? 16 : 8;
    // Add dummy insts
    for (size_t i = block->address; i < block->address + symbol->getSize(); i += len) {
      block->insts.push_back(new GPUParse::CudaInst(i, len));
    }
    function->blocks.push_back(block);
    functions.push_back(function);
    parsed_func_symbol_map[function] = symbol;
  }

  // Step4: add compensate blocks that only contains nop instructions
  for (auto &iter : parsed_func_symbol_map) {
    auto symbol = iter.second;
    auto function = iter.first;
    if (symbol->getSize() > 0) {
      int len = cuda_arch >= 70 ? 16 : 8;
      unsigned long function_size = function->blocks.back()->insts.back()->offset + len - function->address;
      unsigned long symbol_size = symbol->getSize();
      if (function_size < symbol_size) {
        if (DEBUG_CFG_PARSE) {
          std::cout << function->name << " append nop instructions" << std::endl;
          std::cout << "function_size: " << function_size << " < " << "symbol_size: " << symbol_size << std::endl;
        }
        auto *block = new GPUParse::Block(max_block_id, ".L_" + std::to_string(max_block_id));
        block->address = function_size + function->address;
        block->begin_offset = cuda_arch >= 70 ? 16 : 8;
        max_block_id++;
        while (function_size < symbol_size) {
          block->insts.push_back(new GPUParse::CudaInst(function_size + function->address, len));
          function_size += len;
        }
        if (function->blocks.size() > 0) {
          auto *last_block = function->blocks.back();
          last_block->targets.push_back(
            new GPUParse::Target(last_block->insts.back(), block, GPUParse::TargetType::DIRECT));
        }
        function->blocks.push_back(block);
      }
    }
  }

  // Parse function calls
  cfg_parser.parse_calls(functions);

  // Debug final functions and blocks
  if (DEBUG_CFG_PARSE) {
    std::cout << "Debug functions after adding unparsable" << std::endl;
    for (auto *function : functions) {
      std::cout << "Function: " << function->name << std::endl;
      for (auto *block : function->blocks) {
        std::cout << "Block: " << block->name << " address: 0x" << std::hex << block->address << std::dec << std::endl;;
      }
      std::cout << std::endl;
    }
  }

  // Step 5: create a nvidia directory and dump dot files
  if (parsed_function_symbols.size() > 0) {
    const std::string dot_dir = search_path + "/nvidia";
    int ret = mkdir(dot_dir.c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH);
    if (ret != 0 && errno != EEXIST) {
      std::cout << "WARNING: failed to mkdir: " << dot_dir << std::endl;
      return;
    }

    const std::string dot_output = search_path + "/nvidia/" + FileUtil::basename(elf_filename) + ".dot";
    std::string cmd = CUDA_NVDISASM_PATH " -cfg -poff -fun ";
    for (auto *symbol : parsed_function_symbols) {
      auto index = symbol->getIndex();
      cmd += std::to_string(index) + ",";
    }
    cmd += " " + cubin + " > " + dot_output;

    if (system(cmd.c_str()) != 0) {
      std::cout << "WARNING: failed to dump static database file: " << dot_output << std::endl;
    }
  }
}



//***************************************************************************
// interface operations
//***************************************************************************

std::string
getFilename
(
  void
)
{
  pid_t self = getpid();
  std::stringstream ss;
  ss << self;
  return ss.str();
}


void
buildCudaGPUCFG
(
 const std::string &search_path,
 ElfFile *elfFile,
 Dyninst::SymtabAPI::Symtab *the_symtab,
 Dyninst::ParseAPI::CodeSource **code_src,
 Dyninst::ParseAPI::CodeObject **code_obj
)
{
  bool dump_cubin_success = false;

  std::string filename = getFilename();
  std::string cubin = filename;
  std::string dot = filename + ".dot";

  dump_cubin_success = dumpCubin(cubin, elfFile);
  if (!dump_cubin_success) {
    std::cout << "ERROR: unable to write a cubin to the file system to analyze its CFG" << std::endl;
    throw 1;
  }

  std::vector<GPUParse::Function *> functions;
  parseDotCFG(search_path, elfFile->getFileName(), dot, cubin, elfFile->getArch(), the_symtab, functions);
  CFGFactory *cfg_fact = new GPUCFGFactory(functions);
  *code_src = new GPUCodeSource(functions, the_symtab);
  *code_obj = new CodeObject(*code_src, cfg_fact);
  (*code_obj)->parse();
  unlink(dot.c_str());
  unlink(cubin.c_str());
}
