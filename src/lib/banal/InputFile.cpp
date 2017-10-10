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
// Copyright ((c)) 2002-2017, Rice University
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

#include <err.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "InputFile.hpp"
#include "Fatbin.hpp"


//******************************************************************************
// private operations
//******************************************************************************

int file_size(int fd)
{
  struct stat sb;
  int retval = fstat(fd, &sb);
  if (retval == 0 && S_ISREG(sb.st_mode)) {
    return sb.st_size;
  }
  return 0;
}



//******************************************************************************
// interface oeprations
//******************************************************************************


ElfFileVector *
InputFile::openFile
(
 std::string filename
)
{
  const char *file_name = filename.c_str();

  int    file_fd = open(file_name, O_RDONLY);

  if (file_fd < 0) {
    errx(1, "open failed: %s", file_name);
  }

  size_t f_size = file_size(file_fd);
  
  if (f_size == 0) {
    errx(1, "file size == 0: %s", file_name);
  }

  char  *file_buffer = (char *) malloc(f_size);

  if (file_buffer == 0) {
    errx(1, "unable to allocate file buffer of %lu bytes", f_size);
  }

  size_t bytes = read(file_fd, file_buffer, f_size);

  if (f_size != bytes) {
    errx(1, "read only %lu bytes of %lu bytes from file %s",
	 bytes, f_size, file_name);
  }

  close(file_fd);

  ElfFileVector *elfFileVector = 0;
  ElfFile *elfFile = new ElfFile;
  if (elfFile->open(file_buffer, f_size, filename)) {
    elfFileVector = new ElfFileVector;
    elfFileVector->push_back(elfFile);
    findCubins(elfFile, elfFileVector);
  } else {
    delete elfFile;
  }

  return elfFileVector;
}
