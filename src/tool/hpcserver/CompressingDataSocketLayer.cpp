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
// Copyright ((c)) 2002-2013, Rice University
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
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#include "ByteUtilities.hpp"
#include <iostream>
#include "CompressingDataSocketLayer.hpp"

using namespace std;
namespace TraceviewerServer
{

	CompressingDataSocketLayer::CompressingDataSocketLayer(
		)
	{

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

		makeRoom(4);
		ByteUtilities::writeInt(inBuf + bufferIndex, toWrite);
		bufferIndex += 4;
	}
	void CompressingDataSocketLayer::writeLong(Long toWrite)
	{
		makeRoom(8);
		ByteUtilities::writeLong(inBuf + bufferIndex, toWrite);
		bufferIndex += 8;
	}
	void CompressingDataSocketLayer::writeDouble(double toWrite)
	{
		makeRoom(8);
		ByteUtilities::writeLong(inBuf + bufferIndex, ByteUtilities::convertDoubleToLong(toWrite));
		bufferIndex += 8;
	}
	void CompressingDataSocketLayer::flush()
	{
		softFlush();
	}
	void CompressingDataSocketLayer::makeRoom(int count)
	{
		if (count + bufferIndex > BUFFER_SIZE)
			softFlush();
	}
	void CompressingDataSocketLayer::softFlush()
	{
		//gzwrite(Compressor, Buffer, BufferIndex);

		/* run deflate() on input until output buffer not full, finish
		 compression if all of source has been read in */
		//Taken mostly from http://www.zlib.net/zpipe.c
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
		deflateEnd(&compressor);
		//delete[] (Buffer);
		//delete(Compressor);
	}

} /* namespace TraceviewerServer */
