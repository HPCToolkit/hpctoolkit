// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2013, Rice University
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
//   $HeadURL$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#include <stdlib.h>
#include <fstream>
#include <vector>
#include <map>

#include "FileUtils.hpp"
#include "mpi.h"
#include "Server.hpp"
#include "Slave.hpp"
#include "MPICommunication.hpp"
#include "zlib.h"
#include "Constants.hpp"
#include "Args.hpp"


using namespace std;
using namespace MPI;

int main(int argc, char *argv[])
{
	Args args(argc, argv);
	TraceviewerServer::useCompression = args.compression;
	TraceviewerServer::xmlPortNumber = args.xmlPort;
	TraceviewerServer::mainPortNumber = args.mainPort;

#ifdef USE_MPI
	MPI::Init(argc, argv);
	int rank, size;
	rank = MPI::COMM_WORLD.Get_rank();
	size = MPI::COMM_WORLD.Get_size();
	if (size <= 1)
	{
		cout << "The MPI version of hpcserver must be run with more than one process. "<<
				"If you are looking for a single threaded version, you must recompile hpcserver. "<<
				"See the hpctoolkit documentation for more information."<<endl;
		return 0;
	}
	if (rank == TraceviewerServer::MPICommunication::SOCKET_SERVER)
	{
		try
		{
			TraceviewerServer::Server();
		}
		catch (int e)
		{
		}
		cout<<"Server done, closing..."<<endl;
		TraceviewerServer::MPICommunication::CommandMessage serverShutdown;
		serverShutdown.command = DONE;
		COMM_WORLD.Bcast(&serverShutdown, sizeof(serverShutdown), MPI_PACKED,
				TraceviewerServer::MPICommunication::SOCKET_SERVER);
	}
	else
	{
		try
		{
			TraceviewerServer::Slave();
		}
		catch (int e)
		{

		}
	}
#else
	try
	{
		TraceviewerServer::Server();
	} catch (int e)
	{
		cout << "Closing with error code " << e << endl;
	}
#endif


#ifdef USE_MPI
	MPI::Finalize();
#endif
	return 0;
}
