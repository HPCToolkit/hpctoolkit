// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/main.cpp $
// $Id: main.cpp 4291 2013-07-09 22:25:53Z felipet1326@gmail.com $
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
//   $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/main.cpp $
//
// Purpose:
//   The entry point for the server regardless of how it is being run.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#include "Server.hpp"
#include "Communication.hpp"
#include "Constants.hpp"
#include "Args.hpp"
#include "DebugUtils.hpp"

using namespace std;

int main(int argc, char *argv[])
{
	if (!TraceviewerServer::Communication::basicInit(argc, argv))
		return 0;

	Args args(argc, argv);
	TraceviewerServer::useCompression = args.compression;
	TraceviewerServer::xmlPortNumber = args.xmlPort;
	TraceviewerServer::mainPortNumber = args.mainPort;

	try
	{
		TraceviewerServer::Communication::run();
	}
	catch (TraceviewerServer::ErrorCode& e)
	{//We had some sort of error. If it hasn't been handled by this point, we just close.
		DEBUGCOUT(1) << "Error on closing was " << hex << e << endl;
	}


	TraceviewerServer::Communication::closeServer();

	return 0;
}
