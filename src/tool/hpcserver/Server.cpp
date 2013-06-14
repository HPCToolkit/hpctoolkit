//#define UseBoost
//#define USE_MPI
/*
 * Server.cpp
 *
 *  Created on: Jul 9, 2012
 *      Author: pat2
 */

#include "Server.h"
#include "SpaceTimeDataControllerLocal.h"
#include "DataSocketStream.h"
#include "LocalDBOpener.h"
#include "Constants.h"
#include "MPICommunication.h"
#include "CompressingDataSocketLayer.h"
#include <iostream>
#include <fstream>
#include <mpi.h>
#include "zlib.h"
//#include "SpaceTimeDataControllerLocal.h"

using namespace std;
using namespace MPI;
namespace TraceviewerServer
{
	bool Compression = true;
	int MainPort = DEFAULT_PORT;
	int XMLPort = 0;

	static SpaceTimeDataControllerLocal* STDCL;

	Server::Server()
	{

	}

	Server::~Server()
	{
		delete (STDCL);
	}
	int Server::main(int argc, char *argv[])
	{

		DataSocketStream* socketptr;
		try
		{
//TODO: Change the port to 21591 because 21590 has some other use...
			//DataSocketStream CLsocket = new DataSocketStream(21590);
			socketptr = new DataSocketStream(MainPort, true);
			/*
			 ip::tcp::socket CLsocket(io_service);
			 //CLsocket.open(ip::tcp::v4());
			 acceptor.accept(CLsocket);
			 socketptr = (DataSocketStream*) &CLsocket;
			 */

			//socketptr = &CLsocket;
		} catch (std::exception& e)
		{
			std::cerr << e.what() << std::endl;
			return ERROR_STREAM_OPEN_FAILED;
		}
		MainPort = socketptr->GetPort();
		cout << "Received connection" << endl;

		//vector<char> test(4);
		//as::read(*socketptr, as::buffer(test));
		int Command = socketptr->ReadInt();
		if (Command == OPEN)
		{
			while( RunConnection(socketptr)==START_NEW_CONNECTION_IMMEDIATELY)
				;
		}
		else
		{
			cerr << "Expected an open command, got " << Command << endl;
			return ERROR_EXPECTED_OPEN_COMMAND;
		}
		return 0;
	}


	int Server::RunConnection(DataSocketStream* socketptr)
	{
		ParseOpenDB(socketptr);

		if (STDCL == NULL)
		{
			cout << "Could not open database" << endl;
			SendDBOpenFailed(socketptr);
			return ERROR_DB_OPEN_FAILED;
		}
		else
		{
			cout << "Database opened" << endl;
			SendDBOpenedSuccessfully(socketptr);
		}

		int Message = socketptr->ReadInt();
		if (Message == INFO)
			ParseInfo(socketptr);
		else
			cerr << "Did not receive info packet" << endl;

		bool EndingConnection = false;
		while (!EndingConnection)
		{
			int NextCommand = socketptr->ReadInt();
			switch (NextCommand)
			{
				case DATA:
					GetAndSendData(socketptr);
					break;
				case DONE:
					EndingConnection = true;
					break;
				case OPEN:
					EndingConnection = true;
					return START_NEW_CONNECTION_IMMEDIATELY;
				default:
					cerr << "Unknown command received" << endl;
					return (ERROR_UNKNOWN_COMMAND);
			}
		}

		return 0;
	}
	void Server::ParseInfo(DataSocketStream* socket)
	{

		Long minBegTime = socket->ReadLong();
		Long maxEndTime = socket->ReadLong();
		int headerSize = socket->ReadInt();
		STDCL->SetInfo(minBegTime, maxEndTime, headerSize);
#ifdef USE_MPI
		MPICommunication::CommandMessage Info;
		Info.Command = INFO;
		Info.minfo.minBegTime = minBegTime;
		Info.minfo.maxEndTime = maxEndTime;
		Info.minfo.headerSize = headerSize;
		COMM_WORLD.Bcast(&Info, sizeof(Info), MPI_PACKED, MPICommunication::SOCKET_SERVER);
#endif
	}
	void Server::SendDBOpenedSuccessfully(DataSocketStream* socket)
	{

		socket->WriteInt(DBOK);

		//int port = 2224;

		//socket->WriteInt(port);
		DataSocketStream* XmlSocket;
		if (XMLPort == -1)
			XMLPort = MainPort;
		int ActualXMLPort = XMLPort;
		if (XMLPort != MainPort)
		{//On a different port. Create another socket.
			XmlSocket = new DataSocketStream(XMLPort, false);
			ActualXMLPort = XmlSocket->GetPort();//Could be different if XMLPort is 0
		}
		else
		{
			//Same port, simply use this socket
			XmlSocket = socket;
		}
		socket->WriteInt(ActualXMLPort);

		int NumFiles = STDCL->GetHeight();
		socket->WriteInt(NumFiles);

		int CompressionType; //This is an int so that it is possible to have different compression algorithms in the future. For now, it is just 0=no compression, 1= normal compression
		CompressionType = Compression ? 1 : 0;
		socket->WriteInt(CompressionType);

		//Send ValuesX
		int* ValuesXPI = STDCL->GetValuesXProcessID();
		short* ValuesXTI = STDCL->GetValuesXThreadID();
		for (int i = 0; i < NumFiles; i++)
		{
			socket->WriteInt(ValuesXPI[i]);
			socket->WriteShort(ValuesXTI[i]);
		}

		socket->Flush();

		cout << "Waiting to send XML on port " << ActualXMLPort << ". Num traces was "
				<< STDCL->GetHeight() << endl;
		if (ActualXMLPort != MainPort)
		{
			XmlSocket->AcceptS();
		}
		else
		{
			XmlSocket->WriteInt(EXML);
		}
		vector<char> CompressedXML = CompressXML();
		XmlSocket->WriteInt(CompressedXML.size());
		XmlSocket->WriteRawData(&CompressedXML[0], CompressedXML.size());
		XmlSocket->Flush();
		cout << "XML Sent" << endl;
		if(ActualXMLPort != MainPort)
			delete (XmlSocket);
	}

	vector<char> Server::CompressXML()
	{
		vector<char> Compressed;

		FILE* in = fopen(STDCL->GetExperimentXML().c_str(), "r");
		//From http://zlib.net/zpipe.c with some editing
		z_stream Compressor;
		Compressor.zalloc = Z_NULL;
		Compressor.zfree = Z_NULL;
		Compressor.opaque = Z_NULL;
		//int ret = deflateInit(&Compressor, -1);
		//This makes a gzip stream with a window of 15 bits
		int ret = deflateInit2(&Compressor, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16+15, 8, Z_DEFAULT_STRATEGY);
		if (ret != Z_OK)
			throw ret;

		//int size = TraceviewerServer::FileUtils::GetFileSize(STDCL->GetExperimentXML());

#define CHUNK 0x4000

		unsigned char InBuffer[CHUNK];
		unsigned char OutBuffer[CHUNK];
		int flush;
		int amtprocessed =0;
		int compramt;
		do {
			Compressor.avail_in = fread(InBuffer, 1, CHUNK, in);
			amtprocessed += Compressor.avail_in;
			//cout<<"Processed " << amtprocessed << " / " <<size<<endl;
			if (ferror(in)) {
				cerr<<"Error"<<endl;
				(void)deflateEnd(&Compressor);
				throw Z_ERRNO;
			}
			flush = feof(in) ? Z_FINISH : Z_NO_FLUSH;

			Compressor.next_in = InBuffer;
			int have;
			/* run deflate() on input until output buffer not full, finish
		           compression if all of source has been read in */
			do {
				Compressor.avail_out = CHUNK;
				Compressor.next_out = OutBuffer;
				ret = deflate(&Compressor, flush);    /* no bad return value */
				if (ret == Z_STREAM_ERROR) throw -33445;  /* state not clobbered */
				have = CHUNK - Compressor.avail_out;
				compramt += have;
				//cout<<"Writing "<<have<< " compressed bytes. Length of compressed file so far is "<< compramt<<endl;
				Compressed.insert(Compressed.end(), OutBuffer, OutBuffer+have);
			} while (Compressor.avail_out == 0);


			/* done when last data in file processed */
		} while (flush != Z_FINISH);
		deflateEnd(&Compressor);
		cout<<"Compressed XML Size: "<<Compressed.size()<<endl;
		return Compressed;
	}

	void Server::ParseOpenDB(DataSocketStream* receiver)
	{

		if (false) //(!receiver->is_open())
			cout << "Socket not open!" << endl;

		string PathToDB = receiver->ReadString();
		LocalDBOpener DBO;
		cout << "Opening database: " << PathToDB << endl;
		STDCL = DBO.OpenDbAndCreateSTDC(PathToDB);
#ifdef USE_MPI
		if (STDCL != NULL)
		{
		MPICommunication::CommandMessage cmdPathToDB;
		cmdPathToDB.Command = OPEN;
		if (PathToDB.length() > MAX_DB_PATH_LENGTH)
		{
			cerr << "Path too long" << endl;
			throw ERROR_PATH_TOO_LONG;
		}
		copy(PathToDB.begin(), PathToDB.end(), cmdPathToDB.ofile.Path);
		cmdPathToDB.ofile.Path[PathToDB.size()] = '\0';

		COMM_WORLD.Bcast(&cmdPathToDB, sizeof(cmdPathToDB), MPI_PACKED,
				MPICommunication::SOCKET_SERVER);
		}
#endif


	}

	void Server::SendDBOpenFailed(DataSocketStream* socket)
	{
		socket->WriteInt(NODB);
		socket->WriteInt(0);
		socket->Flush();
	}

#define ISN(g) (g<0)

	void Server::GetAndSendData(DataSocketStream* Stream)
	{

		int processStart = Stream->ReadInt();
		int processEnd = Stream->ReadInt();
		double timeStart = Stream->ReadDouble();
		double timeEnd = Stream->ReadDouble();
		int verticalResolution = Stream->ReadInt();
		int horizontalResolution = Stream->ReadInt();
		cout << "Time end: " << timeEnd <<endl;

		if (ISN(processStart) || ISN(processEnd) || (processStart > processEnd)
				|| ISN(verticalResolution) || ISN(horizontalResolution)
				|| (timeEnd < timeStart))
		{
			cerr
					<< "A data request with invalid parameters was received. This sometimes happens if the client shuts down in the middle of a request. The server will now shut down."
					<< endl;
			throw(ERROR_INVALID_PARAMETERS);
		}

#ifdef USE_MPI

		MPICommunication::CommandMessage toBcast;
		toBcast.Command = DATA;
		toBcast.gdata.processStart = processStart;
		toBcast.gdata.processEnd = processEnd;
		toBcast.gdata.timeStart = timeStart;
		toBcast.gdata.timeEnd = timeEnd;
		toBcast.gdata.verticalResolution = verticalResolution;
		toBcast.gdata.horizontalResolution = horizontalResolution;
		COMM_WORLD.Bcast(&toBcast, sizeof(toBcast), MPI_PACKED,
			MPICommunication::SOCKET_SERVER);
#else
		ImageTraceAttributes correspondingAttributes;

		correspondingAttributes.begProcess = processStart;
		correspondingAttributes.endProcess = processEnd;
		correspondingAttributes.numPixelsH = horizontalResolution;
		correspondingAttributes.numPixelsV = verticalResolution;
		// Time start and Time end?? Should actually be longs instead of
		// doubles????
		//double timeSpan = timeEnd - timeStart;
		correspondingAttributes.begTime = (long) timeStart;
		correspondingAttributes.endTime = (long) timeEnd;
		correspondingAttributes.lineNum = 0;
		STDCL->Attributes = &correspondingAttributes;

		// TODO: Make this so that the Lines get sent as soon as they are
		// filled.

		STDCL->FillTraces(-1, true);
#endif

		Stream->WriteInt(HERE);
		Stream->Flush();


		/*#define stWr(type,val) CompL.Write##type((val))
		 #else
		 #define stWr(type,val) Stream->Write##type((val))
		 #endif*/

#ifdef USE_MPI
		int RanksDone = 1;
		int Size = COMM_WORLD.Get_size();

		while (RanksDone < Size)
		{
			MPICommunication::ResultMessage msg;
			COMM_WORLD.Recv(&msg, sizeof(msg), MPI_PACKED, MPI_ANY_SOURCE, MPI_ANY_TAG);
			if (msg.Tag == SLAVE_REPLY)
			{

				//Stream->WriteInt(msg.Data.Line);
				Stream->WriteInt(msg.Data.Line);
				Stream->WriteInt(msg.Data.Entries);
				Stream->WriteDouble(msg.Data.Begtime); // Begin time
				Stream->WriteDouble(msg.Data.Endtime); //End time
				Stream->WriteInt(msg.Data.CompressedSize);

				char CompressedTraceLine[msg.Data.CompressedSize];
				COMM_WORLD.Recv(CompressedTraceLine, msg.Data.CompressedSize, MPI_BYTE, msg.Data.RankID,
						MPI_ANY_TAG);

				Stream->WriteRawData(CompressedTraceLine, msg.Data.CompressedSize);

				//delete(&msg);
				Stream->Flush();
			}
			else if (msg.Tag == SLAVE_DONE)
			{
				cout << "Rank " << msg.Done.RankID << " done" << endl;
				RanksDone++;
			}
		}


#else
		for (int i = 0; i < STDCL->TracesLength; i++)
		{

			ProcessTimeline* T = STDCL->Traces[i];
			Stream->WriteInt( T->Line());
			vector<TimeCPID> data = T->Data->ListCPID;
			Stream->WriteInt( data.size());
			Stream->WriteDouble( data[0].Timestamp);
			// Begin time
			Stream->WriteDouble( data[data.size() - 1].Timestamp);
			//End time
			CompressingDataSocketLayer Compr;

			vector<TimeCPID>::iterator it;
			cout << "Sending process timeline with " << data.size() << " entries" << endl;
			
			double currentTime = data[0].Timestamp;
			for (it = data.begin(); it != data.end(); ++it)
			{
				Compr.WriteInt( (int)(it->Timestamp - currentTime));
				Compr.WriteInt( it->CPID);
				currentTime = it->Timestamp;
			}
			Compr.Flush();
			int OutputBufferLen = Compr.GetOutputLength();
			char* OutputBuffer = (char*)Compr.GetOutputBuffer();

			Stream->WriteInt(OutputBufferLen);

			Stream->WriteRawData(OutputBuffer, OutputBufferLen);
		}
		Stream->Flush();
#endif
		cout << "Data sent" << endl;
	}

} /* namespace TraceviewerServer */
