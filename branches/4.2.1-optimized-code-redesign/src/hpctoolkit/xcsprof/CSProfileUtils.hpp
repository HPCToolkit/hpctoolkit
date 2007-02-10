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

#ifndef CSProfileUtils_hpp 
#define CSProfileUtils_hpp

//************************* System Include Files ****************************

#include <iostream>
#include <vector>
#include <stack>
#include <string>

//*************************** User Include Files ****************************

#include <include/general.h> 

#include "CSProfile.hpp"

#include <lib/binutils/LM.hpp>

//*************************** Forward Declarations ***************************

//*************************** Forward Declarations ***************************

CSProfile* ReadProfile_CSPROF(const char* fnm, const char *execnm);

//****************************************************************************

void InferCallFrames(CSProfile* prof, VMA begVMA, VMA endVMA,
		     LoadModScope* lmScope, VMA relocVMA);

void InferCallFrames(CSProfile* prof, VMA begVMA, VMA endVMA,
		     binutils::LM* lm);

bool NormalizeCSProfile(CSProfile* prof);

//****************************************************************************

void WriteCSProfileInDatabase(CSProfile* prof, 
			      const std::string& dbDirectory);
void WriteCSProfile(CSProfile* prof, std::ostream& os,
		    bool prettyPrint = true);

void copySourceFiles (CSProfile *prof, 
		      std::vector<std::string>& searchPaths,
		      const std::string& dbSourceDirectory);  

void LdmdSetUsedFlag(CSProfile* prof); 

//****************************************************************************

#define MAX_PATH_SIZE 2048 
/** Normalizes a file path.*/
std::string normalizeFilePath(const std::string& filePath);
std::string normalizeFilePath(const std::string& filePath, 
			      std::stack<std::string>& pathSegmentsStack);
void breakPathIntoSegments(const std::string& normFilePath, 
			   std::stack<std::string>& pathSegmentsStack);

#define DEB_READ_MMETRICS 0
#define DEB_LOAD_MODULE 0
#define DEB_PROC_FIRST_LINE 0
#define DEB_NORM_SEARCH_PATH  0
#define DEB_MKDIR_SRC_DIR 0

#endif
