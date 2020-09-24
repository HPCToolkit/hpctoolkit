// -*-Mode: C++;-*-

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

// This file defines the API for writing an hpcstruct file directly
// from the TreeNode format.

//***************************************************************************

#ifndef Banal_Struct_Output_hpp
#define Banal_Struct_Output_hpp

#include <ostream>
#include <string>

#include <lib/support/StringTable.hpp>

#include "Struct-Inline.hpp"
#include "Struct-Skel.hpp"

namespace BAnal {
namespace Output {

using namespace Struct;
using namespace std;

void printStructFileBegin(ostream *, ostream *, string);
void printStructFileEnd(ostream *, ostream *);

void printLoadModuleBegin(ostream *, string);
void printLoadModuleEnd(ostream *);

void printFileBegin(ostream *, FileInfo *);
void printFileEnd(ostream *, FileInfo *);

void printProc(ostream *, ostream *, string, FileInfo *, GroupInfo *,
	       ProcInfo *, HPC::StringTable & strTab);

void printBlockAndInstructionOffset(ostream * os, string file_name);

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

}  // namespace Output
}  // namespace BAnal

#endif
