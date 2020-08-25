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


#include <CodeSource.h>
#include <CodeObject.h>
#include <CFGFactory.h>


#include <lib/binutils/VMAInterval.hpp>
#include <lib/binutils/RelocateCubin.hpp>
#include <lib/support/FileUtil.hpp>

#include "CudaCFGFactory.hpp"
#include "CudaFunction.hpp"
#include "CudaBlock.hpp"
#include "CudaCodeSource.hpp"
#include "CFGParser.hpp"
#include "Instruction.hpp"
#include "AnalyzeInstruction.hpp"
#include "GraphReader.hpp"
#include "ReadCubinCFG.hpp"

using namespace Dyninst;
using namespace ParseAPI;
using namespace SymtabAPI;
using namespace InstructionAPI;

#define DEBUG_CFG_PARSE  0
#define CUDA_GLOBAL 16

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


// Iterate all the functions in the symbol table.
// Parse the ones that can be dumped by nvdisasm in sequence;
// construct a dummy block for others
static void
parseDotCFG
(
 const std::string &dot_filename, 
 const std::string &cubin,
 int cuda_arch,
 Dyninst::SymtabAPI::Symtab *the_symtab,
 std::vector<CudaParse::Function *> &functions,
 std::vector<int> symbol_visibility
) 
{
  CudaParse::CFGParser cfg_parser;
  // Step 1: parse all function symbols
  std::vector<Symbol *> symbols;
  the_symtab->getAllSymbols(symbols);

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
  // Remove functions that share the same names
  std::map<std::string, CudaParse::Function *> function_map;
  std::map<std::string, Symbol *> symbol_map;

  for (auto *symbol : symbols) {
    symbol_map[symbol->getMangledName()] = symbol;
  }

  // Test valid symbols
  for (auto *symbol : symbols) {
    if (symbol->getType() == Dyninst::SymtabAPI::Symbol::ST_FUNCTION && symbol->getSize() != 0) {
      auto index = symbol->getIndex();
      const std::string cmd = "nvdisasm -fun " +
        std::to_string(index) + " -cfg -poff " + cubin + " > " + dot_filename;
      if (system(cmd.c_str()) == 0) {
        // Only parse valid symbols
        CudaParse::GraphReader graph_reader(dot_filename);
        CudaParse::Graph graph;
        std::vector<CudaParse::Function *> functions;
        graph_reader.read(graph);
        cfg_parser.parse(graph, functions);
        // Local functions inside a global function cannot be independently parsed
        for (auto *function : functions) {
          if (function_map.find(function->name) == function_map.end()) {
            auto iter = symbol_map.find(function->name);
            if (iter == symbol_map.end()) {
              // If nvcc-11 has special suffix, remove it
              auto function_name = function->name;
              auto pos = function_name.rfind("__");
              if (pos == std::string::npos) {
                // cannot find
                continue;
              }
              function_name.erase(pos);
              iter = symbol_map.find(function_name);
              if (iter == symbol_map.end()) {
                // cannot find
                continue;
              }
              function->name = function_name;
            }
            // Assign symbol index to function
            auto *symbol_function = iter->second;
            function->index = symbol_function->getIndex();
            function->address = symbol_function->getOffset();
            function->global = symbol_visibility[function->index] == CUDA_GLOBAL ? true : false;
            if (symbol_function != symbol) {
              if (symbol_function->getType() != Dyninst::SymtabAPI::Symbol::ST_FUNCTION) {
                // NOTYPE functions' original offsets are relative.
                // hpcstruct relocates them with absolute offsets.
                // Allow gaps between a function begining and the first block?
                //function->blocks[0]->address = symbol->getOffset();
                function->address += symbol->getOffset();
              }
            }
            function_map[function->name] = function;
          }
        }
      } else {
        unparsable_function_symbols.push_back(symbol);
        std::cout << "WARNING: unable to parse function: " << symbol->getMangledName() << std::endl;
      }
    }
  }

  for (auto &iter : function_map) {
    functions.push_back(iter.second);
  }

  // Step 2: Relocate instructions
  for (auto *function : functions) {
    auto begin_offset = function->blocks[0]->begin_offset;
    for (auto *block : function->blocks) {
      for (auto *inst : block->insts) {
        inst->offset = (inst->offset - begin_offset) + function->address;
      }
      block->address = block->insts[0]->offset;
    }
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
    auto *function = new CudaParse::Function(max_function_id++, std::move(function_name));
    function->unparsable = true;
    // Assign symbol index to function
    function->index = symbol->getIndex();
    function->address = symbol->getOffset();
    auto block_name = symbol->getMangledName() + "_0";
    auto *block = new CudaParse::Block(max_block_id++, std::move(block_name));
    block->begin_offset = cuda_arch >= 70 ? 0 : 8;
    // Absolute address
    block->address = block->begin_offset + function->address;
    int len = cuda_arch >= 70 ? 16 : 8;
    // Add dummy insts with absolute addresses
    for (size_t i = block->begin_offset; i < symbol->getSize(); i += len) {
      block->insts.push_back(new CudaParse::Instruction(i + function->address, i));
    }
    function->blocks.push_back(block);
    functions.push_back(function);
  }

  // Step4: add compensate blocks that only contains nop instructions
  for (auto *function : functions) {
    if (symbol_map.find(function->name) == symbol_map.end()) {
      continue;
    }
    auto *symbol = symbol_map[function->name];
    int len = cuda_arch >= 70 ? 16 : 8;
    int function_size = function->blocks.back()->insts.back()->offset + len - function->address;
    int symbol_size = symbol->getSize();
    if (function_size < symbol_size) {
      if (DEBUG_CFG_PARSE) {
        std::cout << function->name << " append nop instructions" << std::endl;
        std::cout << "function_size: " << function_size << " < " << "symbol_size: " << symbol_size << std::endl;
      }
      auto *block = new CudaParse::Block(max_block_id, ".L_" + std::to_string(max_block_id));
      block->address = function_size + function->address;
      block->begin_offset = cuda_arch >= 70 ? 16 : 8;
      max_block_id++;
      while (function_size < symbol_size) {
        block->insts.push_back(new CudaParse::Instruction(function_size + function->address, function_size));
        function_size += len;
      } 
      if (function->blocks.size() > 0) {
        auto *last_block = function->blocks.back();
        last_block->targets.push_back(
          new CudaParse::Target(last_block->insts.back(), block, CudaParse::TargetType::DIRECT));
      }
      function->blocks.push_back(block);
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
}


static void
dumpCudaInstructions
(
 const std::string &search_path,
 const std::string &elf_filename,
 const std::vector<CudaParse::Function *> &functions
)
{
  // Create a nvidia directory and dump instruction files
  if (functions.size() > 0) {
    const std::string dot_dir = search_path + "/nvidia";
    int ret = mkdir(dot_dir.c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH);
    if (ret != 0 && errno != EEXIST) {
      std::cout << "WARNING: failed to mkdir: " << dot_dir << std::endl;
      return;
    }

    const std::string inst_output = search_path + "/nvidia/" + FileUtil::basename(elf_filename) + ".inst";
    if (CudaParse::dumpCudaInstructions(inst_output, functions) != true) {
      std::cout << "WARNING: failed to dump static database file: " << inst_output << std::endl;
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
readCubinCFG
(
 const std::string &search_path,
 ElfFile *elfFile,
 Dyninst::SymtabAPI::Symtab *the_symtab, 
 bool cfg_wanted,
 Dyninst::ParseAPI::CodeSource **code_src, 
 Dyninst::ParseAPI::CodeObject **code_obj,
 bool dump_insts,
 bool control,
 bool slice,
 bool liveness
) 
{
  static bool compute_cfg = cfg_wanted && test_nvdisasm();
  bool dump_cubin_success = false;

  if (compute_cfg) {
    std::string filename = getFilename();
    std::string cubin = filename;
    std::string dot = filename + ".dot";

    dump_cubin_success = dumpCubin(cubin, elfFile);
    if (!dump_cubin_success) {
      std::cout << "WARNING: unable to write a cubin to the file system to analyze its CFG" << std::endl; 
    } else {
      // Get raw dot functions
      std::vector<CudaParse::Function *> functions;
      std::vector<int> symbol_visibility = getVisibilityCubin(elfFile->getMemoryOriginal(), elfFile->getElf());
      parseDotCFG(dot, cubin, elfFile->getArch(), the_symtab, functions, symbol_visibility);

      // Dyninst adapters
      // First construct dyninst functions, blocks, and instructions
      // Then parse slice and liveness, and update instruction_stats
      CFGFactory *cfg_fact = new CudaCFGFactory(functions);
      *code_src = new CudaCodeSource(functions, the_symtab); 
      *code_obj = new CodeObject(*code_src, cfg_fact);
      (*code_obj)->parse();

      if (control) {
        controlCudaInstructions(elfFile->getMemoryOriginal(), functions);
      }

      if (slice) {
        sliceCudaInstructions((*code_obj)->funcs(), functions);
      }

      if (liveness) {
        processLivenessCudaInstructions((*code_obj)->funcs(), functions);
      }

      if (dump_insts) {
        dumpCudaInstructions(search_path, elfFile->getFileName(), functions);
      }

      unlink(dot.c_str());
      unlink(cubin.c_str());
      return true;
    }
  }

  *code_src = new SymtabCodeSource(the_symtab);
  *code_obj = new CodeObject(*code_src, NULL, NULL, false, true);

  return false;
}
