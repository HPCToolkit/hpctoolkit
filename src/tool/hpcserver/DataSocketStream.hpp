// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/DataSocketStream.hpp $
// $Id: DataSocketStream.hpp 4283 2013-07-02 20:13:13Z felipet1326@gmail.com $
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
//   $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/DataSocketStream.hpp $
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************
/*
 * DataSocketStream.h
 *
 *  Created on: Jul 11, 2012
 *      Author: pat2
 */

#ifndef DATASOCKETSTREAM_H_
#define DATASOCKETSTREAM_H_

#include <string>
#include <cstdio>

#include "ByteUtilities.hpp"

namespace TraceviewerServer
{
	using namespace std;

	typedef int SocketFD;
	class DataSocketStream
	{
	public:
		DataSocketStream(int, bool);
		void acceptSocket();
		DataSocketStream();

		int getPort();

		virtual ~DataSocketStream();

		virtual void writeInt(int);
		virtual void writeLong(Long);
		virtual void writeDouble(double);
		virtual void writeRawData(char*, int);
		virtual void writeString(string);
		virtual void writeShort(short);

		virtual void flush();

		int readInt();
		Long readLong();
		string readString();
		double readDouble();
		short readShort();
		char readByte();

		SocketFD getDescriptor();
	private:
		int port;
		SocketFD socketDesc;
		SocketFD unopenedSocketFD;
		void checkForErrors(int);
		FILE* file;
	};

} /* namespace TraceviewerServer */
#endif /* DATASOCKETSTREAM_H_ */
