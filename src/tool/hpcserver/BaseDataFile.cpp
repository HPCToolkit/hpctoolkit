// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/BaseDataFile.cpp $
// $Id: BaseDataFile.cpp 4291 2013-07-09 22:25:53Z felipet1326@gmail.com $
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
//   $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/BaseDataFile.cpp $
//
// Purpose:
//   Another level of abstraction for the data from the file.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#include "BaseDataFile.hpp"
#include "Constants.hpp"
#include "DebugUtils.hpp"

using namespace std;

namespace TraceviewerServer
{
//-----------------------------------------------------------
// Global variables
//-----------------------------------------------------------




	BaseDataFile::BaseDataFile(string _filename, int _headerSize)
	{

		DEBUGCOUT(1) << "Setting Data File: " << _filename << endl;

		if (_filename != "")
		{
			setData(_filename, _headerSize);
		}

	}

	int BaseDataFile::getNumberOfFiles()
	{
		return numFiles;
	}

	OffsetPair* BaseDataFile::getOffsets()
	{
		return offsets;
	}

	LargeByteBuffer* BaseDataFile::getMasterBuffer()
	{
		return masterBuff;
	}

	/***
	 * set the data to the specified file
	 */
	void BaseDataFile::setData(string filename, int headerSize)
	{
		masterBuff = new LargeByteBuffer(filename, headerSize);

		FileOffset currentPos = 0;
		type = masterBuff->getInt(currentPos);
		currentPos += SIZEOF_INT;
		numFiles = masterBuff->getInt(currentPos);
		currentPos += SIZEOF_INT;

		processIDs = new int[numFiles];
		threadIDs = new short[numFiles];
		offsets = new OffsetPair[numFiles];



		offsets[numFiles-1].end	= masterBuff->size()- SIZE_OF_TRACE_RECORD - SIZEOF_END_OF_FILE_MARKER;
		// get the procs and threads IDs and build the table that maps rank to file position
		for (int i = 0; i < numFiles; i++)
		{
			 int proc_id = masterBuff->getInt(currentPos);
			currentPos += SIZEOF_INT;
			 int thread_id = masterBuff->getInt(currentPos);
			currentPos += SIZEOF_INT;

			offsets[i].start = masterBuff->getLong(currentPos);
			//offset.end is the position of the last trace record that is a part of that line
			if (i > 0)
				offsets[i-1].end = offsets[i].start - SIZE_OF_TRACE_RECORD;
			currentPos += SIZEOF_LONG;


			//--------------------------------------------------------------------
			// adding list of x-axis
			//--------------------------------------------------------------------


			if (isHybrid())
			{
				processIDs[i] = proc_id;
				threadIDs[i] = thread_id;
			}
			else if (isMultiProcess())
			{
				processIDs[i] = proc_id;
				threadIDs[i] = -1;
			}
			else
			{
				// If the application is neither hybrid nor multiproc nor multithreads,
				// we just print whatever the order of file name alphabetically
				// this is not the ideal solution, but we cannot trust the value of proc_id and thread_id
				processIDs[i] = i;
				threadIDs[i] = -1;
			}

		}
	}

//Check if the application is a multi-processing program (like MPI)
	bool BaseDataFile::isMultiProcess()
	{
		return (type & MULTI_PROCESSES) != 0;
	}
//Check if the application is a multi-threading program (OpenMP for instance)
	bool BaseDataFile::isMultiThreading()
	{
		return (type & MULTI_THREADING) != 0;
	}
//Check if the application is a hybrid program (MPI+OpenMP)
	bool BaseDataFile::isHybrid()
	{
		return (isMultiProcess() && isMultiThreading());

	}

	BaseDataFile::~BaseDataFile()
	{
		delete(masterBuff);
		delete[] processIDs;
		delete[] threadIDs;
		delete[] offsets;
	}

} /* namespace TraceviewerServer */
