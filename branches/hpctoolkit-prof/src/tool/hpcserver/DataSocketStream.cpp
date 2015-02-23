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
#include <stdint.h>
#include <stdlib.h>

#include "DataSocketStream.hpp"
#include "ByteUtilities.hpp"
#include "Constants.hpp"
#include <include/big-endian.h>

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

		// create socket
		unopenedSocketFD = socket(PF_INET, SOCK_STREAM, 0);
		if (unopenedSocketFD < 0) {
			cerr << "unable to create socket" << endl;
			exit(1);
		}

		// add SO_REUSEADDR.  this allows binding to a recently closed
		// port (but not an active socket owned by another process).
		int optval = 1;
		setsockopt(unopenedSocketFD, SOL_SOCKET, SO_REUSEADDR,
			   &optval, sizeof(optval));

		// bind
		sockaddr_in Address;
		memset(&Address, 0, sizeof(Address));
		Address.sin_family = AF_INET;
		Address.sin_port = htons(_Port);
		Address.sin_addr.s_addr = INADDR_ANY;
		int err = bind(unopenedSocketFD, (sockaddr*) &Address, sizeof(Address));
		if (err != 0) {
			cerr << "unable to bind to port " << port
			     << ": " << strerror(errno) << endl;
			if (errno == EADDRINUSE) {
				cerr << "try another port, or else use '-p 0' "
				     << "to use an ephemeral port." << endl;
			}
			exit(1);
		}

		// listen
		err = listen(unopenedSocketFD, 5);
		if (err != 0) {
			cerr << "unable to listen on port " << port
			     << ": " << strerror(errno) << endl;
			exit(1);
		}

		if (Accept)
		{
			cout << "Waiting for connection on port " << getPort() << endl;
			acceptSocket();
		}
	}

	// return true on success, false on failure
	bool DataSocketStream::acceptSocket()
	{
		sockaddr_in client;
		unsigned int len = sizeof(client);

		socketDesc = accept(unopenedSocketFD, (sockaddr*) &client, &len);
		if (socketDesc < 0) {
			cerr << "error trying to accept connection: "
			     << strerror(errno) << endl;
			return false;
		}

		file = fdopen(socketDesc, "r+"); //read + write
		return true;
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
	
    	// close the sock fd for this connection but not the listen fd.
    	void DataSocketStream::closeSocket()
	{
		fclose(file);
		shutdown(socketDesc, SHUT_RDWR);
		close(socketDesc);
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
		union { char buf[4]; uint32_t ival; } u32;
		u32.ival = host_to_be_32(toWrite);
		fwrite(u32.buf, 4, 1, file);
	}
	void DataSocketStream::writeLong(Long toWrite)
	{
		union { char buf[8]; uint64_t ival; } u64;
		u64.ival = host_to_be_64(toWrite);
		fwrite(u64.buf, 8, 1, file);
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
		union { char buf[2]; uint16_t ival; } u16;
		u16.ival = host_to_be_16(toWrite);
		fwrite(u16.buf, 2, 1, file);
	}

	void DataSocketStream::flush()
	{
		int e = fflush(file);
		if (e == EOF)
			cerr << "Error on sending" << endl;
	}

	int DataSocketStream::readInt()
	{
		union { char buf[4]; uint32_t ival; } u32;

		long ret = fread(u32.buf, 1, 4, file);
		if (ret != 4) {
			throw ERROR_READ_TOO_LITTLE;
		}
		return (int) be_to_host_32(u32.ival);
	}

	Long DataSocketStream::readLong()
	{
		union { char buf[8]; uint64_t ival; } u64;

		long ret = fread(u64.buf, 1, 8, file);
		if (ret != 8) {
			throw ERROR_READ_TOO_LITTLE;
		}
		return (Long) be_to_host_64(u64.ival);
	}

	short DataSocketStream::readShort()
	{
		union { char buf[2]; uint16_t ival; } u16;

		long ret = fread(u16.buf, 1, 2, file);
		if (ret != 2) {
			throw ERROR_READ_TOO_LITTLE;
		}
		return (short) be_to_host_16(u16.ival);
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
		union { char buf[8]; uint64_t ival; double dval; } u64;

		long ret = fread(u64.buf, 1, 8, file);
		if (ret != 8) {
			throw ERROR_READ_TOO_LITTLE;
		}
		u64.ival = be_to_host_64(u64.ival);

		return u64.dval;
	}

	void DataSocketStream::writeDouble(double val)
	{
		union { char buf[8]; uint64_t ival; double dval; } u64;

		u64.dval = val;
		u64.ival = host_to_be_64(u64.ival);
		fwrite(u64.buf, 8, 1, file);
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
