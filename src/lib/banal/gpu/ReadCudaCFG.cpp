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
#include <omp.h>

#include <set>
#include <sstream>


#include <CodeSource.h>
#include <CodeObject.h>
#include <CFGFactory.h>


#include <lib/binutils/VMAInterval.hpp>
#include <lib/support/FileUtil.hpp>


#include "GPUCFGFactory.hpp"
#include "GPUFunction.hpp"
#include "GPUBlock.hpp"
#include "GPUCodeSource.hpp"
#include "CudaCFGParser.hpp"
#include "GraphReader.hpp"
#include "ReadCudaCFG.hpp"

using namespace Dyninst;
using namespace ParseAPI;
using namespace SymtabAPI;
using namespace InstructionAPI;

#define DEBUG_CFG_PARSE  0


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
  symbol_name = name;
  suffix = 0;
  if (iter == symbols_by_name.end()) {
    // If the function name cannot be found in the symbol table,
    // then we assume it is a function name with a suffix
    auto pos = name.rfind("__");
    if (pos != std::string::npos && pos != 0) {
      // +1 to differentiate between __cuda_div_xxx and __cuda_div_xxx__0
      suffix = stoi(name.substr(pos + 2)) + 1;
      symbol_name = name.substr(0, pos);
    }
  }
}


static void
addCompensateBlocks
(
 int cuda_arch,
 std::unordered_map<GPUParse::Function *, Symbol*> &parsed_func_symbol_map,
 size_t &max_block_id
)
{
  for (auto &iter : parsed_func_symbol_map) {
    auto symbol = iter.second;
    auto function = iter.first;
    if (symbol->getSize() > 0) {
      int len = GPUParse::get_cuda_inst_size(cuda_arch);
      int function_size = function->blocks.back()->insts.back()->offset + len - function->address;
      int symbol_size = symbol->getSize();
      if (function_size < symbol_size) {
        auto *block = new GPUParse::Block(max_block_id, ".L_" + std::to_string(max_block_id));
        block->address = function_size + function->address;
        block->begin_offset = GPUParse::get_cuda_inst_size(cuda_arch);
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
}


static void
relocateFunctions
(
 int cuda_arch,
 std::unordered_map<GPUParse::Function *, Symbol*> &parsed_func_symbol_map
)
{
  for (auto &iter : parsed_func_symbol_map) {
    auto symbol = iter.second;
    auto function = iter.first;
    auto begin_offset = function->blocks[0]->begin_offset;
    for (auto *block : function->blocks) {
      for (auto *inst : block->insts) {
        inst->offset = (inst->offset - begin_offset) + symbol->getOffset();
        inst->size = GPUParse::get_cuda_inst_size(cuda_arch);
      }
      block->address = block->insts[0]->offset;
    }
    // Allow gaps between a function begining and the first block?
    //function->blocks[0]->address = symbol->getOffset();
    function->address = symbol->getOffset();
  }
}

static void
assignFunctionAndBlockIds
(
 std::vector<GPUParse::Function *> &functions,
 size_t &max_function_id,
 size_t &max_block_id
)
{
  for (auto *function : functions) {
    function->id = max_function_id++;
    for (auto *block : function->blocks) {
      block->id = max_block_id++;
    }
  }
}


static void
addUnparsableFunctions
(
 int cuda_arch,
 std::vector<GPUParse::Function *> &functions,
 std::vector<Symbol *> &unparsable_function_symbols,
 std::unordered_map<GPUParse::Function *, Symbol*> parsed_func_symbol_map,
 size_t &max_function_id,
 size_t &max_block_id
)
{
  // For functions that cannot be parsed
  for (auto *symbol : unparsable_function_symbols) {
    auto function_name = symbol->getMangledName();
    auto *function = new GPUParse::Function(max_function_id++, std::move(function_name));
    function->address = symbol->getOffset();
    auto block_name = symbol->getMangledName() + "_0";
    auto *block = new GPUParse::Block(max_block_id++, std::move(block_name));
    block->begin_offset = GPUParse::get_cuda_func_offset(cuda_arch);
    block->address = symbol->getOffset() + block->begin_offset;
    int len = GPUParse::get_cuda_inst_size(cuda_arch);
    // Add dummy insts
    for (size_t i = block->address; i < block->address + symbol->getSize(); i += len) {
      block->insts.push_back(new GPUParse::CudaInst(i, len));
    }
    function->blocks.push_back(block);
    functions.push_back(function);
    parsed_func_symbol_map[function] = symbol;
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
 const std::string &cubin,
 int cuda_arch,
 Dyninst::SymtabAPI::Symtab *the_symtab,
 std::vector<GPUParse::Function *> &functions
) 
{
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
  // nvdisasm add suffix such as "__1" and "__3" to funciton names
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
  #pragma omp parallel shared(parsed_function_symbols, unparsable_function_symbols, symbols, \
    function_map, symbols_by_name)
  { 
    std::string dot_filename = cubin + std::to_string(omp_get_thread_num()) + ".dot"; 
    std::vector<Symbol *> local_parsed_function_symbols;
    std::vector<Symbol *> local_unparsable_function_symbols;
    decltype(function_map) local_function_map;

    #pragma omp for schedule(dynamic, 1)
    for (size_t i = 0; i < symbols.size(); ++i) {
      auto *symbol = symbols[i];
      if (symbol->getType() == Dyninst::SymtabAPI::Symbol::ST_FUNCTION) {
        auto index = symbol->getIndex();
        const std::string cmd = "nvdisasm -fun " +
          std::to_string(index) + " -cfg -poff " + cubin + " > " + dot_filename;
        if (system(cmd.c_str()) == 0) {
          // Only parse valid symbols
          local_parsed_function_symbols.push_back(symbol);
          GPUParse::Graph graph;
          std::vector<GPUParse::Function *> funcs;
          GPUParse::GraphReader::read(dot_filename, graph);
          GPUParse::CudaCFGParser::parse(graph, funcs);
          // Local functions inside a global function cannot be independently parsed
          for (auto *func : funcs) {
            std::string symbol_name;
            int nvdisasm_suffix;
            splitFunctionName(symbols_by_name, func->name, symbol_name, nvdisasm_suffix);
            local_function_map.emplace(std::make_pair(symbol_name, nvdisasm_suffix), func);
          }
        } else {
          local_unparsable_function_symbols.push_back(symbol);
          std::cout << "WARNING: unable to parse function: " << symbol->getMangledName() << std::endl;
        }
      }
    }

    unlink(dot_filename.c_str());

    #pragma omp critical
    {
      for (auto &iter : local_function_map) {
        function_map[iter.first] = iter.second;
      }
      for (auto *symbol : local_unparsable_function_symbols) {
        unparsable_function_symbols.push_back(symbol);
      }
      for (auto *symbol : local_parsed_function_symbols) {
        parsed_function_symbols.push_back(symbol);
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
  relocateFunctions(cuda_arch, parsed_func_symbol_map);

  // Step 3: Assign function and block ids
  size_t max_block_id = 0;
  size_t max_function_id = 0;
  assignFunctionAndBlockIds(functions, max_function_id, max_block_id);

  // Step 4: Add unparsable functions
  addUnparsableFunctions(cuda_arch, functions, unparsable_function_symbols,
    parsed_func_symbol_map, max_function_id, max_block_id);

  // Step 5: Add compensate blocks that only contains nop instructions
  addCompensateBlocks(cuda_arch, parsed_func_symbol_map, max_block_id);

  // Step 6:
  // Parse function calls to link call instructions with unnested functions.
  // nested functions can be parsed directly for individual global functions
  // 
  // XXX: nested functions are device functions inside global functions
  GPUParse::CudaCFGParser::parse_calls(functions);

  // Step 7: create a nvidia directory and dump dot files
  if (DEBUG_CFG_PARSE) {
    if (parsed_function_symbols.size() > 0) {
      const std::string dot_dir = search_path + "/nvidia";
      int ret = mkdir(dot_dir.c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH);
      if (ret != 0 && errno != EEXIST) {
        std::cout << "WARNING: failed to mkdir: " << dot_dir << std::endl;
        return;
      }

      const std::string dot_output = search_path + "/nvidia/" + FileUtil::basename(elf_filename) + ".dot";
      std::string cmd = "nvdisasm -cfg -poff -fun ";
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
}


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


bool
readCudaCFG
(
 const std::string &search_path,
 ElfFile *elfFile,
 Dyninst::SymtabAPI::Symtab *the_symtab, 
 bool cfg_wanted,
 Dyninst::ParseAPI::CodeSource **code_src, 
 Dyninst::ParseAPI::CodeObject **code_obj
) 
{
  static bool compute_cfg = cfg_wanted && test_nvdisasm();
  bool dump_cubin_success = false;

  if (compute_cfg) {
    std::string filename = getFilename();
    std::string cubin = filename;

    dump_cubin_success = dumpCubin(cubin, elfFile);
    if (!dump_cubin_success) {
      std::cout << "WARNING: unable to write a cubin to the file system to analyze its CFG" << std::endl; 
    } else {
      std::vector<GPUParse::Function *> functions;
      parseDotCFG(search_path, elfFile->getFileName(), cubin, elfFile->getArch(), the_symtab, functions);
      CFGFactory *cfg_fact = new GPUCFGFactory(functions);
      *code_src = new GPUCodeSource(functions, the_symtab); 
      *code_obj = new CodeObject(*code_src, cfg_fact);
      (*code_obj)->parse();
      unlink(cubin.c_str());
      return true;
    }
  }

  *code_src = new SymtabCodeSource(the_symtab);
  *code_obj = new CodeObject(*code_src, NULL, NULL, false, true);

  return false;
}
