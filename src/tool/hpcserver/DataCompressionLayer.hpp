// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/DataCompressionLayer.hpp $
// $Id: DataCompressionLayer.hpp 4286 2013-07-09 19:03:59Z felipet1326@gmail.com $
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
//   $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/DataCompressionLayer.hpp $
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************
#include "zlib.h"
#include <stdint.h>
#include <cstdio>

#include "ProgressBar.hpp"
/*
 * CompressingDataSocketLayer.h
 *
 *  Created on: Jul 25, 2012
 *      Author: pat2
 */

#ifndef COMPRESSINGDATASOCKETLAYER_H_
#define COMPRESSINGDATASOCKETLAYER_H_

namespace TraceviewerServer
{
#define BUFFER_SIZE 0x4000
#define BUFFER_GROW_FACTOR 2 //Double buffer size each time it fills up
	class DataCompressionLayer
	{
	public:
		DataCompressionLayer();
		//Advanced constructor:
		DataCompressionLayer(z_stream customCompressor, ProgressBar* progMonitor);

		virtual ~DataCompressionLayer();
		void writeInt(int);
		void writeLong(uint64_t);
		void writeDouble(double);
		void writeFile(FILE*);
		void flush();
		unsigned char* getOutputBuffer();
		int getOutputLength();

	private:
		//Checks to make sure there is enough room in the buffer for count
		//bytes. If there is not, it makes room by flushing the buffer.
		void makeRoom(int count);
		void softFlush(int flushType);

		//Increment the progress bar if it isn't NULL
		void pInc(unsigned int count);

		void growOutputBuffer();

		unsigned int bufferIndex;
		z_stream compressor;
		char* inBuf;
		unsigned char* outBuf;
		unsigned int posInCompBuffer;

		unsigned int outBufferCurrentSize;

		ProgressBar* progMonitor;
	};

} /* namespace TraceviewerServer */
#endif /* COMPRESSINGDATASOCKETLAYER_H_ */
