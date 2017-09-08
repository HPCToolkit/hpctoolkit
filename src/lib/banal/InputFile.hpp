
#ifndef __InputFile_hpp__
#define __InputFile_hpp__

#include <string>
#include <libelf.h>

class ElfFile {
public:
  ElfFile(char *_memPtr, size_t _memLen, std::string _fileName);
  ~ElfFile();
  Elf *getElf() { return elf; };
  char *getMemory() { return memPtr; };
  size_t getLength() { return memLen; };
  std::string getFileName() { return fileName; };
private:
  char *memPtr;
  size_t memLen;
  Elf *elf;
  std::string fileName;
};


class InputFile {
public:
  ElfFile *openFile(std::string filename);
};

#endif
