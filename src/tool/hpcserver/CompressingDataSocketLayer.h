
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
		void WriteInt(int);
		void WriteLong(Long);
		void WriteDouble(double);
		void Flush();
		const unsigned char* GetOutputBuffer();
		int GetOutputLength();

	private:
		void SoftFlush();
		int BufferIndex;
		z_stream Compressor;
		char inBuf[BUFFER_SIZE];
		unsigned char outBuf[BUFFER_SIZE];
		int posInCompBuffer;

	};

} /* namespace TraceviewerServer */
#endif /* COMPRESSINGDATASOCKETLAYER_H_ */
