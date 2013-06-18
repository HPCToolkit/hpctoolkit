/*
 * LargeByteBuffer.h
 *
 *  Created on: Jul 9, 2012
 *      Author: pat2
 */

#ifndef LARGEBYTEBUFFER_H_
#define LARGEBYTEBUFFER_H_

#include "ByteUtilities.hpp"
#include <string>
#include "VersatileMemoryPage.hpp"
#include <vector>


namespace TraceviewerServer
{
	typedef uint64_t ULong;
	class LargeByteBuffer
	{
	public:
		LargeByteBuffer(std::string, int);
		virtual ~LargeByteBuffer();
		FileOffset size();
		Long getLong(FileOffset);
		int getInt(FileOffset);
	private:
		static ULong lcm(ULong, ULong);
		static ULong getRamSize();
		vector<VersatileMemoryPage> masterBuffer;
		FileOffset length;
		int numPages;

	};

} /* namespace TraceviewerServer */
#endif /* LARGEBYTEBUFFER_H_ */
