#ifndef Kernel_hpp

#include <string>
#include <string>

class KernelSymbols {
public:
  KernelSymbols();
  bool parseLinuxKernelSymbols();
  bool find(uint64_t vma, std::string &fnname);
  void dump();
private:
  struct KernelSymbolsRepr *R;
};

#endif
