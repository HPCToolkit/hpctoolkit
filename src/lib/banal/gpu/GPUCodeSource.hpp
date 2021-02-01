#ifndef BANAL_GPU_GPU_CODE_SOURCE_H
#define BANAL_GPU_GPU_CODE_SOURCE_H

#include <dyn_regs.h>
#include <CodeSource.h>
#include <Symtab.h>

#include "DotCFG.hpp"

namespace Dyninst {
namespace ParseAPI {
  class PARSER_EXPORT GPUCodeSource : public /*Symtab */ CodeSource {
 public:
  GPUCodeSource(std::vector<GPUParse::Function *> &functions, 
		Dyninst::SymtabAPI::Symtab *s);
  ~GPUCodeSource() {}

 public:
  /** InstructionSource implementation **/
  virtual bool isValidAddress(const Address) const { return false; }
  virtual void* getPtrToInstruction(const Address) const { return NULL; }
  virtual void* getPtrToData(const Address) const { return NULL; }
  virtual unsigned int getAddressWidth() const { return 0; }
  virtual bool isCode(const Address) const { return false; }
  virtual bool isData(const Address) const { return false; }
  virtual bool isReadOnly(const Address) const { return false; }
  virtual Address offset() const { return 0; }
  virtual Address length() const { return 0; }
  virtual Architecture getArch() const { return Arch_cuda; }

  virtual bool nonReturning(Address /*func_entry*/) { return false; }                                                                                  
	virtual bool nonReturningSyscall(int /*number*/) { return false; }

	/* If the binary file type supplies per-function
	 * TOC's (e.g. ppc64 Linux), override.
	 */
  virtual Address getTOC(Address) const { return _table_of_contents; }

  // statistics accessor
  virtual void print_stats() const { return; }                                                                                                         
  virtual bool have_stats() const { return false; }

  // manage statistics
  virtual void incrementCounter(const std::string& /*name*/) const { return; } 
  virtual void addCounter(const std::string& /*name*/, int /*num*/) const { return; }
  virtual void decrementCounter(const std::string& /*name*/) const { return; }
  virtual void startTimer(const std::string& /*name*/) const { return; } 
  virtual void stopTimer(const std::string& /*name*/) const { return; }
  virtual bool findCatchBlockByTryRange(Address /*given try address*/, std::set<Address> & /* catch start */)  const { return false; }
};

}
}

#endif
