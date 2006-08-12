// -*-Mode: C++;-*-
// $Id$

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

#ifndef Files_h
#define Files_h 

//************************* System Include Files ****************************

#include <string>

//*************************** User Include Files ****************************

//*************************** Forward Declarations ***************************

//****************************************************************************

// ... is a NULL terminated list of file names 
// CopyFile appends these files into destFile 
//          returns NULL upon success
//          otherwise returns an error message in a static variable 
//          which is overwritten with each call to CopyFile
extern const char* CopyFile(const char* destFile, ...); 


extern int MakeDir(const char* dir);


// retuns a name that can safely be used for a temporary file 
// in a static variable, which is overwritten with each call to 
// TmpFileName
extern const char* TmpFileName(); 


// count how often char appears in file
// return that number or -1 upon failure to open file for reading
extern int CountChar(const char* file, char c); 


// deletes fname (unlink) 
extern int DeleteFile(const char *fname);


// 'BaseFileName': returns the 'fname.ext' component of fname=/path/fname.ext
extern std::string BaseFileName(const char* fname); 

inline std::string 
BaseFileName(const std::string& fname)
{
  return BaseFileName(fname.c_str());
}


// 'PathComponent': returns the '/path' component of fname=/path/fname.ext
extern std::string PathComponent(const char* fname); 

inline std::string 
PathComponent(const std::string& fname)
{
  return PathComponent(fname.c_str());
}

#endif 
