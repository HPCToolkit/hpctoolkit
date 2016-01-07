// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/Server.hpp $
// $Id: Server.hpp 4286 2013-07-09 19:03:59Z felipet1326@gmail.com $
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
//   $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/Server.hpp $
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef Server_H_
#define Server_H_

#include "DataSocketStream.hpp"
#include "SpaceTimeDataController.hpp"



namespace TraceviewerServer
{
	extern bool useCompression;
	extern int mainPortNumber;
	extern int xmlPortNumber;
	class Server
	{

	public:
		Server();
		virtual ~Server();
		static int main(int argc, char *argv[]);

	private:
		int runConnection(DataSocketStream*, DataSocketStream* xmlSocket);
		void sendDBOpenedSuccessfully(DataSocketStream* socket, DataSocketStream* xmlSocket);

		void parseInfo(DataSocketStream*);
		SpaceTimeDataController* parseOpenDB(DataSocketStream*);
		void filter(DataSocketStream*);
		void getAndSendData(DataSocketStream*);
		void sendXML(DataSocketStream*);
		void sendDBOpenFailed(DataSocketStream*);
		void checkProtocolVersions(DataSocketStream* receiver);

		SpaceTimeDataController* controller;

		//Currently not really used, but pretty necessary for future extensions
		int agreedUponProtocolVersion;
		static const int SERVER_PROTOCOL_MAX_VERSION = 0x00010001;

	};
}/* namespace TraceviewerServer */
#endif /* Server_H_ */
