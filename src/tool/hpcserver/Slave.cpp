// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/Slave.cpp $
// $Id: Slave.cpp 4461 2014-03-12 16:49:28Z laksono@gmail.com $
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
//   $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/Slave.cpp $
//
// Purpose:
//   Controls MPI processes' communication and data-getting behavior
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#include "Slave.hpp"

#include <mpi.h>

#include <vector>
#include <list>
#include <cmath>
#include <assert.h>

#include "TimeCPID.hpp"
#include "Constants.hpp"
#include "DBOpener.hpp"
#include "ImageTraceAttributes.hpp"
#include "DataCompressionLayer.hpp"
#include "Server.hpp"
#include "FilterSet.hpp"
#include "DebugUtils.hpp"

#ifdef HPCTOOLKIT_PROFILE
 #include "hpctoolkit.h"
#endif

using namespace MPI;
using namespace std;

namespace TraceviewerServer
{

	Slave::Slave()
	{
		controller= NULL;

		run();

	}
	void Slave::run()
	{
		while (true)
		{
			MPICommunication::CommandMessage Message;
			COMM_WORLD.Bcast(&Message, sizeof(Message), MPI_PACKED,
					MPICommunication::SOCKET_SERVER);
#ifdef HPCTOOLKIT_PROFILE
                                        hpctoolkit_sampling_start();
#endif
			switch (Message.command)
			{
				case OPEN:
					delete (controller);
					{//Set an artificial context to avoid initialization crossing cases
						DBOpener DBO;
						controller = DBO.openDbAndCreateStdc(string(Message.ofile.path));
					}
					break;
				case INFO:
					controller->setInfo(Message.minfo.minBegTime, Message.minfo.maxEndTime,
							Message.minfo.headerSize);
					break;
				case DATA:
				{
					int linesSent = getData(&Message);
					MPICommunication::ResultMessage nodeFinishedMsg;
					nodeFinishedMsg.tag = SLAVE_DONE;
					nodeFinishedMsg.done.rankID = COMM_WORLD.Get_rank();
					nodeFinishedMsg.done.traceLinesSent = linesSent;
					DEBUGCOUT(1) << "Rank " << nodeFinishedMsg.done.rankID << " done, having created "
							<< linesSent << " trace lines." << endl;

					COMM_WORLD.Send(&nodeFinishedMsg, sizeof(nodeFinishedMsg), MPI_PACKED,
							MPICommunication::SOCKET_SERVER, 0);
					break;
				}
				case FLTR:
				{
					FilterSet f(Message.filt.excludeMatches);
					for (int i = 0; i < Message.filt.count; ++i) {
						BinaryRepresentationOfFilter b;
						COMM_WORLD.Bcast(&b, sizeof(b), MPI_PACKED, MPICommunication::SOCKET_SERVER);
						f.add(Filter(b));
					}
					controller->applyFilters(f);
					break;
				}
				case DONE: //Server shutdown
					return;
				default:
					cerr << "Unexpected message command: " << Message.command << endl;
					break;
			}
#ifdef HPCTOOLKIT_PROFILE
                                        hpctoolkit_sampling_stop();
#endif
		}
	}

	int Slave::getData(MPICommunication::CommandMessage* Message)
	{
		MPICommunication::get_data_command gc = Message->gdata;
		ImageTraceAttributes correspondingAttributes;

		int trueRank = COMM_WORLD.Get_rank();
		int size = COMM_WORLD.Get_size();

		// Keep track of all these buffers we declare so that we can free them
		// all at the end. Allocating them on the heap lets us put out multiple
		// ISends and overlap computation and communication at the cost of extra
		// memory usage (a negligible amount though: < 10 MB)
		list<MPICommunication::ResultBufferLocations*> buffers;

		//Gives us a contiguous count of ranks from 0 to size-2 regardless of which node is the socket server
		//If ss = 0, they are all mapped one less. If ss = size-1, no changes happen
		int rank = trueRank > MPICommunication::SOCKET_SERVER ? trueRank - 1 : trueRank;

		int n = gc.processEnd - gc.processStart;
		int p = size;
		int mod = n % (p - 1);
		double q = ((double) n) / (p - 1);

		//If rank > n, there are more nodes than trace lines, so this node will do nothing
		if (rank > n)
		{
			DEBUGCOUT(1) << "No work to do" << endl;
			return 0;
		}

		//If rank < (n % (p-1)) this node should compute ceil(n/(p-1)) trace lines,
		//otherwise it should compute floor(n/(p-1))

		//=MIN(F5, $D$1)*(CEILING($B$1/($B$2-1),1)) + (F5-MIN(F5, $D$1))*(FLOOR($B$1/($B$2-1),1))
		int LowerInclusiveBound = (int) (min(mod, rank) * ceil(q)
				+ (rank - min(mod, rank)) * floor(q) + gc.processStart);
		int UpperInclusiveBound = (int) (min(mod, rank + 1) * ceil(q)
				+ (rank + 1 - min(mod, rank + 1)) * floor(q) - 1 + gc.processStart);

		DEBUGCOUT(1) << "Rank " << trueRank << " is getting lines [" << LowerInclusiveBound << ", "
				<< UpperInclusiveBound << "]" << endl;

		//These have to be the originals so that the strides will be correct
		correspondingAttributes.begProcess = gc.processStart;
		correspondingAttributes.endProcess = gc.processEnd;
		correspondingAttributes.numPixelsH = gc.horizontalResolution;
		//double processsamplefreq = ((double)gc.verticalResolution)/(p-1);
		//correspondingAttributes.numPixelsV = /*rank< mod*/ true ? ceil(processsamplefreq) : floor(processsamplefreq);
		correspondingAttributes.numPixelsV = gc.verticalResolution;

		correspondingAttributes.begTime = gc.timeStart;
		correspondingAttributes.endTime = gc.timeEnd;

		int totalTraces = min(correspondingAttributes.numPixelsV, n);
		int autoskip;
		/*  The work distribution is a tiny bit irregular because we distribute based on process number,
		 *  which because of the striding can lead to an occasional +- 1. I spent a while trying to get the
		 *  skipping exact, but I don't think it's worth it. We just need to get it close, and then we can
		 *  do an additional O(1) step. Even though this solution is not as elegant, the big difference is
		 *  that this brings it down to O(1) from O(n).*/

		autoskip = (int)floor(rank*totalTraces/(size - 1.0));

		DEBUGCOUT(2) << "Was going to autoskip " <<autoskip << " traces."<<endl;

		correspondingAttributes.lineNum = autoskip;

		*controller->attributes = correspondingAttributes;

		ProcessTimeline* nextTrace = controller->getNextTrace();
		int LinesSentCount = 0;
		int waitcount = 0;

		//If it were NULL, the no rank check would have caught it.
		assert (nextTrace != NULL);


		while (nextTrace != NULL)
		{

			if ((nextTrace->data->rank < LowerInclusiveBound)
					|| (nextTrace->data->rank > UpperInclusiveBound))
			{
				nextTrace = controller->getNextTrace();
				waitcount++;
				continue;
			}
			if (waitcount != 0)
			{
				DEBUGCOUT(2) << trueRank << " skipped " << waitcount
						<< " processes before actually starting work. Autoskip was " <<autoskip<< " and actual skip was " << controller->attributes->lineNum -1 << endl;

				waitcount = 0;
			}
			nextTrace->readInData();

			vector<TimeCPID> ActualData = *nextTrace->data->listCPID;

			MPICommunication::ResultBufferLocations* locs = new MPICommunication::ResultBufferLocations;

			MPICommunication::ResultMessage* msg = new MPICommunication::ResultMessage;
			locs->header = msg;

			msg->tag = SLAVE_REPLY;
			msg->data.line = nextTrace->line();
			int entries = ActualData.size();
			msg->data.entries = entries;

			msg->data.begtime = ActualData[0].timestamp;
			msg->data.endtime = ActualData[entries - 1].timestamp;
			msg->data.rankID = trueRank;


			int i = 0;

			unsigned char* outputBuffer = NULL;
			DataCompressionLayer* compr = NULL;
			int outputBufferLen;
			if (useCompression)
			{
				compr = new DataCompressionLayer();

				locs->compressed = true;
				locs->compMsg = compr;

				Time currentTimestamp = msg->data.begtime;
				for (i = 0; i < entries; i++)
				{
					compr->writeInt((int) (ActualData[i].timestamp - currentTimestamp));
					compr->writeInt(ActualData[i].cpid);
					currentTimestamp = ActualData[i].timestamp;
				}
				compr->flush();
				outputBufferLen = compr->getOutputLength();
				outputBuffer = compr->getOutputBuffer();
			}
			else
			{

				outputBuffer = new unsigned char[entries*SIZEOF_DELTASAMPLE];

				locs->compressed = false;
				locs->message = outputBuffer;

				char* ptrToFirstElem = (char*)&(outputBuffer[0]);
				char* currentPtr = ptrToFirstElem;
				Time currentTimestamp = msg->data.begtime;
				for (i = 0; i < entries; i++)
				{
					int deltaTimestamp = ActualData[i].timestamp - currentTimestamp;
					ByteUtilities::writeInt(currentPtr, deltaTimestamp);
					currentPtr += SIZEOF_INT;
					ByteUtilities::writeInt(currentPtr, ActualData[i].cpid);
					currentPtr += SIZEOF_INT;
				}
				outputBufferLen = entries*SIZEOF_DELTASAMPLE;
			}



			msg->data.compressedSize = outputBufferLen;
			locs->headerRequest = COMM_WORLD.Isend(msg, sizeof(*msg), MPI_PACKED,
					MPICommunication::SOCKET_SERVER, 0);

			locs->bodyRequest = COMM_WORLD.Isend(outputBuffer, outputBufferLen,
					MPI_BYTE, MPICommunication::SOCKET_SERVER, 0);

			LinesSentCount++;
			buffers.push_back(locs);

			cleanSent(buffers, false);

			if (LinesSentCount % 100 == 0)
				DEBUGCOUT(2) << trueRank << " Has sent " << LinesSentCount
						<< " ranks." << endl;

			delete nextTrace;
			nextTrace = controller->getNextTrace();
		}
		//Clean up all our MPI buffers.
		cleanSent(buffers, true);


		return LinesSentCount;
	}
	void Slave::cleanSent(list<MPICommunication::ResultBufferLocations*>& buffers, bool wait)
	{
		MPICommunication::ResultBufferLocations* current;
		while (!buffers.empty())
		{
			current = buffers.front();

			bool okayToDelete = current->headerRequest.Test() && current->bodyRequest.Test();
			if (!wait && !okayToDelete) return;

			if (!okayToDelete)
			{
				current->headerRequest.Wait();
				current->bodyRequest.Wait();
			}
			//Now it is safe to delete everything
			delete (current->header);
			if (current->compressed)
				delete (current->compMsg);
			else
				delete (current->message);
			delete (current);
			buffers.pop_front();
		}
	}
	Slave::~Slave()
	{
		delete (controller);
	}

} /* namespace TraceviewerServer */
