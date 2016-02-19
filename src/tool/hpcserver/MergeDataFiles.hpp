// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/MergeDataFiles.hpp $
// $Id: MergeDataFiles.hpp 4317 2013-07-25 16:32:22Z felipet1326@gmail.com $
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
//
// File:
//   $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/MergeDataFiles.hpp $
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************
#ifndef MERGEDATAFILES_H_
#define MERGEDATAFILES_H_

#include "DataOutputFileStream.hpp"
#include <vector>
#include <string>
#include <stdint.h>

using namespace std;
namespace TraceviewerServer
{

	enum MergeDataAttribute
	{
		SUCCESS_MERGED, SUCCESS_ALREADY_CREATED, FAIL_NO_DATA, STATUS_UNKNOWN
	};

	class MergeDataFiles
	{
	public:
		static MergeDataAttribute merge(string, string, string);

		static vector<string> splitString(string, char);
	private:
		static const uint64_t MARKER_END_MERGED_FILE = 0xFFFFFFFFDEADF00D;
		static const int PAGE_SIZE_GUESS = 4096;
		static const int PROC_POS = 5;
		static const int THREAD_POS = 4;
		static void insertMarker(DataOutputFileStream*);
		static bool isMergedFileCorrect(string*);
		static bool removeFiles(vector<string>);
		//This was in Util.java in a modified form but is more useful here
		static bool atLeastOneValidFile(string);




	};

} /* namespace TraceviewerServer */
#endif /* MERGEDATAFILES_H_ */
