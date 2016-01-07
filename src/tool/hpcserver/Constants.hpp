// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/Constants.hpp $
// $Id: Constants.hpp 4291 2013-07-09 22:25:53Z felipet1326@gmail.com $
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
//   $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/Constants.hpp $
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************


#ifndef CONSTANTS_H_
#define CONSTANTS_H_
namespace TraceviewerServer
{

#define SIZEOF_LONG 8
#define SIZEOF_INT 4
#define SIZEOF_SHORT 2
#define SIZEOF_BYTE 1

#define SIZEOF_DELTASAMPLE (2*SIZEOF_INT) //The CPID is an int, and the delta timestamp is also an int

/**The size of one trace record in bytes (cpid (= 4 bytes) + timeStamp (= 8 bytes)).*/
#define SIZE_OF_TRACE_RECORD (SIZEOF_INT+SIZEOF_LONG)
#define SIZEOF_END_OF_FILE_MARKER 4

	static const int DEFAULT_PORT = 21590;
	static const unsigned int MAX_DB_PATH_LENGTH = 1023;

enum DatabaseType {
	MULTI_PROCESSES = 1,
	MULTI_THREADING = 2
};

enum Header {
	DATA = 0x44415441,
	OPEN = 0x4F50454E,
	HERE = 0x48455245,
	DONE = 0x444F4E45,
	DBOK = 0x44424F4B,
	INFO = 0x494E464F,
	NODB = 0x4E4F4442,
	EXML = 0x45584D4C,
	FLTR = 0x464C5452,
	SLAVE_REPLY = 0x534C5250,
	SLAVE_DONE = 0x534C444E
};

enum ServerNextAction {
	CLOSE_SERVER = 0,
	START_NEW_CONNECTION_IMMEDIATELY=1
};

	//Yes, these are almost entirely arbitrary
enum ErrorCode {
	ERROR_STREAM_OPEN_FAILED = -3,
	ERROR_EXPECTED_OPEN_COMMAND = -77,
	ERROR_DB_OPEN_FAILED = -4,
	ERROR_UNKNOWN_COMMAND = -7,
	ERROR_PATH_TOO_LONG = -9,
	ERROR_INVALID_PARAMETERS = -99,
	ERROR_COMPRESSION_FAILED = -33445,
	ERROR_GET_RAM_SIZE_FAILED = -4456,
	ERROR_READ_TOO_LITTLE = -5200,
	ERROR_STREAM_CLOSED = -12,
	ERROR_SOCKET_IN_USE = -1111
};

}
#endif /* CONSTANTS_H_ */
