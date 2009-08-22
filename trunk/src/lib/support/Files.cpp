// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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

#include <cstdlib> // for 'mkstemp' (not technically visible in C++)
#include <cstdio>  // for 'tmpnam'
#include <cerrno>
#include <cstdarg>
#include <cstring>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <fnmatch.h>

#include <string>
using std::string;

//*************************** User Include Files ****************************

#include "Files.hpp"

#include "diagnostics.h"
#include "StrUtil.hpp"
#include "Trace.hpp"

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


namespace FileUtil {

const char* 
CopyFile(const char* destFile, ...) 
{
  static string error; 
  error = ""; 
  va_list srcFiles;
  va_start(srcFiles, destFile);

  IFTRACE << "CopyFile: destFile = " << destFile ; 
  int dstFd = open(destFile, O_WRONLY | O_CREAT | O_TRUNC, 
		    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); 
  if (dstFd < 0) {
    error = string("Could not open ") + destFile + ": " + strerror(errno); 
    IFTRACE << " Error: " << error << endl; 
    return error.c_str(); 
  } 
  
  char* srcFile; 
  while ( (srcFile = va_arg(srcFiles, char*)) ) {
    int srcFd = open(srcFile, O_RDONLY); 
    if ((srcFd < 0) || (dstFd < 0)) {
      error = string("Could not open ") + srcFile + ": " + strerror(errno); 
    } 
    else { 
      IFTRACE << " " << srcFile; 
      cpy(srcFd, dstFd); 
      close(srcFd); 
    }
  } 
  IFTRACE << endl;
  close(dstFd); 
  if (error.length() > 0) {
    return error.c_str(); 
  } 
  else {
    return NULL; 
  } 
} 


int 
mkdir(const char* dir)
{
  int ret = ::mkdir(dir, 00755); 
  return ret;
} 


std::pair<string, bool>
mkdirUnique(const char* dirnm)
{
  string dirnm_new = dirnm;
  bool is_done = false;

  mode_t mkmode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;

  int ret = ::mkdir(dirnm, mkmode);
  if (ret == -1) {
    if (errno == EEXIST) {
      // attempt to create dirnm+pid;
      pid_t pid = getpid();
      string pid_str = StrUtil::toStr(pid);
      dirnm_new = dirnm_new + "-" + pid_str;
      DIAG_Msg(1, "Directory '" << dirnm << "' already exists. Trying " << dirnm_new);
      ret = ::mkdir(dirnm_new.c_str(), mkmode);
      if (ret == -1) {
	DIAG_Die("Could not create alternate directory " << dirnm_new);
      }
      else {
	DIAG_Msg(1, "Created directory: " << dirnm_new);
	is_done = true;
      }
    } 
    else {
      DIAG_Die("Could not create database directory " << dirnm);
    }
  }
  
  return make_pair(dirnm_new, is_done);
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
tmpname()   
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


bool
isReadable(const char *fileName)
{
  bool result = false;
  struct stat sbuf;
  if (stat(fileName, &sbuf) == 0) {
    // the file is readable if the return code is OK
    result = true;
  }
  return result;
}


string 
basename(const char* fName) 
{
  const char* lastSlash = strrchr(fName, '/'); 
  string baseFileName;
  
  if (lastSlash) {
    // valid: "/foo" || ".../foo" AND invalid: "/" || ".../" 
    baseFileName = lastSlash + 1; 
  } 
  else {
    // filename contains no slashes, already in short form 
    baseFileName = fName; 
  } 
  return baseFileName; 
} 


string 
dirname(const char* fName) 
{
  const char* lastSlash = strrchr(fName, '/'); 
  string pathComponent = "."; 
  if (lastSlash) {
    pathComponent = fName; 
    pathComponent.resize(lastSlash - fName);
  }
  return pathComponent; 
}


bool 
fnmatch(const std::vector<std::string>& patternVec, 
	const char* string, 
	int flags)
{
  for (uint i = 0; i < patternVec.size(); ++i) {
    const std::string& pat = patternVec[i];
    bool fnd = FileUtil::fnmatch(pat, string, flags);
    if (fnd) {
      return true;
    }
  }

  return false;
}

} // end of FileUtil namespace
