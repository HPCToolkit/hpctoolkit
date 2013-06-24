/*
 * DataSocketStream.h
 *
 *  Created on: Jul 11, 2012
 *      Author: pat2
 */

#ifndef DATASOCKETSTREAM_H_
#define DATASOCKETSTREAM_H_

#include <string>

#include "ByteUtilities.hpp"

namespace TraceviewerServer
{
	using namespace std;

	typedef int SocketFD;
	class DataSocketStream
	{
	public:
		DataSocketStream(int, bool);
		void acceptSocket();
		DataSocketStream();

		int getPort();

		virtual ~DataSocketStream();

		virtual void writeInt(int);
		virtual void writeLong(Long);
		virtual void writeDouble(double);
		virtual void writeRawData(char*, int);
		virtual void writeString(string);
		virtual void writeShort(short);

		virtual void flush();

		int readInt();
		Long readLong();
		string readString();
		double readDouble();
		short readShort();
		char readByte();

		SocketFD getDescriptor();
	private:
		int port;
		SocketFD socketDesc;
		SocketFD unopenedSocketFD;
		void checkForErrors(int);
		FILE* file;
	};

} /* namespace TraceviewerServer */
#endif /* DATASOCKETSTREAM_H_ */
