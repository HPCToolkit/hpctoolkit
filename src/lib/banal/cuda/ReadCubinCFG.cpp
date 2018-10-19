
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


#include <CodeSource.h>
#include <CodeObject.h>
#include <CFGFactory.h>


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
using namespace std;


static bool
test_nvdisasm() 
{
  // check whether nvdisasm works
  int retval = system("nvdisasm > /dev/null") == 0;
  if (!retval) {
     cout << "WARNING: nvdisasm is not available on your path to analyze control flow in NVIDIA CUBINs" << endl;
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
 const std::string &dot
) 
{
  std::string cmd = "nvdisasm -cfg -poff " + cubin + " > " + dot;
#if 0
  FILE *output = popen(cmd.c_str(), "r");
  if (!output) {
    cout << "Dump " << dot << " to disk failed" << endl; 
    return false;
  }
  pclose(output);
  return true;
#endif
  return system(cmd.c_str()) == 0; 
}


static void
parseDotCFG
(
 const std::string &filename, 
 std::vector<CudaParse::Function *> &functions
) 
{
  CudaParse::GraphReader graph_reader(filename);
  CudaParse::Graph graph;
  graph_reader.read(graph);
  CudaParse::CFGParser cfg_parser;
  cfg_parser.parse(graph, functions);
}


static void
relocateFunctions
(
 Dyninst::SymtabAPI::Symtab *the_symtab, 
 std::vector<CudaParse::Function *> &functions
) 
{
  std::vector<Symbol *> symbols;
  the_symtab->getAllSymbols(symbols);
  for (auto *symbol : symbols) {
    for (auto *function : functions) {
      if (function->name == symbol->getMangledName()) {
        auto begin_offset = function->begin_offset;
        for (auto *block : function->blocks) {
          for (auto *inst : block->insts) {
            inst->offset = (inst->offset - begin_offset) + symbol->getOffset();
          }
        }
        function->address = symbol->getOffset();
      }
    }
  }
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

    dump_cubin_success = dumpCubin(cubin, elfFile) ? true : false;
    if (!dump_cubin_success) {
      cout << "WARNING: unable to write a cubin to the file system to analyze its CFG" << endl; 
    } else {
      dump_dot_success = dumpDot(cubin, dot) ? true : false;
      if (!dump_dot_success) {
        cout << "WARNING: unable to use nvdisasm to produce a CFG for a cubin" << endl; 
      } else {
        // Parse dot cfg
        // relocate instructions according to the 
        // relocated symbols in the_symtab
        std::vector<CudaParse::Function *> functions;
        parseDotCFG(dot, functions);
        relocateFunctions(the_symtab, functions);

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
//#if 1
//  for (auto *function : functions) {
//    cout << "cuda function: " << function->name << " " << 
//      std::hex << (void *) function << std::dec << endl;
//    delete function;
//  }
//#endif

  return false;
}
