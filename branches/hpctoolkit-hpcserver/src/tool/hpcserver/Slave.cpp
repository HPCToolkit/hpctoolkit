// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2013, Rice University
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
//   $HeadURL$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#include "Slave.hpp"
#include <mpi.h>
#include <iostream>
#include <vector>
#include "TimeCPID.hpp"
#include "Constants.hpp"
#include "DBOpener.hpp"
#include "ImageTraceAttributes.hpp"
#include "CompressingDataSocketLayer.hpp"
#include "Server.hpp"
#include <cmath>
#include <assert.h>
#include "FilterSet.hpp"
using namespace MPI;
using namespace std;

namespace TraceviewerServer
{

	Slave::Slave()
	{
		controllerNeedsDeleting = false;

		run();

	}
	void Slave::run()
	{
		while (true)
		{
			MPICommunication::CommandMessage Message;
			COMM_WORLD.Bcast(&Message, sizeof(Message), MPI_PACKED,
					MPICommunication::SOCKET_SERVER);
			switch (Message.command)
			{
				case OPEN:
					if (controllerNeedsDeleting)
					{
						delete (controller);
						controllerNeedsDeleting = false;
					}
					{//Set an artificial context to avoid initialization crossing cases
						DBOpener DBO;
						controller = DBO.openDbAndCreateStdc(string(Message.ofile.path));
						controllerNeedsDeleting = true;
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
					#if DEBUG > 1
					cout << "Rank " << nodeFinishedMsg.done.rankID << " done, having created "
							<< linesSent << " trace lines." << endl;
					#endif
					int SizeToSend = 3 * SIZEOF_INT;
					COMM_WORLD.Send(&nodeFinishedMsg, SizeToSend, MPI_PACKED,
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
		}
	}

	int Slave::getData(MPICommunication::CommandMessage* Message)
	{
		MPICommunication::get_data_command gc = Message->gdata;
		ImageTraceAttributes correspondingAttributes;

		int trueRank = COMM_WORLD.Get_rank();
		int size = COMM_WORLD.Get_size();

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
			#if DEBUG > 1
			cout << "No work to do" << endl;
			#endif
			return 0;
		}

		//If rank < (n % (p-1)) this node should compute ceil(n/(p-1)) trace lines,
		//otherwise it should compute floor(n/(p-1))

		//=MIN(F5, $D$1)*(CEILING($B$1/($B$2-1),1)) + (F5-MIN(F5, $D$1))*(FLOOR($B$1/($B$2-1),1))
		int LowerInclusiveBound = (int) (min(mod, rank) * ceil(q)
				+ (rank - min(mod, rank)) * floor(q) + gc.processStart);
		int UpperInclusiveBound = (int) (min(mod, rank + 1) * ceil(q)
				+ (rank + 1 - min(mod, rank + 1)) * floor(q) - 1 + gc.processStart);

		#if DEBUG > 1
		cout << "Rank " << trueRank << " is getting lines [" << LowerInclusiveBound << ", "
				<< UpperInclusiveBound << "]" << endl;
		#endif

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
		#if DEBUG > 1
		cout<< "Was going to autoskip " <<autoskip << " traces."<<endl;
		#endif

		correspondingAttributes.lineNum = autoskip;

		controller->attributes = &correspondingAttributes;

		ProcessTimeline* NextTrace = controller->getNextTrace();
		int LinesSentCount = 0;
		int waitcount = 0;

		assert (NextTrace != NULL);


		while (NextTrace != NULL)
		{

			if ((NextTrace->data->rank < LowerInclusiveBound)
					|| (NextTrace->data->rank > UpperInclusiveBound))
			{
				NextTrace = controller->getNextTrace();
				waitcount++;
				continue;
			}
			if (waitcount != 0)
			{
				#if DEBUG > 1
				cout << trueRank << " skipped " << waitcount
						<< " processes before actually starting work. Autoskip was " <<autoskip<< " and actual skip was " << controller->attributes->lineNum -1 << endl;
				#endif
				waitcount = 0;
			}
			NextTrace->readInData();

			vector<TimeCPID> ActualData = *NextTrace->data->listCPID;
			MPICommunication::ResultMessage msg;
			msg.tag = SLAVE_REPLY;
			msg.data.line = NextTrace->line();
			int entries = ActualData.size();
			msg.data.entries = entries;

			msg.data.begtime = ActualData[0].timestamp;
			msg.data.endtime = ActualData[entries - 1].timestamp;
			msg.data.rankID = trueRank;
			/*Have to move this so that we know how large the compressed stream is
			 * COMM_WORLD.Send(&msg, sizeof(msg), MPI_PACKED, MPICommunication::SOCKET_SERVER,
			 0);*/

			int i = 0;
			/*for (it = ActualData->begin(); it != ActualData->end(); it++)
			 {
			 msg.Data.Data[i++] = it->CPID;
			 }*/
			unsigned char* outputBuffer;
			int outputBufferLen;
			if (useCompression)
			{
				CompressingDataSocketLayer compr;

				Time currentTimestamp = msg.data.begtime;
				for (i = 0; i < entries; i++)
				{
					compr.writeInt((int) (ActualData[i].timestamp - currentTimestamp));
					compr.writeInt(ActualData[i].cpid);
					currentTimestamp = ActualData[i].timestamp;
				}
				compr.flush();
				outputBufferLen = compr.getOutputLength();
				outputBuffer = compr.getOutputBuffer();
			}
			else
			{

				outputBuffer = new unsigned char[entries*SIZEOF_DELTASAMPLE];
				char* ptrToFirstElem = (char*)&(outputBuffer[0]);
				char* currentPtr = ptrToFirstElem;
				Time currentTimestamp = msg.data.begtime;
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
			msg.data.compressedSize = outputBufferLen;
			COMM_WORLD.Send(&msg, sizeof(msg), MPI_PACKED, MPICommunication::SOCKET_SERVER,
					0);

			COMM_WORLD.Send(outputBuffer, outputBufferLen, MPI_BYTE,
					MPICommunication::SOCKET_SERVER, 0);
			if (!useCompression)
			{
				delete[] outputBuffer;
			}
			LinesSentCount++;
			#if DEBUG > 2
			if (LinesSentCount % 100 == 0)
				cout << trueRank << " Has sent " << LinesSentCount
						<< " ranks." << endl;
			#endif

			NextTrace = controller->getNextTrace();
		}
		return LinesSentCount;
	}

	Slave::~Slave()
	{
		if (controllerNeedsDeleting)
		{
			delete (controller);
			controllerNeedsDeleting = false;
		}
	}

} /* namespace TraceviewerServer */
