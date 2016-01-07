// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/DataSocketStream.cpp $
// $Id: DataSocketStream.cpp 4306 2013-07-17 21:19:13Z laksono@gmail.com $
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
//   $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/DataSocketStream.cpp $
//
// Purpose:
//   An approximate implementation of Java's DataOutputStream, DataInputStream,
//   combined with the actual socket code to make it all work. Provides useful,
//   yet fairly direct access to the socket.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#include <stdio.h>//fread, fwrite, etc.
#include <cstring> //For memset
#include <iostream>
#include <string>//for string
#include <cstring>//for strerror

#include <sys/socket.h>
#include <unistd.h> // close socket
#include <arpa/inet.h> //htons
#include <sys/types.h>
#include <netinet/in.h>
#include <errno.h>

#include "DataSocketStream.hpp"
#include "ByteUtilities.hpp"
#include "Constants.hpp"

namespace TraceviewerServer
{
	using namespace std;

	DataSocketStream::DataSocketStream()
	{
		//Do nothing because this is used when the CompressingDataSocket is constructed, which means we already have a socket constructed that we want to use
	}

	DataSocketStream::DataSocketStream(int _Port, bool Accept = true)
	{
		port = _Port;
		
		unopenedSocketFD = socket(PF_INET, SOCK_STREAM, 0);
		if (unopenedSocketFD == -1)
			cerr << "Could not create socket" << endl;
		//bind
		sockaddr_in Address;
		memset(&Address, 0, sizeof(Address));
		Address.sin_family = AF_INET;
		Address.sin_port = htons(_Port);
		Address.sin_addr.s_addr = INADDR_ANY;
		int err = bind(unopenedSocketFD, (sockaddr*) &Address, sizeof(Address));
		if (err)
		{
			if (errno == EADDRINUSE)
				cerr<< "Could not bind socket because socket is already in use. "<<
				"Make sure you have closed hpctraceviewer, wait 30 seconds and try again. " <<
				"Alternatively, you can choose a port other than "<< _Port<<"."<<endl;
			else
				cerr << "Could not bind socket. Error was " << strerror(errno) << endl;
			throw ERROR_SOCKET_IN_USE;
		}
		//listen
		err = listen(unopenedSocketFD, 5);
		if (err)
			cerr<<"Listen failed: " << strerror(errno)<<endl;
		if (Accept)
		{
			cout << "Waiting for connection on port " << getPort() << endl;
			acceptSocket();
		}
	}
	void DataSocketStream::acceptSocket()
	{
		//accept
		sockaddr_in client;
		unsigned int len = sizeof(client);
		socketDesc = accept(unopenedSocketFD, (sockaddr*) &client, &len);
		if (socketDesc < 0)
			cerr << "Error on accept" << endl;
		file = fdopen(socketDesc, "r+b"); //read, write, binary
	}

	int DataSocketStream::getPort()
	{
		if (port == 0)
		{
			//http://stackoverflow.com/questions/4046616/sockets-how-to-find-out-what-port-and-address-im-assigned
			struct sockaddr_in sin;
			socklen_t len = sizeof(sin);
			if (getsockname(unopenedSocketFD, (struct sockaddr *) &sin, &len) == -1)
			{
				cerr << "Could not obtain port" << endl;
				return 0;
			}
			else
				return ntohs(sin.sin_port);
		}
		else
		{
			return port;
		}
	}
	
	DataSocketStream::~DataSocketStream()
	{
		fclose(file);
		shutdown(socketDesc, SHUT_RDWR);
		close(socketDesc);
		close(unopenedSocketFD);
	}

	void DataSocketStream::writeInt(int toWrite)
	{
		char Buffer[4];
		ByteUtilities::writeInt(Buffer, toWrite);
		fwrite(Buffer, 4, 1, file);
	}
	void DataSocketStream::writeLong(Long toWrite)
	{
		char Buffer[8];
		ByteUtilities::writeLong(Buffer, toWrite);
		fwrite(Buffer, 8, 1, file);
	}

	void DataSocketStream::writeRawData(char* Data, int Length)
	{
		int r = fwrite(Data, 1, Length, file);
		if (r != Length)
			cerr<<"Only wrote "<<r << " / " <<Length<<endl;
	}

	void DataSocketStream::writeString(string toWrite)
	{
		writeShort(toWrite.length());
		fwrite(toWrite.c_str(), toWrite.length(), 1, file);
	}

	void DataSocketStream::writeShort(short toWrite)
	{
		char Buffer[2];
		ByteUtilities::writeShort(Buffer, toWrite);
		fwrite(Buffer, 2, 1, file);
	}

	void DataSocketStream::flush()
	{
		int e = fflush(file);
		if (e == EOF)
			cerr << "Error on sending" << endl;
	}

	int DataSocketStream::readInt()
	{
		char Af[SIZEOF_INT];
		int err = fread(Af, 1, SIZEOF_INT, file);
		if (err != SIZEOF_INT)
			throw ERROR_READ_TOO_LITTLE;
		return ByteUtilities::readInt(Af);

	}

	Long DataSocketStream::readLong()
	{
		char Af[SIZEOF_LONG];
		int err = fread(Af, 1, SIZEOF_LONG, file);
		if (err != SIZEOF_LONG)
			throw ERROR_READ_TOO_LITTLE;
		return ByteUtilities::readLong(Af);

	}

	short DataSocketStream::readShort()
	{
		char Af[SIZEOF_SHORT];
		int err = fread(Af, 1, SIZEOF_SHORT, file);
		if (err != 2)
			throw ERROR_READ_TOO_LITTLE;
		return ByteUtilities::readShort(Af);
	}
	char DataSocketStream::readByte()
	{
		char Af[SIZEOF_BYTE];
		int err = fread(Af, 1, SIZEOF_BYTE, file);
		if (err != 1)
			throw ERROR_READ_TOO_LITTLE;
		return Af[0];
	}

	string DataSocketStream::readString()
	{

		short Len = readShort();

		char* Msg = new char[Len + 1];
		int err = fread(Msg, 1, Len, file);
		if (err != Len)
			throw ERROR_READ_TOO_LITTLE;

		Msg[Len] = '\0';

		string SF(Msg);
		//The string constructor copies the array, and we don't want to leak the memory
		delete[] Msg;
		return SF;
	}

	double DataSocketStream::readDouble()
	{
		Long longForm = readLong();
		return ByteUtilities::convertLongToDouble(longForm);
	}
	void DataSocketStream::writeDouble(double val)
	{
		Long longForm = ByteUtilities::convertDoubleToLong(val);
		writeLong(longForm);
	}

	void DataSocketStream::checkForErrors(int e)
	{
		if (e == 0)
		{
			cout << "Connection closed" << endl; // EOF
			throw ERROR_STREAM_CLOSED;
		}
		else if (e == -1)
			throw e; // Some other error.
	}

	SocketFD DataSocketStream::getDescriptor()
	{
		return socketDesc;
	}
} /* namespace TraceviewerServer */
