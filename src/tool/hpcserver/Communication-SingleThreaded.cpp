// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/Communication-SingleThreaded.cpp $
// $Id: Communication-SingleThreaded.cpp 4307 2013-07-18 17:04:52Z felipet1326@gmail.com $
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
//   $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/Communication-SingleThreaded.cpp $
//
// Purpose:
//   The single-threaded implementation of the methods in Communication.hpp
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#include <stdint.h>                     // for uint64_t
#include <iostream>                     // for operator<<, basic_ostream, etc
#include <string>                       // for string
#include <vector>                       // for vector, vector<>::iterator

#include "Communication.hpp"            // for Communication
#include "DataCompressionLayer.hpp"     // for DataCompressionLayer
#include "DataSocketStream.hpp"         // for DataSocketStream
#include "DebugUtils.hpp"               // for DEBUGCOUT
#include "Filter.hpp"
#include "ImageTraceAttributes.hpp"     // for ImageTraceAttributes
#include "ProcessTimeline.hpp"          // for ProcessTimeline
#include "ProgressBar.hpp"              // for ProgressBar
#include "Server.hpp"                   // for Server
#include "SpaceTimeDataController.hpp"  // for SpaceTimeDataController
#include "TimeCPID.hpp"                 // for TimeCPID, Time
#include "TraceDataByRank.hpp"          // for TraceDataByRank


using namespace std;
namespace TraceviewerServer {

void Communication::sendParseInfo(uint64_t minBegTime, uint64_t maxEndTime, int headerSize)
{//Do nothing
}

void Communication::sendParseOpenDB(string pathToDB) {}

void Communication::sendStartGetData(SpaceTimeDataController* contr, int processStart, int processEnd,
			Time timeStart, Time timeEnd, int verticalResolution, int horizontalResolution)
{

	ImageTraceAttributes* correspondingAttributes = contr->attributes;

	correspondingAttributes->begProcess = processStart;
	correspondingAttributes->endProcess = processEnd;
	correspondingAttributes->numPixelsH = horizontalResolution;
	correspondingAttributes->numPixelsV = verticalResolution;
	correspondingAttributes->begTime =  timeStart;
	correspondingAttributes->endTime =  timeEnd;
	correspondingAttributes->lineNum = 0;


}
void Communication::sendEndGetData(DataSocketStream* stream, ProgressBar* prog, SpaceTimeDataController* controller)
{
	// TODO: Make this so that the Lines get sent as soon as they are
	// filled.

	controller->fillTraces();
	for (int i = 0; i < controller->tracesLength; i++)
	{

		ProcessTimeline* timeline = controller->traces[i];
		stream->writeInt( timeline->line());
		vector<TimeCPID> data = *timeline->data->listCPID;
		stream->writeInt( data.size());
		// Begin time
		stream->writeLong( data[0].timestamp);
		//End time
		stream->writeLong( data[data.size() - 1].timestamp);

		DataCompressionLayer comprStr;

		vector<TimeCPID>::iterator it;
		DEBUGCOUT(2) << "Sending process timeline with " << data.size() << " entries" << endl;


		Time currentTime = data[0].timestamp;
		for (it = data.begin(); it != data.end(); ++it)
		{
			comprStr.writeInt( (int)(it->timestamp - currentTime));
			comprStr.writeInt( it->cpid);
			currentTime = it->timestamp;
		}
		comprStr.flush();
		int outputBufferLen = comprStr.getOutputLength();
		char* outputBuffer = (char*)comprStr.getOutputBuffer();

		stream->writeInt(outputBufferLen);

		stream->writeRawData(outputBuffer, outputBufferLen);
		prog->incrementProgress();
	}
	stream->flush();
}

void Communication::sendStartFilter(int count, bool excludeMatches)
{//Do nothing
}

void Communication::sendFilter(BinaryRepresentationOfFilter filt)
{
}

bool Communication::basicInit(int argc, char** argv)
{
	return true;
}
void Communication::run()
{
	TraceviewerServer::Server();
}
void Communication::closeServer()
{
	cout<<"Server done, closing..."<<endl;
}
}
