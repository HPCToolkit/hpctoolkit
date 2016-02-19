/// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/Server.cpp $
// $Id: Server.cpp 4461 2014-03-12 16:49:28Z laksono@gmail.com $
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
//   $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/Server.cpp $
//
// Purpose:
//   Handles the socket communication for the most part and the conversion
//   between messages and work to be done
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#include "Server.hpp"
#include "DataSocketStream.hpp"
#include "DBOpener.hpp"
#include "Constants.hpp"
#include "DataCompressionLayer.hpp"
#include "ProgressBar.hpp"
#include "FileUtils.hpp"
#include "Communication.hpp"
#include "DebugUtils.hpp"
#include "Filter.hpp"
#include "FilterSet.hpp"
#include "SpaceTimeDataController.hpp"
#include "TimeCPID.hpp" //For Time

#ifdef HPCTOOLKIT_PROFILE
 #include "hpctoolkit.h"
#endif

#include <iostream>
#include <cstdio>
#include <zlib.h>
#include <algorithm> //for min of int64_t
#include <string>

using namespace std;

namespace TraceviewerServer
{
	bool useCompression = true;
	int mainPortNumber = DEFAULT_PORT;
	int xmlPortNumber = 0;

	Server::Server()
	{
		DataSocketStream* socketptr = NULL;
		DataSocketStream* xmlSocketPtr = NULL;

		//Port 21590 is used by vofr-gateway. Do we want to change it?
		DataSocketStream socket(mainPortNumber, true);
		socketptr = &socket;

		mainPortNumber = socketptr->getPort();
		cout << "Received connection" << endl;

		int command = socketptr->readInt();
		if (command == OPEN)
		{
 			// Laksono 2014.11.11: Somehow the class Args.cpp cannot accept -1 as an integer argument
	 		// (not sure if this is a feature or it's a bug to confuse between a flag and negative number)
 			// Temporary, we can specify that if the xml port is 1 then it will be the same as the main port
			if (xmlPortNumber == 1)
			  xmlPortNumber = mainPortNumber;

			if (xmlPortNumber != mainPortNumber)
			{//On a different port. Create another socket.
			  xmlSocketPtr = new DataSocketStream(xmlPortNumber, false);
			}
			else
			{
			  //Same port, simply use this socket
			  xmlSocketPtr = socketptr;
			}

			// ----------------------------------------------------------------------------------
			// Loop for the Server: As long as the client doesn't close the socket communication
			// 			we'll remain in this loop
			// ----------------------------------------------------------------------------------

			while( runConnection(socketptr, xmlSocketPtr)==START_NEW_CONNECTION_IMMEDIATELY) ;

			if (xmlPortNumber != mainPortNumber)
			{
			  delete (xmlSocketPtr);
			}
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


	int Server::runConnection(DataSocketStream* socketptr, DataSocketStream* xmlSocket)
	{
#ifdef HPCTOOLKIT_PROFILE
		hpctoolkit_sampling_start();
#endif
		controller = parseOpenDB(socketptr);

		if (controller == NULL)
		{
			cout << "Could not open database" << endl;
			sendDBOpenFailed(socketptr);
			//Now wait until we get the next tag. If we don't read it here, the message stream will
			//be off by 4 bytes because parseOpenDB (which reads from the stream next) does not expect OPEN.
			int tag = socketptr->readInt();
			if (tag == OPEN)
				return START_NEW_CONNECTION_IMMEDIATELY;
			else
				return CLOSE_SERVER;
		}
		else
		{
			DEBUGCOUT(1) << "Database opened" << endl;
			sendDBOpenedSuccessfully(socketptr, xmlSocket);
		}

		int Message = socketptr->readInt();
		if (Message == INFO)
			parseInfo(socketptr);
		else
			cerr << "Did not receive info packet" << endl;
	
#ifdef HPCTOOLKIT_PROFILE
		hpctoolkit_sampling_stop();
#endif
		// ------------------------------------------------------------------
		// main loop for a communication session
		// as long as the client doesn't send OPEN or DONE, we remain in 
		// in this loop 
		// ------------------------------------------------------------------
		while (true)
		{
			int nextCommand = socketptr->readInt();
			switch (nextCommand)
			{
				case DATA:
#ifdef HPCTOOLKIT_PROFILE
					hpctoolkit_sampling_start();
#endif
					getAndSendData(socketptr);
#ifdef HPCTOOLKIT_PROFILE
					hpctoolkit_sampling_stop();
#endif
					break;
				case FLTR:
#ifdef HPCTOOLKIT_PROFILE
					hpctoolkit_sampling_start();
#endif
					filter(socketptr);
#ifdef HPCTOOLKIT_PROFILE
					hpctoolkit_sampling_stop();
#endif
					break;
				case DONE:
					return CLOSE_SERVER;
				case OPEN:
					return START_NEW_CONNECTION_IMMEDIATELY;
				default:
					cerr << "Unknown command received" << endl;
					return ERROR_UNKNOWN_COMMAND;
			}
		}

		return CLOSE_SERVER;
	}
	void Server::parseInfo(DataSocketStream* socket)
	{

		Time minBegTime = socket->readLong();
		Time maxEndTime = socket->readLong();
		int headerSize = socket->readInt();
		controller->setInfo(minBegTime, maxEndTime, headerSize);

		Communication::sendParseInfo(minBegTime, maxEndTime, headerSize);//Send to MPI if necessary
	}

	void Server::sendDBOpenedSuccessfully(DataSocketStream* socket, DataSocketStream* xmlSocket)
	{
		socket->writeInt(DBOK);
 	
		int actualXMLPort = xmlSocket->getPort();
		socket->writeInt(actualXMLPort);

		int numFiles = controller->getNumRanks();
		socket->writeInt(numFiles);

		// This is an int so that it is possible to have different compression
		// algorithms in the future. For now, it is just
		// 0=no compression, 1= normal compression
		int compressionType;
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


		sendXML(xmlSocket);
	}

	void Server::sendXML(DataSocketStream* xmlSocket)
	{
		xmlSocket->writeInt(EXML);
		//If this overflows, we may have problems...
		int uncompressedFileSize = FileUtils::getFileSize(controller->getExperimentXML());
		//Set up a subscope so that the ProgressBar will get deleted (which
		//cleans up the output) before we write to cout again.
		{
			ProgressBar prog("Compressing XML", uncompressedFileSize);
			FILE* in = fopen(controller->getExperimentXML().c_str(), "r");
			//From http://zlib.net/zpipe.c with some editing
			z_stream compressor;
			compressor.zalloc = Z_NULL;
			compressor.zfree = Z_NULL;
			compressor.opaque = Z_NULL;

			//This makes a gzip stream with a window of 15 bits
			int ret = deflateInit2(&compressor, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16+15, 8, Z_DEFAULT_STRATEGY);
			if (ret != Z_OK)
				throw ret;

			DataCompressionLayer compL(compressor, &prog);
			compL.writeFile(in);

			fclose(in);
			int compressedSize = compL.getOutputLength();
			DEBUGCOUT(2)<<"Compressed XML Size: "<<compressedSize<<endl;

			xmlSocket->writeInt(compressedSize);
			xmlSocket->writeRawData((char*)compL.getOutputBuffer(), compressedSize);

			xmlSocket->flush();
		}
		cout << "XML Sent" << endl;
	}

	void Server::checkProtocolVersions(DataSocketStream* receiver)
	{
		int clientProtocolVersion = receiver->readInt();

		if (clientProtocolVersion != SERVER_PROTOCOL_MAX_VERSION)
			cout << "The client is using protocol version 0x" << hex << clientProtocolVersion<<
			" and the server supports version 0x" << SERVER_PROTOCOL_MAX_VERSION << endl;

		if (clientProtocolVersion < SERVER_PROTOCOL_MAX_VERSION) {
			cout << "Warning: The server is running in compatibility mode." << endl;
			agreedUponProtocolVersion = clientProtocolVersion;
		}
		else if (clientProtocolVersion > SERVER_PROTOCOL_MAX_VERSION) {
			cout << "The client protocol version is not supported by this server."<<
					"Please upgrade the server. This session may be buggy and problematic." << endl;
			agreedUponProtocolVersion = SERVER_PROTOCOL_MAX_VERSION;
		}
		cout << dec;//Switch it back to decimal mode
	}

	SpaceTimeDataController* Server::parseOpenDB(DataSocketStream* receiver)
	{
		checkProtocolVersions(receiver);

		string pathToDB = receiver->readString();
		DBOpener DBO;
		cout << "Opening database: " << pathToDB << endl;
		SpaceTimeDataController* controller = DBO.openDbAndCreateStdc(pathToDB);

		if (controller != NULL)
		{
			Communication::sendParseOpenDB(pathToDB);
		}

		return controller;

	}

	void Server::sendDBOpenFailed(DataSocketStream* socket)
	{
		socket->writeInt(NODB);
		socket->writeInt(0);
		socket->flush();
	}



	void Server::getAndSendData(DataSocketStream* stream)
	{
		LOGTIMESTAMPEDMSG("Front end received data request.")
		int processStart = stream->readInt();
		int processEnd = stream->readInt();
		Time timeStart = stream->readLong();
		Time timeEnd = stream->readLong();
		int verticalResolution = stream->readInt();
		int horizontalResolution = stream->readInt();

		DEBUGCOUT(2) << "Time end: " << timeEnd <<endl;


		if ((processStart < 0) || (processEnd<0) || (processStart > processEnd)
				|| (verticalResolution<0) || (horizontalResolution<0)
				|| (timeEnd < timeStart))
		{
			cerr
					<< "A data request with invalid parameters was received. This sometimes happens if the client shuts down in the middle of a request. The server will now shut down."
					<< endl;
			throw(ERROR_INVALID_PARAMETERS);
		}
		Communication::sendStartGetData(controller, processStart, processEnd, timeStart, timeEnd, verticalResolution, horizontalResolution);
		LOGTIMESTAMPEDMSG("Back end received data request.")

		stream->writeInt(HERE);
		stream->flush();

		ProgressBar prog("Computing traces", min(processEnd - processStart, verticalResolution));

		Communication::sendEndGetData(stream, &prog, controller);

	}

	void Server::filter(DataSocketStream* stream)
	{
		stream->readByte();//Padding
		bool excludeMatches = stream->readByte();
		int count = stream->readShort();
		Communication::sendStartFilter(count, excludeMatches);
		FilterSet filters(excludeMatches);
		for (int i = 0; i < count; ++i) {
			BinaryRepresentationOfFilter filt;//This makes the MPI code easier and the non-mpi code about the same
			filt.processMin = stream->readInt();
			filt.processMax = stream->readInt();
			filt.processStride = stream->readInt();
			filt.threadMin = stream->readInt();
			filt.threadMax = stream->readInt();
			filt.threadStride = stream->readInt();
			DEBUGCOUT(2) << "Filter proc: " << filt.processMin <<":" << filt.processMax <<":"<<filt.processStride<<",";
			DEBUGCOUT(2) << "Filter thread: " << filt.threadMax <<":" << filt.threadMax <<":"<<filt.threadStride<<endl;

			Communication::sendFilter(filt);

			filters.add(Filter(filt));
		}
		controller->applyFilters(filters);
	}

} /* namespace TraceviewerServer */
