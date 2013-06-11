/*
 * DataOutputStream.h
 *
 *	The equivalent of the Java type DataOutputStream
 *
 *  Created on: Jul 9, 2012
 *      Author: pat2
 */

#ifndef DATAOUTPUTFILESTREAM_H_
#define DATAOUTPUTFILESTREAM_H_
#include <fstream>
#include "ByteUtilities.h"
namespace TraceviewerServer
{
	using namespace std;
	class DataOutputFileStream: public ofstream
	{
	public:
		DataOutputFileStream(const char*, openmode mode = ios_base::binary | ios_base::out);
		virtual ~DataOutputFileStream();
		void WriteInt(int);
		void WriteLong(Long);
	private:
	};

} /* namespace TraceviewerServer */
#endif /* DATAOUTPUTFILESTREAM_H_ */
