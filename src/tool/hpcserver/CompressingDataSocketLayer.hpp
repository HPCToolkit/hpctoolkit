
#include "zlib.h"
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
#define BUFFER_SIZE 0x200000 //2MB
	class CompressingDataSocketLayer
	{
	public:
		CompressingDataSocketLayer();
		virtual ~CompressingDataSocketLayer();
		void writeInt(int);
		void writeLong(Long);
		void writeDouble(double);
		void flush();
		unsigned char* getOutputBuffer();
		int getOutputLength();

	private:
		void softFlush();
		int bufferIndex;
		z_stream compressor;
		char inBuf[BUFFER_SIZE];
		unsigned char outBuf[BUFFER_SIZE];
		int posInCompBuffer;

	};

} /* namespace TraceviewerServer */
#endif /* COMPRESSINGDATASOCKETLAYER_H_ */
