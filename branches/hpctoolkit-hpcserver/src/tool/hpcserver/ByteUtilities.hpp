/*
 * ByteUtilities.h
 *
 *  Created on: Jul 17, 2012
 *      Author: pat2
 */



#ifndef BYTEUTILITIES_H_
#define BYTEUTILITIES_H_

#include <stdint.h>

namespace TraceviewerServer
{
	typedef int64_t Long;
	typedef uint64_t ULong;
	class ByteUtilities
	{
	public:

		static short readShort(char* buffer)
		{
			unsigned char* uBuffer = (unsigned char*) buffer;
			return (uBuffer[0] << 8)| (uBuffer[1]);
		}

		static int readInt(char* buffer)
		{
			unsigned char* uBuffer = (unsigned char*) buffer;
			return ((uBuffer[0] << 24) | (uBuffer[1] << 16) | (uBuffer[2] << 8)
					| (uBuffer[3]));
		}
		static Long readLong(char* buffer)
		{
			unsigned int highWord = readInt(buffer);
			unsigned int lowWord = readInt(buffer + 4);
			ULong combined = ((ULong) highWord << 32) | lowWord;
			return combined;
		}

		static void writeShort(char* buffer, short ToWrite)
		{
			unsigned short utoWrite = ToWrite;
			buffer[0] = (utoWrite & MASK_1) >> 8;
			buffer[1] = utoWrite & MASK_0;
		}

		static void writeInt(char* buffer, int ToWrite)
		{
			unsigned int utoWrite = ToWrite;

			buffer[0] = (utoWrite & MASK_3) >> 24;
			buffer[1] = (utoWrite & MASK_2) >> 16;
			buffer[2] = (utoWrite & MASK_1) >> 8;
			buffer[3] = utoWrite & MASK_0;
		}
		static void writeLong(char* buffer, long ToWrite)
		{
			ULong utoWrite = ToWrite;
			buffer[0] = (utoWrite & MASK_7) >> 56;
			buffer[1] = (utoWrite & MASK_6) >> 48;
			buffer[2] = (utoWrite & MASK_5) >> 40;
			buffer[3] = (utoWrite & MASK_4) >> 32;
			buffer[4] = (utoWrite & MASK_3) >> 24;
			buffer[5] = (utoWrite & MASK_2) >> 16;
			buffer[6] = (utoWrite & MASK_1) >> 8;
			buffer[7] = utoWrite & MASK_0;
		}
		static Long convertDoubleToLong(double d)
		{
			union { double d; Long l;} dbLgConv;
			dbLgConv.d = d;
			return dbLgConv.l;
		}
		static double convertLongToDouble(Long l)
		{
			union { double d; Long l;} dbLgConv;
			dbLgConv.l = l;
			return dbLgConv.d;
		}
	private:
		static const unsigned int MASK_0 = 0x000000FF, MASK_1 = 0x0000FF00, MASK_2 =
				0x00FF0000, MASK_3 = 0xFF000000; //For an int
		static const ULong MASK_4 = 0x000000FF00000000ULL,
				MASK_5 = 0x0000FF0000000000ULL, MASK_6 = 0x00FF000000000000ULL, MASK_7 =
						0xFF00000000000000ULL; //for a long
	};

} /* namespace TraceviewerServer */
#endif /* BYTEUTILITIES_H_ */
