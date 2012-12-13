/*
 * DataSocketStream.h
 *
 *  Created on: Jul 11, 2012
 *      Author: pat2
 */

#ifndef DATASOCKETSTREAM_H_
#define DATASOCKETSTREAM_H_

#include <string>

#include "ByteUtilities.h"

namespace TraceviewerServer
{
	using namespace std;

	typedef int SocketFD;
	class DataSocketStream
	{
	public:
		DataSocketStream(int, bool);
		void AcceptS();
		DataSocketStream();

		int GetPort();

		virtual ~DataSocketStream();

		virtual void WriteInt(int);
		virtual void WriteLong(Long);
		virtual void WriteDouble(double);
		virtual void WriteRawData(char*, int);
		virtual void WriteString(string);
		virtual void WriteShort(short);

		virtual void Flush();

		int ReadInt();
		Long ReadLong();
		string ReadString();
		double ReadDouble();
		short ReadShort();

		SocketFD GetDescriptor();
	private:
		int Port;
		SocketFD socketDesc;
		SocketFD unopenedSocketFD;
		void CheckForErrors(int);
		FILE* file;
	};

} /* namespace TraceviewerServer */
#endif /* DATASOCKETSTREAM_H_ */
