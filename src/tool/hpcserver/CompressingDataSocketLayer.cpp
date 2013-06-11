/*
 * CompressingDataSocketLayer.cpp
 *
 *  Created on: Jul 25, 2012
 *      Author: pat2
 */


#include "ByteUtilities.h"
#include <iostream>
#include "CompressingDataSocketLayer.h"

using namespace std;
namespace TraceviewerServer
{

#define Room(a) if ((a) + BufferIndex > BUFFER_SIZE) SoftFlush();

	CompressingDataSocketLayer::CompressingDataSocketLayer(
		)
	{
		//Compressor = gzdopen(BackingSocket->GetDescriptor(), "w");
		//Compressor = gzopen("/Users/pat2/Desktop/test8.gz", "wb");

		//See: http://www.zlib.net/zpipe.c
		int ret;

		BufferIndex = 0;
		posInCompBuffer = 0;

		Compressor.zalloc = Z_NULL;
		Compressor.zfree = Z_NULL;
		Compressor.opaque = Z_NULL;
		ret = deflateInit(&Compressor, -1);
		if (ret != Z_OK)
			throw ret;

	}
	void CompressingDataSocketLayer::WriteInt(int toWrite)
	{

		Room(4);
		ByteUtilities::WriteInt(inBuf + BufferIndex, toWrite);
		BufferIndex += 4;
	}
	void CompressingDataSocketLayer::WriteLong(Long toWrite)
	{
		Room(8);
		ByteUtilities::WriteLong(inBuf + BufferIndex, toWrite);
		BufferIndex += 8;
	}
	void CompressingDataSocketLayer::WriteDouble(double toWrite)
	{
		Room(8);
		Long* plong = (Long*) (&toWrite);
		ByteUtilities::WriteLong(inBuf + BufferIndex, *plong);
		BufferIndex += 8;
	}
	void CompressingDataSocketLayer::Flush()
	{
		SoftFlush();

	}

	void CompressingDataSocketLayer::SoftFlush()
	{
		//gzwrite(Compressor, Buffer, BufferIndex);

		/* run deflate() on input until output buffer not full, finish
		 compression if all of source has been read in */

		do
		{
			Compressor.avail_out = BUFFER_SIZE - posInCompBuffer;
			Compressor.next_out = outBuf + posInCompBuffer;
			Compressor.avail_in = BufferIndex;
			Compressor.next_in = (unsigned char*)inBuf;
			int ret = deflate(&Compressor, Z_SYNC_FLUSH); /* no bad return value */
			if (ret == Z_STREAM_ERROR)
					cerr<<"zlib stream error."<<endl;	/* state not clobbered */
			int have = BUFFER_SIZE - Compressor.avail_out;
			posInCompBuffer += have;

		} while (Compressor.avail_out == 0);

		BufferIndex = 0;
	}
	const unsigned char* CompressingDataSocketLayer::GetOutputBuffer()
	{
		return outBuf;
	}
	int CompressingDataSocketLayer::GetOutputLength()
	{
		return posInCompBuffer;
	}
	CompressingDataSocketLayer::~CompressingDataSocketLayer()
	{
		//delete[] (Buffer);
		//delete(Compressor);
	}

} /* namespace TraceviewerServer */
