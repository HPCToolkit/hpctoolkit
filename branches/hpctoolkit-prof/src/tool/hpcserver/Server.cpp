/// -*-Mode: C++;-*-

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
// Copyright ((c)) 2002-2015, Rice University
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
//   Handles the socket communication for the most part and the conversion
//   between messages and work to be done
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#include <sys/time.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>

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

static sigjmp_buf alarm_jbuf;
static int jbuf_active = 0;

// catch sig alarm and return to server
static void
sigalarm_handler(int sig)
{
	if (jbuf_active) {
		siglongjmp(alarm_jbuf, 1);
	}
}

using namespace std;

namespace TraceviewerServer
{
	int mainPortNumber = DEFAULT_PORT;
	int xmlPortNumber = 0;
	int timeout = 15;
	bool useCompression = true;
	bool stayOpen = true;

	Server::Server()
	{
		DataSocketStream socket(mainPortNumber, false);
		DataSocketStream* socketptr = &socket;

		mainPortNumber = socketptr->getPort();
		controller = NULL;

		// setup sig alarm handler for timeout
		struct itimerval alarm_start, alarm_stop;
		struct sigaction act;

		memset(&alarm_start, 0, sizeof(alarm_start));
		memset(&alarm_stop, 0, sizeof(alarm_stop));
		alarm_start.it_value.tv_sec = 60 * timeout;

		memset(&act, 0, sizeof(act));
		act.sa_handler = sigalarm_handler;
		act.sa_flags = 0;
		sigemptyset(&act.sa_mask);
		if (timeout > 0) {
			sigaction(SIGALRM, &act, NULL);
		}

		// allow sequence of connections
		do {
		    	cout << "\nListening for connection on port "
			     << mainPortNumber << " ..." << endl;

			// jump buf for return from timeout
			jbuf_active = 0;
			if (timeout > 0) {
				if (sigsetjmp(alarm_jbuf, 1) == 0) {
					// normal return
					jbuf_active = 1;
				} else {
					// return from timeout
					jbuf_active = 0;
					cout << "Timeout waiting for connection" << endl;
					setitimer(ITIMER_REAL, &alarm_stop, NULL);
					break;
				}
			}

			// wait on accept() with timeout
			if (timeout > 0) {
				setitimer(ITIMER_REAL, &alarm_start, NULL);
			}
			socketptr->acceptSocket();

			// turn off alarm
			jbuf_active = 0;
			if (timeout > 0) {
				setitimer(ITIMER_REAL, &alarm_stop, NULL);
			}

			cout << "Received connection from "
			     << socketptr->getClientIP() << endl;

			try {
			    	startConnection(socketptr);
			}
			catch (...) {
			}
			socketptr->closeSocket();
		}
		while (stayOpen);
	}

	Server::~Server()
	{
		if (controller != NULL) {
			delete (controller);
		}
	}

    	void Server::startConnection(DataSocketStream* socketptr)
	{
		DataSocketStream* xmlSocketPtr = NULL;

		int command = socketptr->readInt();
		if (command == OPEN)
		{
		    	// port 1 means use same connection as main port
		    	if (xmlPortNumber == 1) {
				xmlPortNumber = mainPortNumber;
			}

			// create new socket if different port
			if (xmlPortNumber != mainPortNumber) {
			    	xmlSocketPtr = new DataSocketStream(xmlPortNumber, false);
			}
			else {
			    	// Same port, simply use this socket
			    	xmlSocketPtr = socketptr;
			}

			// stay in this loop as long as the connection stays open
			while( runConnection(socketptr, xmlSocketPtr)
			       == START_NEW_CONNECTION_IMMEDIATELY ) ;

			if (xmlPortNumber != mainPortNumber) {
			  	delete (xmlSocketPtr);
			}
		}
		else {
			cerr << "Expected an open command, got " << command << endl;
			throw ERROR_EXPECTED_OPEN_COMMAND;
		}
	}

	int Server::runConnection(DataSocketStream* socketptr, DataSocketStream* xmlSocket)
	{
#ifdef HPCTOOLKIT_PROFILE
		hpctoolkit_sampling_start();
#endif
		if (controller != NULL) {
			delete controller;
		}
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

		// for now, we only support one protocol version, so
		// treat it as a magic number.
		if (clientProtocolVersion != SERVER_PROTOCOL_MAX_VERSION) {
			cout << "The client is using protocol version 0x" << hex << clientProtocolVersion<<
			" and the server supports version 0x" << SERVER_PROTOCOL_MAX_VERSION << endl;
			cout << dec;
			throw ERROR_INVALID_PROTOCOL;
		}

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
		cout << "\nOpening database: " << pathToDB << endl;
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
