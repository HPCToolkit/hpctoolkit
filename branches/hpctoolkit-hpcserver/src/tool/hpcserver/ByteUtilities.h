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

		static short ReadShort(char* Buffer)
		{
			unsigned char* uBuffer = (unsigned char*) Buffer;
			return (uBuffer[0] << 8)| (uBuffer[1]);
		}

		static int ReadInt(char* Buffer)
		{
			unsigned char* uBuffer = (unsigned char*) Buffer;
			return ((uBuffer[0] << 24) | (uBuffer[1] << 16) | (uBuffer[2] << 8)
					| (uBuffer[3]));
		}
		static Long ReadLong(char* Buffer)
		{
			unsigned int HighWord = ReadInt(Buffer);
			unsigned int LowWord = ReadInt(Buffer + 4);
			ULong Combined = ((ULong) HighWord << 32) | LowWord;
			return Combined;
		}

		static void WriteShort(char* Buffer, short ToWrite)
		{
			unsigned short utoWrite = ToWrite;
			Buffer[0] = (utoWrite & MASK_1) >> 8;
			Buffer[1] = utoWrite & MASK_0;
		}

		static void WriteInt(char* Buffer, int ToWrite)
		{
			unsigned int utoWrite = ToWrite;

			Buffer[0] = (utoWrite & MASK_3) >> 24;
			Buffer[1] = (utoWrite & MASK_2) >> 16;
			Buffer[2] = (utoWrite & MASK_1) >> 8;
			Buffer[3] = utoWrite & MASK_0;
		}
		static void WriteLong(char* Buffer, long ToWrite)
		{
			ULong utoWrite = ToWrite;
			Buffer[0] = (utoWrite & MASK_7) >> 56;
			Buffer[1] = (utoWrite & MASK_6) >> 48;
			Buffer[2] = (utoWrite & MASK_5) >> 40;
			Buffer[3] = (utoWrite & MASK_4) >> 32;
			Buffer[4] = (utoWrite & MASK_3) >> 24;
			Buffer[5] = (utoWrite & MASK_2) >> 16;
			Buffer[6] = (utoWrite & MASK_1) >> 8;
			Buffer[7] = utoWrite & MASK_0;
		}
		static Long ConvertDoubleToLong(double d)
		{
			union { double d; Long l;} dbLgConv;
			dbLgConv.d = d;
			return dbLgConv.l;
		}
		static double ConvertLongToDouble(Long l)
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
