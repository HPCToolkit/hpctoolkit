// $Id$
// -*-C++-*-
// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002, Rice University 
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

//************************* System Include Files ****************************

#include <sys/param.h>

#ifdef NO_STD_CHEADERS
# include <stdlib.h>
# include <stdio.h>
# include <errno.h>
# include <stdarg.h>
#else
# include <cstdlib> // for 'mkstemp' (not technically visible in C++)
# include <cstdio>  // for 'tmpnam'
# include <cerrno>
# include <cstdarg>
using namespace std; // For compatibility with non-std C headers
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

//*************************** User Include Files ****************************

#include "Files.h"
#include "String.h"
#include "Trace.h"

//*************************** Forward Declarations **************************

using std::endl;

//***************************************************************************

static void
cpy(int srcFd, int dstFd) 
{
  char buf[256]; 
  ssize_t nRead; 
  while ((nRead = read(srcFd, buf, 256)) > 0) {
    write(dstFd, buf, nRead); 
  } 
} 

const char* 
CopyFile(const char* destFile, ...) 
{
  static String error; 
  error = ""; 
  va_list srcFiles;
  va_start(srcFiles, destFile);

  IFTRACE << "CopyFile: destFile = " << destFile ; 
  int dstFd = open(destFile, O_WRONLY | O_CREAT | O_TRUNC, 
		    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); 
  if (dstFd < 0) {
    error = String("Could not open ") + destFile + ": " + strerror(errno); 
    IFTRACE << " Error: " << error << endl; 
    return error; 
  } 
  
  char* srcFile; 
  while ( (srcFile = va_arg(srcFiles, char*)) ) {
    int srcFd = open(srcFile, O_RDONLY); 
    if ((srcFd < 0) || (dstFd < 0)) {
      error = String("Could not open ") + srcFile + ": " + strerror(errno); 
    } else { 
      IFTRACE << " " << srcFile; 
      cpy(srcFd, dstFd); 
      close(srcFd); 
    }
  } 
  IFTRACE << endl;
  close(dstFd); 
  if (error.Length() > 0) {
    return error; 
  } else {
    return NULL; 
  } 
} 

int 
CountChar(const char* file, char c) 
{
  int srcFd = open(file, O_RDONLY); 
  if (srcFd < 0) {
    return -1; 
  } 
  int count = 0;
  char buf[256]; 
  ssize_t nRead; 
  while ((nRead = read(srcFd, buf, 256)) > 0) {
    for (int i = 0; i < nRead; i++) {
      if (buf[i] == c) count++; 
    }
  }
  return count; 
} 

const char* 
TmpFileName()   
{
  // below is a hack to replace the deprecated tmpnam which g++ 3.2.2 will
  // no longer allow. the mkstemp routine, which is touted as the replacement
  // for tmpnam, provides a file descriptor as a return value. there is
  // unfortunately no way to interface this with the ofstream class constructor
  // which requires a filename. thus, a hack is born ...
  // John Mellor-Crummey 5/7/2003
  
  // eraxxon: GNU is right that 'tmpnam' can be dangerous, but
  // 'mkstemp' is not strictly part of C++! We could create an
  // interface to 'mkstemp' within a C file, but this is getting
  // cumbersome... and 'tmpnam' is not quite a WMD.

#ifdef __GNUC__
  static char tmpfilename[MAXPATHLEN];

  // creating a unique temp name with the new mkstemp interface now 
  // requires opening, closing, and deleting a file when all we want
  // is the filename. sigh ...
  strcpy(tmpfilename,"/tmp/hpcviewtmpXXXXXX");
  close(mkstemp(tmpfilename));
  unlink(tmpfilename);

  return tmpfilename;
#else
  return tmpnam(NULL); 
#endif
}

int
DeleteFile(const char* file) 
{ 
  return unlink(file); 
}

String 
BaseFileName(const char* fName) 
{
  const char* lastSlash = strrchr(fName, '/'); 
  String baseFileName;
  
  if (lastSlash) {
    // valid: "/foo" || ".../foo" AND invalid: "/" || ".../" 
    baseFileName = lastSlash + 1; 
  } else {
    // filename contains no slashes, already in short form 
    baseFileName = fName; 
  } 
  return baseFileName; 
} 

String 
PathComponent(const char* fName) 
{
  const char* lastSlash = strrchr(fName, '/'); 
  String pathComponent = "."; 
  if (lastSlash) {
    pathComponent = fName; 
    pathComponent.Resize(lastSlash - fName); 
  }
  return pathComponent; 
}

