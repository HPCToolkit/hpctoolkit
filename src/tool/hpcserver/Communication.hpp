// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/Communication.hpp $
// $Id: Communication.hpp 4307 2013-07-18 17:04:52Z felipet1326@gmail.com $
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
//   $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/Communication.hpp $
//
// Purpose:
//    Abstracts the MPI/No-MPI differences away.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************


#ifndef COMMUNICATION_H_
#define COMMUNICATION_H_

#include <string>

#include "TimeCPID.hpp" //For Time
#include "ProgressBar.hpp"
#include "SpaceTimeDataController.hpp"
#include "DataSocketStream.hpp"
#include "Filter.hpp"

namespace TraceviewerServer
{
enum ServerType {
	NONE_EXIT_IMMEDIATELY = 0,
	MASTER = 1,
	SLAVE = 2
};

class Communication {
public:

	static void sendParseInfo(Time minBegTime, Time maxEndTime, int headerSize);
	static void sendParseOpenDB(string pathToDB);
	static void sendStartGetData(SpaceTimeDataController* contr, int processStart, int processEnd,
			Time timeStart, Time timeEnd, int verticalResolution, int horizontalResolution);
	static void sendEndGetData(DataSocketStream* stream, ProgressBar* prog, SpaceTimeDataController* controller);
	static void sendStartFilter(int count, bool excludeMatches);
	static void sendFilter(BinaryRepresentationOfFilter filt);

	static bool basicInit(int argc, char** argv);
	static void run();
	static void closeServer();
};
}
#endif /* COMMUNICATION_H_ */
