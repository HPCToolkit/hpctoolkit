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

#include "Server.hpp"
#include "DataSocketStream.hpp"
#include "DBOpener.hpp"
#include "Constants.hpp"
#include "MPICommunication.hpp"
#include "CompressingDataSocketLayer.hpp"
#include "ProgressBar.hpp"
#include "FileUtils.hpp"
#include "Communication.hpp"

#include <iostream>
#include <fstream>

#include <mpi.h>
#include "zlib.h"


using namespace std;
using namespace MPI;
namespace TraceviewerServer
{
	bool useCompression = true;
	int mainPortNumber = DEFAULT_PORT;
	int xmlPortNumber = 0;



	Server::Server()
	{
		DataSocketStream* socketptr = NULL;
		try
		{
			//Port 21590 is used by vofr-gateway. Do we want to change it?
			socketptr = new DataSocketStream(mainPortNumber, true);

		} catch (std::exception& e)
		{
			std::cerr << e.what() << std::endl;
			throw ERROR_STREAM_OPEN_FAILED;
		}
		mainPortNumber = socketptr->getPort();
		cout << "Received connection" << endl;


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
				case FLTR:
					filter(socketptr);
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

		return CLOSE_SERVER;
	}
	void Server::parseInfo(DataSocketStream* socket)
	{

		Long minBegTime = socket->readLong();
		Long maxEndTime = socket->readLong();
		int headerSize = socket->readInt();
		controller->setInfo(minBegTime, maxEndTime, headerSize);

		Communication::sendParseInfo(minBegTime, maxEndTime, headerSize);//Send to MPI if necessary
	}
	void Server::sendDBOpenedSuccessfully(DataSocketStream* socket)
	{

		socket->writeInt(DBOK);

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

		xmlSocket->writeInt(EXML);
		vector<char> compressedXML;
		compressXML(compressedXML);
		xmlSocket->writeInt(compressedXML.size());

		//To be more C++-ish, DataSocketStream would have an overload that takes
		//an iterator. Unfortunately, behind the scenes, DataSocketStream uses
		//fwrite and other C functions, which don't take iterators.
		xmlSocket->writeRawData(&compressedXML[0], compressedXML.size());
		xmlSocket->flush();

		cout << "XML Sent" << endl;
		if(actualXMLPort != mainPortNumber)
			delete (xmlSocket);
	}

	//Pass by reference to avoid copying the whole vector...
	void Server::compressXML(vector<char>& compressed)
	{

		//If this overflows, we may have problems...
		int uncompressedFileSize = FileUtils::getFileSize(controller->getExperimentXML());
		ProgressBar prog("Compressing XML", uncompressedFileSize);
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

		int compramt;
		do {
			int amtprocessed = fread(InBuffer, 1, CHUNK, in);
			compressor.avail_in = amtprocessed;
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

			prog.incrementProgress(amtprocessed);
			/* done when last data in file processed */
		} while (flush != Z_FINISH);
		deflateEnd(&compressor);

		DEBUGCOUT(2)<<"Compressed XML Size: "<<compressed.size()<<endl;

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

	void Server::parseOpenDB(DataSocketStream* receiver)
	{
		checkProtocolVersions(receiver);

		string pathToDB = receiver->readString();
		DBOpener DBO;
		cout << "Opening database: " << pathToDB << endl;
		controller = DBO.openDbAndCreateStdc(pathToDB);

		if (controller != NULL)
		{
			Communication::sendParseOpenDB(pathToDB);
		}



	}

	void Server::sendDBOpenFailed(DataSocketStream* socket)
	{
		socket->writeInt(NODB);
		socket->writeInt(0);
		socket->flush();
	}



	void Server::getAndSendData(DataSocketStream* stream)
	{

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
			cout << filt.processMin <<":" << filt.processMax <<":"<<filt.processStride<<",";
			cout << filt.threadMax <<":" << filt.threadMax <<":"<<filt.threadStride<<endl;

			Communication::sendFilter(filt);

			filters.add(Filter(filt));
			controller->applyFilters(filters);
		}
	}

} /* namespace TraceviewerServer */
