/*
 * CompressingDataSocketLayer.cpp
 *
 *  Created on: Jul 25, 2012
 *      Author: pat2
 */


#include "ByteUtilities.hpp"
#include <iostream>
#include "CompressingDataSocketLayer.hpp"

using namespace std;
namespace TraceviewerServer
{

#define Room(a) if ((a) + bufferIndex > BUFFER_SIZE) softFlush();

	CompressingDataSocketLayer::CompressingDataSocketLayer(
		)
	{
		//Compressor = gzdopen(BackingSocket->GetDescriptor(), "w");
		//Compressor = gzopen("/Users/pat2/Desktop/test8.gz", "wb");

		//See: http://www.zlib.net/zpipe.c
		int ret;

		bufferIndex = 0;
		posInCompBuffer = 0;

		compressor.zalloc = Z_NULL;
		compressor.zfree = Z_NULL;
		compressor.opaque = Z_NULL;
		ret = deflateInit(&compressor, -1);
		if (ret != Z_OK)
			throw ret;

	}
	void CompressingDataSocketLayer::writeInt(int toWrite)
	{

		Room(4);
		ByteUtilities::writeInt(inBuf + bufferIndex, toWrite);
		bufferIndex += 4;
	}
	void CompressingDataSocketLayer::writeLong(Long toWrite)
	{
		Room(8);
		ByteUtilities::writeLong(inBuf + bufferIndex, toWrite);
		bufferIndex += 8;
	}
	void CompressingDataSocketLayer::writeDouble(double toWrite)
	{
		Room(8);
		ByteUtilities::writeLong(inBuf + bufferIndex, ByteUtilities::convertDoubleToLong(toWrite));
		bufferIndex += 8;
	}
	void CompressingDataSocketLayer::flush()
	{
		softFlush();

	}

	void CompressingDataSocketLayer::softFlush()
	{
		//gzwrite(Compressor, Buffer, BufferIndex);

		/* run deflate() on input until output buffer not full, finish
		 compression if all of source has been read in */

		do
		{
			compressor.avail_out = BUFFER_SIZE - posInCompBuffer;
			compressor.next_out = outBuf + posInCompBuffer;
			compressor.avail_in = bufferIndex;
			compressor.next_in = (unsigned char*)inBuf;
			int ret = deflate(&compressor, Z_SYNC_FLUSH); /* no bad return value */
			if (ret == Z_STREAM_ERROR)
					cerr<<"zlib stream error."<<endl;	/* state not clobbered */
			int have = BUFFER_SIZE - compressor.avail_out;
			posInCompBuffer += have;

		} while (compressor.avail_out == 0);

		bufferIndex = 0;
	}
	unsigned char* CompressingDataSocketLayer::getOutputBuffer()
	{
		return outBuf;
	}
	int CompressingDataSocketLayer::getOutputLength()
	{
		return posInCompBuffer;
	}
	CompressingDataSocketLayer::~CompressingDataSocketLayer()
	{
		//delete[] (Buffer);
		//delete(Compressor);
	}

} /* namespace TraceviewerServer */
