//#define USE_MPI
/*
 * Server.cpp
 *
 *  Created on: Jul 9, 2012
 *      Author: pat2
 */

#include "Server.hpp"
#include "DataSocketStream.hpp"
#include "DBOpener.hpp"
#include "Constants.hpp"
#include "MPICommunication.hpp"
#include "CompressingDataSocketLayer.hpp"
#include <iostream>
#include <fstream>
#include <mpi.h>
#include "zlib.h"
//#include "SpaceTimeDataController.hpp"

using namespace std;
using namespace MPI;
namespace TraceviewerServer
{
	bool useCompression = true;
	int mainPortNumber = DEFAULT_PORT;
	int xmlPortNumber = 0;



	Server::Server()
	{
		DataSocketStream* socketptr;
		try
		{
//TODO: Change the port to 21591 because 21590 has some other use...
			//DataSocketStream CLsocket = new DataSocketStream(21590);
			socketptr = new DataSocketStream(mainPortNumber, true);
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
			throw ERROR_STREAM_OPEN_FAILED;
		}
		mainPortNumber = socketptr->getPort();
		cout << "Received connection" << endl;

		//vector<char> test(4);
		//as::read(*socketptr, as::buffer(test));
		int command = socketptr->readInt();
		if (command == OPEN)
		{
			while( runConnection(socketptr)==START_NEW_CONNECTION_IMMEDIATELY)
				;
		}
		else
		{
			cerr << "Expected an open command, got " << command << endl;
			throw ERROR_EXPECTED_OPEN_COMMAND;
		}
	}

	Server::~Server()
	{
		delete (controller);
	}


	int Server::runConnection(DataSocketStream* socketptr)
	{
		parseOpenDB(socketptr);

		if (controller == NULL)
		{
			cout << "Could not open database" << endl;
			sendDBOpenFailed(socketptr);
			return ERROR_DB_OPEN_FAILED;
		}
		else
		{
			cout << "Database opened" << endl;
			sendDBOpenedSuccessfully(socketptr);
		}

		int Message = socketptr->readInt();
		if (Message == INFO)
			parseInfo(socketptr);
		else
			cerr << "Did not receive info packet" << endl;

		bool endConnection = false;
		while (!endConnection)
		{
			int nextCommand = socketptr->readInt();
			switch (nextCommand)
			{
				case DATA:
					getAndSendData(socketptr);
					break;
				case DONE:
					endConnection = true;
					break;
				case OPEN:
					endConnection = true;
					return START_NEW_CONNECTION_IMMEDIATELY;
				default:
					cerr << "Unknown command received" << endl;
					return (ERROR_UNKNOWN_COMMAND);
			}
		}

		return 0;
	}
	void Server::parseInfo(DataSocketStream* socket)
	{

		Long minBegTime = socket->readLong();
		Long maxEndTime = socket->readLong();
		int headerSize = socket->readInt();
		controller->setInfo(minBegTime, maxEndTime, headerSize);
#ifdef USE_MPI
		MPICommunication::CommandMessage Info;
		Info.command = INFO;
		Info.minfo.minBegTime = minBegTime;
		Info.minfo.maxEndTime = maxEndTime;
		Info.minfo.headerSize = headerSize;
		COMM_WORLD.Bcast(&Info, sizeof(Info), MPI_PACKED, MPICommunication::SOCKET_SERVER);
#endif
	}
	void Server::sendDBOpenedSuccessfully(DataSocketStream* socket)
	{

		socket->writeInt(DBOK);

		//int port = 2224;

		//socket->WriteInt(port);
		DataSocketStream* xmlSocket;
		if (xmlPortNumber == -1)
			xmlPortNumber = mainPortNumber;
		int actualXMLPort = xmlPortNumber;
		if (xmlPortNumber != mainPortNumber)
		{//On a different port. Create another socket.
			xmlSocket = new DataSocketStream(xmlPortNumber, false);
			actualXMLPort = xmlSocket->getPort();//Could be different if XMLPort is 0
		}
		else
		{
			//Same port, simply use this socket
			xmlSocket = socket;
		}
		socket->writeInt(actualXMLPort);

		int numFiles = controller->getNumRanks();
		socket->writeInt(numFiles);

		int compressionType; //This is an int so that it is possible to have different compression algorithms in the future. For now, it is just 0=no compression, 1= normal compression
		compressionType = useCompression ? 1 : 0;
		socket->writeInt(compressionType);

		//Send ValuesX
		int* rankProcessIds = controller->getValuesXProcessID();
		short* rankThreadIds = controller->getValuesXThreadID();
		for (int i = 0; i < numFiles; i++)
		{
			socket->writeInt(rankProcessIds[i]);
			socket->writeShort(rankThreadIds[i]);
		}

		socket->flush();

		cout << "Waiting to send XML on port " << actualXMLPort << endl;
		if (actualXMLPort != mainPortNumber)
		{
			xmlSocket->acceptSocket();
		}
		else
		{
			xmlSocket->writeInt(EXML);
		}
		vector<char> compressedXML = compressXML();
		xmlSocket->writeInt(compressedXML.size());
		xmlSocket->writeRawData(&compressedXML[0], compressedXML.size());
		xmlSocket->flush();
		cout << "XML Sent" << endl;
		if(actualXMLPort != mainPortNumber)
			delete (xmlSocket);
	}

	vector<char> Server::compressXML()
	{
		vector<char> compressed;

		FILE* in = fopen(controller->getExperimentXML().c_str(), "r");
		//From http://zlib.net/zpipe.c with some editing
		z_stream compressor;
		compressor.zalloc = Z_NULL;
		compressor.zfree = Z_NULL;
		compressor.opaque = Z_NULL;
		//int ret = deflateInit(&compressor, -1);
		//This makes a gzip stream with a window of 15 bits
		int ret = deflateInit2(&compressor, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16+15, 8, Z_DEFAULT_STRATEGY);
		if (ret != Z_OK)
			throw ret;

		#define CHUNK 0x4000

		unsigned char InBuffer[CHUNK];
		unsigned char OutBuffer[CHUNK];
		int flush;
		int amtprocessed =0;
		int compramt;
		do {
			compressor.avail_in = fread(InBuffer, 1, CHUNK, in);
			amtprocessed += compressor.avail_in;
			//cout<<"Processed " << amtprocessed << " / " <<size<<endl;
			if (ferror(in)) {
				cerr<<"Error"<<endl;
				(void)deflateEnd(&compressor);
				throw Z_ERRNO;
			}
			flush = feof(in) ? Z_FINISH : Z_NO_FLUSH;

			compressor.next_in = InBuffer;
			int have;
			/* run deflate() on input until output buffer not full, finish
		           compression if all of source has been read in */
			do {
				compressor.avail_out = CHUNK;
				compressor.next_out = OutBuffer;
				ret = deflate(&compressor, flush);    /* no bad return value */
				if (ret == Z_STREAM_ERROR) throw ERROR_COMPRESSION_FAILED;  /* state not clobbered */
				have = CHUNK - compressor.avail_out;
				compramt += have;
				//cout<<"Writing "<<have<< " compressed bytes. Length of compressed file so far is "<< compramt<<endl;
				compressed.insert(compressed.end(), OutBuffer, OutBuffer+have);
			} while (compressor.avail_out == 0);


			/* done when last data in file processed */
		} while (flush != Z_FINISH);
		deflateEnd(&compressor);
		cout<<"Compressed XML Size: "<<compressed.size()<<endl;
		return compressed;
	}

	void Server::parseOpenDB(DataSocketStream* receiver)
	{

		string pathToDB = receiver->readString();
		DBOpener DBO;
		cout << "Opening database: " << pathToDB << endl;
		controller = DBO.openDbAndCreateStdc(pathToDB);
#ifdef USE_MPI
		if (controller != NULL)
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
#endif


	}

	void Server::sendDBOpenFailed(DataSocketStream* socket)
	{
		socket->writeInt(NODB);
		socket->writeInt(0);
		socket->flush();
	}

#define ISN(g) (g<0)

	void Server::getAndSendData(DataSocketStream* Stream)
	{

		int processStart = Stream->readInt();
		int processEnd = Stream->readInt();
		double timeStart = Stream->readDouble();
		double timeEnd = Stream->readDouble();
		int verticalResolution = Stream->readInt();
		int horizontalResolution = Stream->readInt();
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
		toBcast.command = DATA;
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
		controller->Attributes = &correspondingAttributes;

		// TODO: Make this so that the Lines get sent as soon as they are
		// filled.

		controller->FillTraces(-1, true);
#endif

		Stream->writeInt(HERE);
		Stream->flush();


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
			if (msg.tag == SLAVE_REPLY)
			{

				//Stream->WriteInt(msg.Data.Line);
				Stream->writeInt(msg.data.line);
				Stream->writeInt(msg.data.entries);
				Stream->writeDouble(msg.data.begtime); // Begin time
				Stream->writeDouble(msg.data.endtime); //End time
				Stream->writeInt(msg.data.compressedSize);

				char CompressedTraceLine[msg.data.compressedSize];
				COMM_WORLD.Recv(CompressedTraceLine, msg.data.compressedSize, MPI_BYTE, msg.data.rankID,
						MPI_ANY_TAG);

				Stream->writeRawData(CompressedTraceLine, msg.data.compressedSize);

				//delete(&msg);
				Stream->flush();
			}
			else if (msg.tag == SLAVE_DONE)
			{
				cout << "Rank " << msg.done.rankID << " done" << endl;
				RanksDone++;
			}
		}


#else
		for (int i = 0; i < controller->TracesLength; i++)
		{

			ProcessTimeline* T = controller->Traces[i];
			Stream->writeInt( T->line());
			vector<TimeCPID> data = T->data->ListCPID;
			Stream->writeInt( data.size());
			Stream->writeDouble( data[0].Timestamp);
			// Begin time
			Stream->writeDouble( data[data.size() - 1].Timestamp);
			//End time
			CompressingDataSocketLayer Compr;

			vector<TimeCPID>::iterator it;
			cout << "Sending process timeline with " << data.size() << " entries" << endl;
			
			double currentTime = data[0].Timestamp;
			for (it = data.begin(); it != data.end(); ++it)
			{
				Compr.writeInt( (int)(it->Timestamp - currentTime));
				Compr.writeInt( it->CPID);
				currentTime = it->Timestamp;
			}
			Compr.flush();
			int OutputBufferLen = Compr.GetOutputLength();
			char* OutputBuffer = (char*)Compr.GetOutputBuffer();

			Stream->writeInt(OutputBufferLen);

			Stream->writeRawData(OutputBuffer, OutputBufferLen);
		}
		Stream->flush();
#endif
		cout << "Data sent" << endl;
	}

} /* namespace TraceviewerServer */
