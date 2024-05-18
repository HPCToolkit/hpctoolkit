// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************
//
// File: InputFile.hpp
//
// Purpose:
//   Open a file and return a vector that will contain a pointer to the
//   Elf representation of that file as well pointers to any Elf files
//   nested inside.
//
//***************************************************************************


#ifndef __InputFile_hpp__
#define __InputFile_hpp__

//******************************************************************************
// system includes
//******************************************************************************

#include <string>



//******************************************************************************
// forward declarations
//******************************************************************************

class ElfFileVector;



//******************************************************************************
// type definitions
//******************************************************************************

typedef enum InputFileErrorType_t {
  InputFileError_WarningNothrow,
  InputFileError_Warning,
  InputFileError_Error
} InputFileErrorType_t;


class InputFile {
public:
  InputFile() { filevector = 0; }
  ~InputFile();
  bool openFile(std::string &filename, InputFileErrorType_t errType);

  std::string &fileName() { return filename; }
  const char *CfileName() { return filename.c_str(); }
  ElfFileVector *fileVector() { return filevector; }
private:
  std::string filename;
  ElfFileVector *filevector;
};

#endif
