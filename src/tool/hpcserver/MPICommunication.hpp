// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/MPICommunication.hpp $
// $Id: MPICommunication.hpp 4335 2013-08-01 18:24:05Z felipet1326@gmail.com $
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
//   $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/MPICommunication.hpp $
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************
#ifndef MPICOMMUNICATION_H_
#define MPICOMMUNICATION_H_


#include "TimeCPID.hpp"

#include <mpi.h>
#include <stdint.h>

namespace TraceviewerServer
{
//Forward declarations so we don't need to include the class just to have a pointer to it
	class DataCompressionLayer;


	class MPICommunication
	{
	public:
		static const int SOCKET_SERVER = 0; //The rank of the node that is doing all the socket comm

		typedef struct
		{
			char path[1024];
		} open_file_command;
		typedef struct
		{
			uint32_t processStart;
			uint32_t processEnd;
			Time timeStart;
			Time timeEnd;
			uint32_t verticalResolution;
			uint32_t horizontalResolution;
		} get_data_command;
		typedef struct
		{
			Time minBegTime;
			Time maxEndTime;
			int headerSize;
		} more_info_command;

		typedef struct
		{
			int count;
			bool excludeMatches;
		} filter_header_command;
		typedef struct
		{
			int command;
			union
			{

				open_file_command ofile;
				get_data_command gdata;
				more_info_command minfo;
				filter_header_command filt;
			};
		} CommandMessage;


		typedef struct
		{
			int rankID;
			int traceLinesSent;
		} DoneMessage;

		typedef struct
		{
			int rankID;

			int line;
			int entries;
			Time begtime;
			Time endtime;
			int compressedSize;//In Bytes
		} DataHeader;

		typedef struct
		{
			int tag;
			union
			{
				DataHeader data;
				DoneMessage done;
			};
		} ResultMessage;

		typedef struct
		{
			ResultMessage* header;
			bool compressed;
			union{
				DataCompressionLayer* compMsg;
				unsigned char* message;
			};
			MPI::Request headerRequest;
			MPI::Request bodyRequest;
		} ResultBufferLocations;
	};

} /* namespace TraceviewerServer */
#endif /* MPICOMMUNICATION_H_ */
