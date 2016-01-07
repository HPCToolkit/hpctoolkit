// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/ByteUtilities.hpp $
// $Id: ByteUtilities.hpp 4291 2013-07-09 22:25:53Z felipet1326@gmail.com $
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
//   $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/ByteUtilities.hpp $
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************



#ifndef BYTEUTILITIES_H_
#define BYTEUTILITIES_H_

#include <stdint.h>

namespace TraceviewerServer
{
	typedef int64_t Long;
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
		static int64_t readLong(char* buffer)
		{
			unsigned int highWord = readInt(buffer);
			unsigned int lowWord = readInt(buffer + 4);
			uint64_t combined = (((uint64_t) highWord) << 32) | lowWord;
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
		static void writeLong(char* buffer, int64_t ToWrite)
		{
			uint64_t utoWrite = ToWrite;
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
		static const uint64_t MASK_4 = 0x000000FF00000000ULL,
				MASK_5 = 0x0000FF0000000000ULL, MASK_6 = 0x00FF000000000000ULL, MASK_7 =
						0xFF00000000000000ULL; //for a long
	};

} /* namespace TraceviewerServer */
#endif /* BYTEUTILITIES_H_ */
