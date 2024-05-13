// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

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
