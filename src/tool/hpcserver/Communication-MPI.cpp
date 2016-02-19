// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/Communication-MPI.cpp $
// $Id: Communication-MPI.cpp 4317 2013-07-25 16:32:22Z felipet1326@gmail.com $
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
//   $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/Communication-MPI.cpp $
//
// Purpose:
//   The MPI implementation of the methods in Communication.hpp. These methods
//   are called mostly by the SocketServer.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************


#include "Communication.hpp"
#include "MPICommunication.hpp"
#include "Constants.hpp"
#include "DebugUtils.hpp"
#include "Server.hpp"
#include "Slave.hpp"

#include <mpi.h>

#include <iostream> //For cerr, cout
#include <algorithm> //For copy

using namespace std;
using namespace MPI;

namespace TraceviewerServer
{

void Communication::sendParseInfo(Time minBegTime, Time maxEndTime, int headerSize)
{
	MPICommunication::CommandMessage Info;
			Info.command = INFO;
			Info.minfo.minBegTime = minBegTime;
			Info.minfo.maxEndTime = maxEndTime;
			Info.minfo.headerSize = headerSize;
			COMM_WORLD.Bcast(&Info, sizeof(Info), MPI_PACKED, MPICommunication::SOCKET_SERVER);

}

void Communication::sendParseOpenDB(string pathToDB)
{
	MPICommunication::CommandMessage cmdPathToDB;
	cmdPathToDB.command = OPEN;
	if (pathToDB.length() > MAX_DB_PATH_LENGTH)
	{
		cerr << "Path too long" << endl;
		throw ERROR_PATH_TOO_LONG;
	}
	copy(pathToDB.begin(), pathToDB.end(), cmdPathToDB.ofile.path);
	cmdPathToDB.ofile.path[pathToDB.size()] = '\0';

	COMM_WORLD.Bcast(&cmdPathToDB, sizeof(cmdPathToDB), MPI_PACKED,
			MPICommunication::SOCKET_SERVER);

}
void Communication::sendStartGetData(SpaceTimeDataController* contr, int processStart, int processEnd,
			Time timeStart, Time timeEnd, int verticalResolution, int horizontalResolution)
{
	MPICommunication::CommandMessage toBcast;
	toBcast.command = DATA;
	toBcast.gdata.processStart = processStart;
	toBcast.gdata.processEnd = processEnd;
	toBcast.gdata.timeStart = timeStart;
	toBcast.gdata.timeEnd = timeEnd;
	toBcast.gdata.verticalResolution = verticalResolution;
	toBcast.gdata.horizontalResolution = horizontalResolution;
	COMM_WORLD.Bcast(&toBcast, sizeof(toBcast), MPI_PACKED,
		MPICommunication::SOCKET_SERVER);
}
void Communication::sendEndGetData(DataSocketStream* stream, ProgressBar* prog, SpaceTimeDataController* controller)
{
	int ranksDone = 1;//1 for the MPI rank that deals with the sockets
	int size = COMM_WORLD.Get_size();

	bool first = false;

	while (ranksDone < size)
	{
		MPICommunication::ResultMessage msg;
		COMM_WORLD.Recv(&msg, sizeof(msg), MPI_PACKED, MPI_ANY_SOURCE, MPI_ANY_TAG);
		if (msg.tag == SLAVE_REPLY)
		{
			if (first)
			{
				LOGTIMESTAMPEDMSG("First line computed.")
			}

			stream->writeInt(msg.data.line);
			stream->writeInt(msg.data.entries);
			stream->writeLong(msg.data.begtime); // Begin time
			stream->writeLong(msg.data.endtime); //End time
			stream->writeInt(msg.data.compressedSize);

			char CompressedTraceLine[msg.data.compressedSize];
			COMM_WORLD.Recv(CompressedTraceLine, msg.data.compressedSize, MPI_BYTE, msg.data.rankID,
					MPI_ANY_TAG);

			stream->writeRawData(CompressedTraceLine, msg.data.compressedSize);

			stream->flush();
			if (first)
			{
				LOGTIMESTAMPEDMSG("First line sent.")
			}
			first = false;
			prog->incrementProgress();
		}
		else if (msg.tag == SLAVE_DONE)
		{
			DEBUGCOUT(1) << "Rank " << msg.done.rankID << " done" << endl;
			ranksDone++;
			if (ranksDone == 2)
			{
				LOGTIMESTAMPEDMSG("First rank done.")
			}
		}
	}
	LOGTIMESTAMPEDMSG("All data done.")
}
void Communication::sendStartFilter(int count, bool excludeMatches)
{
	MPICommunication::CommandMessage toBcast;
		toBcast.command = FLTR;
		toBcast.filt.count = count;
		toBcast.filt.excludeMatches = excludeMatches;
		COMM_WORLD.Bcast(&toBcast, sizeof(toBcast), MPI_PACKED,
					MPICommunication::SOCKET_SERVER);

}
void Communication::sendFilter(BinaryRepresentationOfFilter filt)
{
	COMM_WORLD.Bcast(&filt, sizeof(filt), MPI_PACKED, MPICommunication::SOCKET_SERVER);
}

bool Communication::basicInit(int argc, char** argv)
{
	MPI::Init(argc, argv);

	int size;

	size = MPI::COMM_WORLD.Get_size();
	if (size <= 1)
	{
		cout << "The MPI version of hpcserver must be run with more than one process. "<<
				"If you are looking for a single threaded version, you can compile hpcserver without MPI. "<<
				"See the hpctoolkit documentation for more information."<<endl;
		return false;
	}
	return true;
}
void Communication::run()
{
	int rank;
	rank = MPI::COMM_WORLD.Get_rank();
	if (rank == TraceviewerServer::MPICommunication::SOCKET_SERVER)
		TraceviewerServer::Server();
	else
		TraceviewerServer::Slave();
}
void Communication::closeServer()
{
	if (COMM_WORLD.Get_rank()==MPICommunication::SOCKET_SERVER)
	{//The slaves participate in the bcast in the slave class before they parse it
		MPICommunication::CommandMessage serverShutdown;
		serverShutdown.command = DONE;
		COMM_WORLD.Bcast(&serverShutdown, sizeof(serverShutdown), MPI_PACKED, MPICommunication::SOCKET_SERVER);
		cout<<"Server done, closing..."<<endl;
	}
	MPI::Finalize();
}
}
