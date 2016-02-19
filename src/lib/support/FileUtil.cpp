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
// Copyright ((c)) 2002-2016, Rice University
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
#include <cstdio>  // for 'tmpnam', 'rename'
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

#include "FileUtil.hpp"

#include "diagnostics.h"
#include "StrUtil.hpp"
#include "Trace.hpp"

#include <lib/support-lean/OSUtil.h>

//*************************** Forward Declarations **************************

using std::endl;

//***************************************************************************
//
//***************************************************************************

namespace FileUtil {

string
basename(const char* fName)
{
  string baseFileName;

  const char* lastSlash = strrchr(fName, '/');
  if (lastSlash) {
    // valid: "/foo" || ".../foo" AND invalid: "/" || ".../"
    baseFileName = lastSlash + 1;
  }
  else {
    // filename contains no slashes, already in short form
    baseFileName = fName;
  }
  return baseFileName;

#if 0
  // A "more C++" implementation (Gaurav)
  std::string
  PathFindMgr::getFileName(const std::string& path) const
  {
    size_t in = path.find_last_of("/");
    if (in != path.npos && path.length() > 1)
      return path.substr(in + 1);
    
    return path;
  }
#endif
}


string
rmSuffix(const char* fName)
{
  string baseFileName = fName;

  size_t pos = baseFileName.find_last_of('.');
  if (pos != string::npos) {
    baseFileName = baseFileName.substr(0, pos);
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
	const char* string, int flags)
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


//***************************************************************************
//
//***************************************************************************

namespace FileUtil {

bool
isReadable(const char* path)
{
  struct stat sbuf;
  if (stat(path, &sbuf) == 0) {
    return true; // the file is readable
  }
  return false;
}


bool
isDir(const char* path)
{
  struct stat sbuf;
  if (stat(path, &sbuf) == 0) {
    return (S_ISDIR(sbuf.st_mode)
	    /*|| S_ISLNK(sbuf.st_mode) && isDir(readlink(path))*/);
  }
  return false; // unknown
}


int
countChar(const char* path, char c)
{
  int srcFd = open(path, O_RDONLY); 
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

} // end of FileUtil namespace


//***************************************************************************
//
//***************************************************************************

static void
cpy(int srcFd, int dstFd)
{
  static const int bufSz = 4096;
  char buf[bufSz];
  ssize_t nRead;
  while ((nRead = read(srcFd, buf, bufSz)) > 0) {
    write(dstFd, buf, nRead);
  }
}


namespace FileUtil {

void
copy(const char* dst, ...)
{
  va_list srcFnmList;
  va_start(srcFnmList, dst);
  
  DIAG_MsgIf(0, "FileUtil::copy: ... -> " << dst);

  int dstFd = open(dst, O_WRONLY | O_CREAT | O_TRUNC,
		   S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (dstFd < 0) {
    DIAG_Throw("[FileUtil::copy] could not open destination file '"
	       << dst << "' (" << strerror(errno) << ")");
  }

  string errorMsg;

  char* srcFnm;
  while ( (srcFnm = va_arg(srcFnmList, char*)) ) {
    int srcFd = open(srcFnm, O_RDONLY);
    if ((srcFd < 0) || (dstFd < 0)) {
      errorMsg += (string("could not open '") + srcFnm + "' (" 
		   + strerror(errno) + ")");
    }
    else {
      cpy(srcFd, dstFd);
      close(srcFd);
    }
  }

  va_end(srcFnmList);
  close(dstFd);

  if (!errorMsg.empty()) {
    DIAG_Throw("[FileUtil::copy] could not open source files: " << errorMsg);
  }
}


void
move(const char* dst, const char* src)
{
  int ret = rename(src, dst);
  if (ret != 0) {
    DIAG_Throw("[FileUtil::move] '" << src << "' -> '" << dst << "'");
  }
}


int
remove(const char* file)
{ 
  return unlink(file);
}


int
mkdir(const char* dir)
{
  if (!dir) {
    DIAG_Throw("Invalid mkdir argument: (NULL)");
  }

  string pathStr = dir;
  bool isAbsPath = (dir[0] == '/');

  // -------------------------------------------------------
  // 1. Convert path string to vector of path components
  //    "/p0/p1/p2/.../pn/px" ==> [p0 p1 p2 ... pn px]
  //    "/p0"                 ==> [p0]
  //    "p0"                  ==> [p0]
  //    "./p0"                ==> [. p0]
  //
  // Note: we could do tokenization in place (string::find_last_of()),
  // but (1) this is more elegant and (2) sytem calls and disk
  // accesses will overwhelm any possible difference in performance.
  // -------------------------------------------------------
  std::vector<string> pathVec;
  StrUtil::tokenize_char(pathStr, "/", pathVec);

  DIAG_Assert(!pathVec.empty(), DIAG_UnexpectedInput);

  // -------------------------------------------------------
  // 2. Find 'curIdx' such that all paths before pathVec[curIdx] have
  // been created.
  //
  // Note: Start search from the last path component, assuming that in
  // the common case, intermediate directories are already created.
  //
  // Note: Could make this a binary search, but it would likely have
  // insignificant effects.
  // -------------------------------------------------------
  size_t begIdx = 0;
  size_t endIdx = pathVec.size() - 1;

  size_t curIdx = endIdx;
  for ( ; curIdx >= begIdx; --curIdx) {
    string x = StrUtil::join(pathVec, "/", 0, curIdx + 1);
    if (isAbsPath) {
      x = "/" + x;
    }
    
    if (isDir(x)) {
      break; // FIXME: double check: what if this is a symlink?
    }
  }

  curIdx++;

  // -------------------------------------------------------
  // 3. Build directories from pathVec[curIdx ... endIdx]
  // -------------------------------------------------------
  mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;

  for ( ; curIdx <= endIdx; ++curIdx) {
    string x = StrUtil::join(pathVec, "/", 0, curIdx + 1);
    if (isAbsPath) {
      x = "/" + x;
    }

    int ret = ::mkdir(x.c_str(), mode);
    if (ret != 0) {
      DIAG_Throw("[FileUtil::mkdir] '" << pathStr << "': Could not mkdir '"
		 << x << "' (" << strerror(errno) << ")");
    }
  }

  return 0;
}


std::pair<string, bool>
mkdirUnique(const char* dirnm)
{
  string dirnm_new = dirnm;
  bool is_done = false;

  mode_t mkmode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;

  int ret = ::mkdir(dirnm, mkmode);
  if (ret != 0) {
    if (errno == EEXIST) {

      std::vector<string> dirnmVec;

      // qualifier 1: jobid
      const char* jobid_cstr = OSUtil_jobid();
      if (jobid_cstr) {
	string dirnm1 = string(dirnm) + "-" + string(jobid_cstr);
	dirnmVec.push_back(dirnm1);
      }

      // qualifier 2: pid
      uint pid = OSUtil_pid();
      string pid_str = StrUtil::toStr(pid);
      string dirnm2 = string(dirnm) + "-" + pid_str;
      dirnmVec.push_back(dirnm2);

      // attempt to create alternative directories
      for (uint i = 0; i < dirnmVec.size(); ++i) {
	dirnm_new = dirnmVec[i];
	DIAG_Msg(1, "Directory '" << dirnm << "' already exists. Trying '" << dirnm_new << "'");
	ret = ::mkdir(dirnm_new.c_str(), mkmode);
	if (ret == 0) {
	  is_done = true;
	  break;
	}
      }
      
      if (is_done) {
	DIAG_Msg(1, "Created directory: " << dirnm_new);
      }
      else {
	DIAG_Die("Could not create an alternative to directory " << dirnm);
      }
    }
    else {
      DIAG_Die("Could not create database directory " << dirnm);
    }
  }
  
  return make_pair(dirnm_new, is_done);
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


} // end of FileUtil namespace

