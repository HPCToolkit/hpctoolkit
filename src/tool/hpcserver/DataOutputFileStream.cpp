/*
 * DataOutputFileStream.cpp
 *
 *  Created on: Jul 9, 2012
 *      Author: pat2
 */

#include "DataOutputFileStream.hpp"

using namespace std;
namespace TraceviewerServer
{

//ostream* Backer;
	DataOutputFileStream::DataOutputFileStream(const char* filename, openmode mode) :
			ofstream(filename, mode)
	{

	}

	DataOutputFileStream::~DataOutputFileStream()
	{

	}
	void DataOutputFileStream::writeInt(int toWrite)
	{

		char arrayform[4];
		ByteUtilities::writeInt(arrayform, toWrite);
		write(arrayform, 4);
	}
	void DataOutputFileStream::writeLong(Long toWrite)
	{
		char arrayform[8];
		ByteUtilities::writeLong(arrayform, toWrite);
		write(arrayform, 8);
	}

} /* namespace TraceviewerServer */
