// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************
//
// File:
//
// Purpose:
//
// Description:
//
//
//***************************************************************************


//******************************************************************************
// system includes
//******************************************************************************

#include <sys/types.h>
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>


//******************************************************************************
// local includes
//******************************************************************************

#include "../common/diagnostics.h"

#include "ElfHelper.hpp"
#include "Fatbin.hpp"
#include "intel/IntelGPUBinutils.hpp"
#include "InputFile.hpp"



//******************************************************************************
// private operations
//******************************************************************************

static size_t
file_size(int fd)
{
  struct stat sb;
  int retval = fstat(fd, &sb);
  if (retval == 0 && S_ISREG(sb.st_mode)) {
    return sb.st_size;
  }
  return 0;
}


// Automatically restart short reads.
// This protects against EINTR.
//
static size_t
read_all(int fd, void *buf, size_t count)
{
  ssize_t ret;
  size_t len;

  len = 0;
  while (len < count) {
    ret = read(fd, ((char *) buf) + len, count - len);
    if (ret == 0 || (ret < 0 && errno != EINTR)) {
      break;
    }
    if (ret > 0) {
      len += ret;
    }
  }

  return len;
}


//******************************************************************************
// interface operations
//******************************************************************************


bool
InputFile::openFile
(
 std::string &filename,
 InputFileErrorType_t errType
)
{
  const char *tag =
    (errType == InputFileError_Error) ? "ERROR: " : "WARNING: ";

  this->filename = filename;

  int    file_fd = open(filename.c_str(), O_RDONLY);

  if (file_fd < 0) {
    DIAG_MsgIf_GENERIC(tag, 1, "Unable to open input file: "
           << filename << " (" << strerror(errno) << ")");

    if (errType != InputFileError_WarningNothrow) throw 1;

    return false;
  }

  size_t f_size = file_size(file_fd);

  if (f_size == 0) {
    DIAG_MsgIf_GENERIC(tag, 1, "Empty input file " << filename);

    if (errType != InputFileError_WarningNothrow) throw 1;

    return false;
  }

  char  *file_buffer = (char *) malloc(f_size);

  if (file_buffer == 0) {
    DIAG_MsgIf_GENERIC(tag, 1, "Unable to allocate file buffer of "
           << f_size << " bytes");
    if (errType != InputFileError_WarningNothrow) throw 1;

    return false;
  }

  size_t bytes = read_all(file_fd, file_buffer, f_size);

  if (f_size != bytes) {
    DIAG_MsgIf_GENERIC(tag, 1, "Read only " << bytes << " bytes of "
           << f_size << " bytes from file " << filename);

    if (errType != InputFileError_WarningNothrow) throw 1;

    return false;
  }

  close(file_fd);

  filevector = new ElfFileVector;
  ElfFile *elfFile = new ElfFile;

  bool result = elfFile->open(file_buffer, f_size, filename);
  if (result) {
    filevector->push_back(elfFile);
    //findCubins(elfFile, filevector);
  }
  #ifdef ENABLE_IGC
  else if (!findIntelGPUBins(filename, file_buffer, f_size, filevector)) { // Check if the file is a intel debug binary
    // Release memory
    delete(elfFile);
    DIAG_MsgIf_GENERIC(tag, 1, "Not an ELF binary " << filename);
    if (errType != InputFileError_WarningNothrow) throw 1;
    // Not a standard elf file
    return false;
  }
  #endif // ENABLE_IGC

  return result;
}


InputFile::~InputFile
(
 void
)
{
  if (filevector) {
    for(auto fi = filevector->begin(); fi < filevector->end(); fi++) {
      delete *fi;
    }
    delete filevector;
  }
}
