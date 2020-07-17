// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2020, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

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

#include <lib/support/diagnostics.h>

#include "ElfHelper.hpp"
#include "Fatbin.hpp"
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
// interface oeprations
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

  ElfFile *elfFile = new ElfFile;
  bool result = elfFile->open(file_buffer, f_size, filename);

  if (result) {
    filevector = new ElfFileVector;
    filevector->push_back(elfFile);
    //findCubins(elfFile, filevector);
  } else {
    DIAG_MsgIf_GENERIC(tag, 1, "Not an ELF binary " << filename);

    if (errType != InputFileError_WarningNothrow) throw 1;

    return false;
  }

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
