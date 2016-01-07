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

#ifndef support_FileUtil_hpp
#define support_FileUtil_hpp 

//************************* System Include Files ****************************

#include <string>
#include <vector>

#include <fnmatch.h>

//*************************** User Include Files ****************************

//*************************** Forward Declarations ***************************

//****************************************************************************

namespace FileUtil {

// ---------------------------------------------------------
// file names
// ---------------------------------------------------------

// 'basename': returns the 'fname.ext' component of '/path/fname.ext'
extern std::string
basename(const char* fname);

inline std::string
basename(const std::string& fname)
{
  return basename(fname.c_str());
}


// 'rmSuffix': returns the 'fname' component of 'fname.ext'
extern std::string
rmSuffix(const char* fname);

inline std::string
rmSuffix(const std::string& fname)
{
  return rmSuffix(fname.c_str());
}


// 'dirname': returns the '/path' component of "/path/fname.ext"
extern std::string
dirname(const char* fname); 

inline std::string
dirname(const std::string& fname)
{
  return dirname(fname.c_str());
}


static inline bool
fnmatch(const std::string pattern, const char* string, int flags = 0)
{
  int fnd = ::fnmatch(pattern.c_str(), string, flags);
  return (fnd == 0);
#if 0
  if (fnd == 0) {
    return true;
  }
  else if (fnd != FNM_NOMATCH) {
    // error
  }
#endif
}


bool
fnmatch(const std::vector<std::string>& patternVec, 
	const char* string, 
	int flags = 0);
  

// ---------------------------------------------------------
// file tests
// ---------------------------------------------------------

extern bool
isReadable(const char* path);

inline bool
isReadable(const std::string& path)
{
  return isReadable(path.c_str());
}


bool
isDir(const char* path);

inline bool
isDir(const std::string& path)
{
  return isDir(path.c_str());
}


// count how often char appears in file
// return that number or -1 upon failure to open file for reading
extern int
countChar(const char* file, char c);


// ---------------------------------------------------------
// file operations
// ---------------------------------------------------------

// copy: takes a NULL terminated list of file name and appends these
// files into destFile.
extern void
copy(const char* destFile, ...);

inline void
copy(const std::string& dst, const std::string& src)
{
  copy(dst.c_str(), src.c_str(), NULL);
}


void
move(const char* dst, const char* src);

inline void
move(const std::string& dst, const std::string& src)
{
  move(dst.c_str(), src.c_str());
}



// deletes fname (unlink) 
extern int
remove(const char* fname);


// mkdir: makes 'dir' (including all intermediate directories)
extern int
mkdir(const char* dir);

inline void
mkdir(const std::string& dir)
{
  FileUtil::mkdir(dir.c_str());
}


// mkdirUnique: 
std::pair<std::string, bool>
mkdirUnique(const char* dirnm);

inline std::pair<std::string, bool>
mkdirUnique(const std::string& dirnm)
{
  return mkdirUnique(dirnm.c_str());
}


// retuns a name that can safely be used for a temporary file 
// in a static variable, which is overwritten with each call to 
// tmpname
extern const char*
tmpname();


} // end of FileUtil namespace


#endif // support_FileUtil_hpp
