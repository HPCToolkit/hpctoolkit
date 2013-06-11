/*
 * Slave.cpp
 *
 *  Created on: Jul 19, 2012
 *      Author: pat2
 */

#include "Slave.h"
#include <mpi.h>
#include <iostream>
#include <vector>
#include "TimeCPID.h"
#include "Constants.h"
#include "LocalDBOpener.h"
#include "ImageTraceAttributes.h"
#include "CompressingDataSocketLayer.h"
#include "Server.h"
#include <cmath>
using namespace MPI;
using namespace std;

namespace TraceviewerServer
{

	Slave::Slave()
	{
		STDCLNeedsDeleting = false;

		RunLoop();

	}
	void Slave::RunLoop()
	{
		while (true)
		{
			MPICommunication::CommandMessage Message;
			COMM_WORLD.Bcast(&Message, sizeof(Message), MPI_PACKED,
					MPICommunication::SOCKET_SERVER);
			switch (Message.Command)
			{
				case OPEN:
					if (STDCLNeedsDeleting)
					{
						delete (STDCL);
						STDCLNeedsDeleting = false;
					}
					{//Set an artificial context to avoid initialization crossing cases
						LocalDBOpener DBO;
						STDCL = DBO.OpenDbAndCreateSTDC(string(Message.ofile.Path));
						STDCLNeedsDeleting = true;
					}
					break;
				case INFO:
					STDCL->SetInfo(Message.minfo.minBegTime, Message.minfo.maxEndTime,
							Message.minfo.headerSize);
					break;
				case DATA:
				{
					int LinesSent = GetData(&Message);
					MPICommunication::ResultMessage NodeFinishedMsg;
					NodeFinishedMsg.Tag = SLAVE_DONE;
					NodeFinishedMsg.Done.RankID = COMM_WORLD.Get_rank();
					NodeFinishedMsg.Done.TraceLinesSent = LinesSent;
					cout << "Rank " << NodeFinishedMsg.Done.RankID << " done, having created "
							<< LinesSent << " trace lines." << endl;
					int SizeToSend = 3 * SIZEOF_INT;
					COMM_WORLD.Send(&NodeFinishedMsg, SizeToSend, MPI_PACKED,
							MPICommunication::SOCKET_SERVER, 0);
					break;
				}
				case DONE: //Server shutdown
					return;
				default:
					cerr << "Unexpected message command: " << Message.Command << endl;
					break;
			}
		}
	}

	int Slave::GetData(MPICommunication::CommandMessage* Message)
	{
		MPICommunication::get_data_command gc = Message->gdata;
		ImageTraceAttributes correspondingAttributes;

		int Truerank = COMM_WORLD.Get_rank();
		int size = COMM_WORLD.Get_size();
		//Gives us a contiguous count of ranks from 0 to size-2 regardless of which node is the socket server
		//If ss = 0, they are all mapped one less. If ss = size-1, no changes happen
		int rank = Truerank > MPICommunication::SOCKET_SERVER ? Truerank - 1 : Truerank;

		int n = gc.processEnd - gc.processStart + 1;
		int p = size;
		int mod = n % (p - 1);
		double q = ((double) n) / (p - 1);

		//If rank > n, there are more nodes than trace lines, so this node will do nothing
		if (rank > n)
		{
			cout << "No work to do" << endl;
			return 0;
		}

		//If rank < (n % (p-1)) this node should compute ceil(n/(p-1)) trace lines,
		//otherwise it should compute floor(n/(p-1))

		//=MIN(F5, $D$1)*(CEILING($B$1/($B$2-1),1)) + (F5-MIN(F5, $D$1))*(FLOOR($B$1/($B$2-1),1))
		int LowerInclusiveBound = (int) (min(mod, rank) * ceil(q)
				+ (rank - min(mod, rank)) * floor(q) + gc.processStart);
		int UpperInclusiveBound = (int) (min(mod, rank + 1) * ceil(q)
				+ (rank + 1 - min(mod, rank + 1)) * floor(q) - 1 + gc.processStart);

		cout << "Rank " << Truerank << " is getting lines [" << LowerInclusiveBound << ", "
				<< UpperInclusiveBound << "]" << endl;

		correspondingAttributes.begProcess = gc.processStart;
		correspondingAttributes.endProcess = gc.processEnd;
		correspondingAttributes.numPixelsH = gc.horizontalResolution;
		//double processsamplefreq = ((double)gc.verticalResolution)/(p-1);
		//correspondingAttributes.numPixelsV = /*rank< mod*/ true ? ceil(processsamplefreq) : floor(processsamplefreq);
		correspondingAttributes.numPixelsV = gc.verticalResolution;

		correspondingAttributes.begTime = gc.timeStart;
		correspondingAttributes.endTime = gc.timeEnd;
		correspondingAttributes.lineNum = 0;
		STDCL->Attributes = &correspondingAttributes;

		ProcessTimeline* NextTrace = STDCL->GetNextTrace(true);
		int LinesSentCount = 0;
		int waitcount = 0;

		if (NextTrace == NULL)
			cout << "First trace was null..." << endl;

		while (NextTrace != NULL)
		{
			if ((NextTrace->Data->Rank < LowerInclusiveBound)
					|| (NextTrace->Data->Rank > UpperInclusiveBound))
			{
				NextTrace = STDCL->GetNextTrace(true);
				waitcount++;
				continue;
			}
			if (waitcount != 0)
			{
				cout << Truerank << " skipped " << waitcount
						<< " processes before actually starting work" << endl;
				waitcount = 0;
			}
			NextTrace->ReadInData();

			vector<TimeCPID>* ActualData = NextTrace->Data->ListCPID;
			MPICommunication::ResultMessage msg;
			msg.Tag = SLAVE_REPLY;
			msg.Data.Line = NextTrace->Line();
			int entries = ActualData->size();
			msg.Data.Entries = entries;

			msg.Data.Begtime = (*ActualData)[0].Timestamp;
			msg.Data.Endtime = (*ActualData)[entries - 1].Timestamp;
			msg.Data.RankID = Truerank;
			/*Have to move this so that we know how large the compressed stream is
			 * COMM_WORLD.Send(&msg, sizeof(msg), MPI_PACKED, MPICommunication::SOCKET_SERVER,
			 0);*/

			int i = 0;
			/*for (it = ActualData->begin(); it != ActualData->end(); it++)
			 {
			 msg.Data.Data[i++] = it->CPID;
			 }*/
			const unsigned char* OutputBuffer;
			int OutputBufferLen;
			if (Compression)
			{
				CompressingDataSocketLayer Compr;

				double CurrentTimestamp = msg.Data.Begtime;
				for (i = 0; i < entries; i++)
				{
					Compr.WriteInt((int) ((*ActualData)[i].Timestamp - CurrentTimestamp));
					Compr.WriteInt((*ActualData)[i].CPID);
					CurrentTimestamp = (*ActualData)[i].Timestamp;
				}
				Compr.Flush();
				OutputBufferLen = Compr.GetOutputLength();
				OutputBuffer = Compr.GetOutputBuffer();
			}
			else
			{

				OutputBuffer = new unsigned char[entries*SIZEOF_INT];
				char* ptrToFirstElem = (char*)&(OutputBuffer[0]);
				int CurrIndexInBuff = 0;
				for (i = 0; i < entries; i++)
				{
					ByteUtilities::WriteInt(ptrToFirstElem + CurrIndexInBuff, (*ActualData)[i].CPID);
					CurrIndexInBuff += 4;
				}
				OutputBufferLen = entries*4;
			}
			msg.Data.CompressedSize = OutputBufferLen;
			COMM_WORLD.Send(&msg, sizeof(msg), MPI_PACKED, MPICommunication::SOCKET_SERVER,
					0);
			//cout << "Buffer overflow protection: Setting " << i << " to 0xABCDEF" << endl;
			//CPIDs[i] = 0xABCDEF;

			// sizeof(msg) is too large because it assumes all the traces are full. It'll lead to lots of extra sending
			//										Tag, Line, Size			 Beg ts, end ts--double same size as long
			//int SizeInBytes = 3 * Constants::SIZEOF_INT + 2 * Constants::SIZEOF_LONG +
			//Each entry is an int
			//		(entries) * Constants::SIZEOF_INT;

			COMM_WORLD.Send(OutputBuffer, OutputBufferLen, MPI_BYTE,
					MPICommunication::SOCKET_SERVER, 0);
			if (!Compression)
			{
				delete[] OutputBuffer;
			}
			LinesSentCount++;
			if (LinesSentCount % 100 == 0)
				cout << Truerank << " Has sent " << LinesSentCount
						<< ". Most recent message was " << NextTrace->Line() << " and contained "
						<< entries << " entries" << endl;

			NextTrace = STDCL->GetNextTrace(true);
		}
		return LinesSentCount;
	}

	Slave::~Slave()
	{
		if (STDCLNeedsDeleting)
		{
			delete (STDCL);
			STDCLNeedsDeleting = false;
		}
	}

} /* namespace TraceviewerServer */
