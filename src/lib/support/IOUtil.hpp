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

//****************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
// 
// Author:
//   Nathan Tallent
//
//****************************************************************************

#ifndef support_IOUtil_hpp 
#define support_IOUtil_hpp

//************************** System Include Files ****************************

#include <iostream>
#include <fstream>
#include <string>

//*************************** User Include Files *****************************

//************************** Forward Declarations ****************************

//****************************************************************************

//****************************************************************************
// IOUtil
//****************************************************************************

namespace IOUtil {

// Open either the file 'filenm' or std::cin (if 'filenm' is
// NULL). Throws an exception on failure.
std::istream*
OpenIStream(const char* filenm);

// Open either the file 'filenm' or std::cout (if 'filenm' is
// NULL). Throws an exception on failure.
std::ostream*
OpenOStream(const char* filenm);

// Close the stream returned by the above functions
void
CloseStream(std::istream* s);
void
CloseStream(std::ostream* s);
void
CloseStream(std::iostream* s);

void 
OpenIFile(std::ifstream& fs, const char* filenm);

void 
OpenOFile(std::ofstream& fs, const char* filenm);

void
CloseFile(std::fstream& fs);


// just like std::get and std::getline, except is not limited by a
// fixed input buffer size
std::string 
Get(std::istream& is, char end = '\n');

std::string 
GetLine(std::istream& is, char end = '\n');

// skips the specified string; returns false if there is a
// deviation between 's' and what is read; true otherwise
bool 
Skip(std::istream& is, const char* s);

inline bool 
Skip(std::istream& is, const std::string& s)
{ return Skip(is, s.c_str()); }


} // end of IOUtil namespace


#endif // support_IOUtil_hpp
