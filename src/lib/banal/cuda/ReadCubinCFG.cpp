
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
relocateInstructions
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
        for (auto *block : function->blocks) {
          for (auto *inst : block->insts) {
            inst->offset += symbol->getOffset();
          }
        }
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
  static bool nvdisasm_usable =  test_nvdisasm();
  
  if (!nvdisasm_usable) return false; 

  std::string filename = "tmp";

  std::string cubin = filename;
  if (!dumpCubin(cubin, elfFile)) {
    cout << "WARNING: unable to write a cubin to the file system to analyze its CFG" << endl; 
    return false;
  }

  std::string dot = filename + ".dot";
  if (!dumpDot(cubin, dot)) {
    cout << "WARNING: unable to use nvdisasm to produce a CFG for a cubin" << endl; 
    return false;
  }

  // Parse dot cfg
  std::vector<CudaParse::Function *> functions;
  parseDotCFG(dot, functions);

  // relocate instructions according to the 
  // relocated symbols in the_symtab
  relocateInstructions(the_symtab, functions);

  CFGFactory *cfg_fact = new CudaCFGFactory(functions);

  if (cfg_fact != NULL) {

    *code_src = new CudaCodeSource(functions); 

    *code_obj = new CodeObject(*code_src, cfg_fact);
    (*code_obj)->parse();

    delete cfg_fact;
  }

  for (auto *function : functions) {
    delete function;
  }

  return true;
}
