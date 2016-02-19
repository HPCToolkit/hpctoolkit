// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/BaseDataFile.hpp $
// $Id: BaseDataFile.hpp 4307 2013-07-18 17:04:52Z felipet1326@gmail.com $
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
//   $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/BaseDataFile.hpp $
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef BASEDATAFILE_H_
#define BASEDATAFILE_H_

using namespace std;

#include <string>

#include "FileUtils.hpp" // For FileOffset
#include "LargeByteBuffer.hpp"

namespace TraceviewerServer {

struct OffsetPair {
	FileOffset start;
	FileOffset end;
};

class BaseDataFile {
public:
	BaseDataFile(string filename, int headerSize);
	virtual ~BaseDataFile();
	int getNumberOfFiles();
	OffsetPair* getOffsets();
	LargeByteBuffer* getMasterBuffer();
	void setData(string, int);

	bool isMultiProcess();
	bool isMultiThreading();
	bool isHybrid();

	int* processIDs;
	short* threadIDs;
private:
	int type; // Default is Constants::MULTI_PROCESSES | Constants::MULTI_THREADING;
	LargeByteBuffer* masterBuff;
	int numFiles;

	OffsetPair* offsets;
};

} /* namespace TraceviewerServer */
#endif /* BASEDATAFILE_H_ */
