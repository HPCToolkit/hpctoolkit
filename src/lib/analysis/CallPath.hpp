// -*-Mode: C++;-*-
// $Id$

// * BeginRiceCopyright *****************************************************
// 
// Copyright ((c)) 2002-2007, Rice University 
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
//   $Source$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef Analysis_CallPath_CallPath_hpp 
#define Analysis_CallPath_CallPath_hpp

//************************* System Include Files ****************************

#include <iostream>
#include <vector>
#include <stack>
#include <string>

//*************************** User Include Files ****************************

#include <include/general.h> 

#include <lib/binutils/LM.hpp>

#include <lib/prof-juicy/CallPath-Profile.hpp>

//*************************** Forward Declarations ***************************

//****************************************************************************

namespace Analysis {

namespace CallPath {

void 
inferCallFrames(Prof::CallPath::Profile* prof, Prof::Epoch::LM* epoch_lm,
		Prof::Struct::LM* lmStrct);

void 
inferCallFrames(Prof::CallPath::Profile* prof, Prof::Epoch::LM* epoch_lm,
		binutils::LM* lm);

bool 
normalize(Prof::CallPath::Profile* prof, string lush_agent);

void 
writeInDatabase(Prof::CallPath::Profile* prof, const std::string& filenm);

void 
write(Prof::CallPath::Profile* prof, std::ostream& os, bool prettyPrint = true);

void 
copySourceFiles(Prof::CallPath::Profile *prof, 
		std::vector<std::string>& searchPaths,
		const std::string& dest_dir);  

} // namespace CallPath

} // namespace Analysis

//****************************************************************************

// FIXME: move to lib/support
#define MAX_PATH_SIZE 2048 
/** Normalizes a file path.*/
std::string normalizeFilePath(const std::string& filePath);
std::string normalizeFilePath(const std::string& filePath, 
			      std::stack<std::string>& pathSegmentsStack);
void breakPathIntoSegments(const std::string& normFilePath, 
			   std::stack<std::string>& pathSegmentsStack);

//****************************************************************************

#endif // Analysis_CallPath_CallPath_hpp
