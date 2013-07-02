/*
 * Communication-MPI.cpp
 *
 *  Created on: Jun 28, 2013
 *      Author: pat2
 */

#include "Communication.hpp"
#include "MPICommunication.hpp"
#include "Constants.hpp"
#include "DebugUtils.hpp"

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
	int RanksDone = 1;//1 for the MPI rank that deals with the sockets
	int Size = COMM_WORLD.Get_size();

	while (RanksDone < Size)
	{
		MPICommunication::ResultMessage msg;
		COMM_WORLD.Recv(&msg, sizeof(msg), MPI_PACKED, MPI_ANY_SOURCE, MPI_ANY_TAG);
		if (msg.tag == SLAVE_REPLY)
		{


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
			prog->incrementProgress();
		}
		else if (msg.tag == SLAVE_DONE)
		{
			DEBUGCOUT(1) << "Rank " << msg.done.rankID << " done" << endl;
			RanksDone++;
		}
	}
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

ServerType Communication::basicInit(int argc, char** argv)
{
	MPI::Init(argc, argv);

	int rank, size;
	rank = MPI::COMM_WORLD.Get_rank();
	size = MPI::COMM_WORLD.Get_size();
	if (size <= 1)
	{
		cout << "The MPI version of hpcserver must be run with more than one process. "<<
				"If you are looking for a single threaded version, you can compile hpcserver without MPI. "<<
				"See the hpctoolkit documentation for more information."<<endl;
		return NONE_EXIT_IMMEDIATELY;
	}
	if (rank == TraceviewerServer::MPICommunication::SOCKET_SERVER)
	{
		return MASTER;
	}
	else
	{
		return SLAVE;
	}
}

void Communication::closeServer()
{
	MPI::Finalize();
}
}
