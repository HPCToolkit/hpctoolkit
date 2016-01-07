// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/CompressingDataSocketLayer.cpp $
// $Id: CompressingDataSocketLayer.cpp 4286 2013-07-09 19:03:59Z felipet1326@gmail.com $
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
//   $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/CompressingDataSocketLayer.cpp $
//
// Purpose:
//   Compresses data to a memory-based buffer.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#include "ByteUtilities.hpp"
#include "DataCompressionLayer.hpp"
#include "DebugUtils.hpp"

#include <iostream> //For cerr
#include <cassert>
#include <algorithm> //For copy
#include <zlib.h>

using namespace std;
namespace TraceviewerServer
{

	DataCompressionLayer::DataCompressionLayer()
	{

		//See: http://www.zlib.net/zpipe.c

		bufferIndex = 0;
		posInCompBuffer = 0;

		inBuf = new char[BUFFER_SIZE];
		outBuf = new unsigned char[BUFFER_SIZE];
		outBufferCurrentSize = BUFFER_SIZE;

		progMonitor = NULL;

		compressor.zalloc = Z_NULL;
		compressor.zfree = Z_NULL;
		compressor.opaque = Z_NULL;
		int ret = deflateInit(&compressor, -1);
		if (ret != Z_OK)
			throw ret;

	}

	DataCompressionLayer::DataCompressionLayer(z_stream customCompressor, ProgressBar* _progMonitor)
	{
		bufferIndex = 0;
		posInCompBuffer = 0;

		inBuf = new char[BUFFER_SIZE];
		outBuf = new unsigned char[BUFFER_SIZE];
		outBufferCurrentSize = BUFFER_SIZE;

		progMonitor = _progMonitor;
		compressor = customCompressor;
	}

	void DataCompressionLayer::writeInt(int toWrite)
	{
		makeRoom(4);
		ByteUtilities::writeInt(inBuf + bufferIndex, toWrite);
		bufferIndex += 4;
		pInc(4);
	}
	void DataCompressionLayer::writeLong(uint64_t toWrite)
	{
		makeRoom(8);
		ByteUtilities::writeLong(inBuf + bufferIndex, toWrite);
		bufferIndex += 8;
		pInc(8);
	}
	void DataCompressionLayer::writeDouble(double toWrite)
	{
		makeRoom(8);
		ByteUtilities::writeLong(inBuf + bufferIndex, ByteUtilities::convertDoubleToLong(toWrite));
		bufferIndex += 8;
		pInc(8);
	}
	void DataCompressionLayer::writeFile(FILE* toWrite)
	{
		while (!feof(toWrite))
		{
			makeRoom(BUFFER_SIZE);
			assert(bufferIndex == 0);
			unsigned int read= fread(inBuf, sizeof(char), BUFFER_SIZE, toWrite);
			bufferIndex += read;
			pInc(read);
		}
		flush();
	}
	void DataCompressionLayer::flush()
	{
		softFlush(Z_FINISH);
	}
	void DataCompressionLayer::makeRoom(int count)
	{
		if (count + bufferIndex > BUFFER_SIZE)
			softFlush(Z_NO_FLUSH);
	}
	void DataCompressionLayer::softFlush(int flushType)
	{

		/* run deflate() on input until output buffer not full, finish
		 compression if all of source has been read in */

		//Adapted from http://www.zlib.net/zpipe.c
		compressor.avail_in = bufferIndex;
		compressor.next_in = (unsigned char*)inBuf;
		do
		{
			compressor.avail_out = outBufferCurrentSize - posInCompBuffer;
			assert(outBufferCurrentSize > posInCompBuffer);
			DEBUGCOUT(2) << "Avail out: " << outBufferCurrentSize - posInCompBuffer<<endl;
			compressor.next_out = outBuf + posInCompBuffer;
			int ret = deflate(&compressor, flushType); /* no bad return value */
			if (ret == Z_STREAM_ERROR)
				cerr<<"zlib stream error."<<endl;	/* state not clobbered */

			posInCompBuffer  = outBufferCurrentSize - compressor.avail_out;
			if (posInCompBuffer == outBufferCurrentSize)
				//Shoot, our output buffer is full...
				growOutputBuffer();

		} while (compressor.avail_out == 0);
		assert(compressor.avail_in == 0);

		bufferIndex = 0;
	}

	void DataCompressionLayer::growOutputBuffer()
	{
		unsigned char* newBuffer = new unsigned char[outBufferCurrentSize * BUFFER_GROW_FACTOR];
		copy(outBuf, outBuf + outBufferCurrentSize, newBuffer);

		delete[] outBuf;

		outBufferCurrentSize*= BUFFER_GROW_FACTOR;
		outBuf = newBuffer;
	}

	void DataCompressionLayer::pInc(unsigned int count)
	{
		if (progMonitor != NULL)
			progMonitor->incrementProgress(count);
	}

	unsigned char* DataCompressionLayer::getOutputBuffer()
	{
		return outBuf;
	}
	int DataCompressionLayer::getOutputLength()
	{
		return posInCompBuffer;
	}
	DataCompressionLayer::~DataCompressionLayer()
	{
		deflateEnd(&compressor);
		delete[] inBuf;
		delete[] outBuf;
	}

} /* namespace TraceviewerServer */
