#ifndef Kernel_hpp

#include <string>

class KernelSymbols {
public:
  KernelSymbols();
  bool parseLinuxKernelSymbols(std::string &filename);
  bool find(uint64_t vma, std::string &fnname);
  void dump();
private:
  struct KernelSymbolsRepr *R;
};

#endif
