/*
 * DataSocketStream.cpp
 *
 *  Created on: Jul 11, 2012
 *      Author: pat2
 */

#include <stdio.h>
#include <vector>
#include <iostream>
#include "ByteUtilities.hpp"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <errno.h>

#include "DataSocketStream.hpp"

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
			if (errno == 48|| errno == 98)
				cerr<< "Could not bind socket because socket is already in use. "<<
				"Make sure you have closed hpctraceviewer, wait 30 seconds and try again. " <<
				"Alternatively, you can choose a port other than "<< _Port<<"."<<endl;
			else
				cerr << "Could not bind socket. Error was " << errno << endl;
			throw 1111;
		}
		//listen
		err = listen(unopenedSocketFD, 5);
		if (err)
			cerr<<"Listen failed: " << errno<<endl;
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
		shutdown(socketDesc, SHUT_RDWR);
		close(socketDesc);
		close(unopenedSocketFD);
	}
	/*DataSocketStream& DataSocketStream::operator=(const DataSocketStream& rhs)
	 {
	 return *this;
	 }*/

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
		char Af[4];
		int err = fread(Af, 1, 4, file);
		if (err != 4)
			throw 0x5249;
		return ByteUtilities::readInt(Af);

	}

	Long DataSocketStream::readLong()
	{
		char Af[8];
		int err = fread(Af, 1, 8, file);
		if (err != 8)
			throw 0x524C;
		return ByteUtilities::readLong(Af);

	}

	short DataSocketStream::readShort()
	{
		char Af[2];
		int err = fread(Af, 1, 2, file);
		if (err != 2)
			throw 0x5253;
		return ByteUtilities::readShort(Af);
	}

	string DataSocketStream::readString()
	{

		short Len = readShort();

		char* Msg = new char[Len + 1];
		int err = fread(Msg, 1, Len, file);
		if (err != Len)
			throw 0x5254;

		Msg[Len] = '\0';

		//TODO: This is wrong for any path that requires special characters
		string SF(Msg);
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
			throw -7;
		}
		else if (e == -1)
			throw e; // Some other error.
	}

	SocketFD DataSocketStream::getDescriptor()
	{
		return socketDesc;
	}
} /* namespace TraceviewerServer */
