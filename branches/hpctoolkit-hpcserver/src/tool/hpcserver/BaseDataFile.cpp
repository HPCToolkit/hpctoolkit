/*
 * BaseDataFile.cpp
 *
 *  Created on: Jul 8, 2012
 *      Author: pat2
 */

#include "BaseDataFile.hpp"
#include <iostream>
#include <sstream>
#include "Constants.hpp"

using namespace std;

namespace TraceviewerServer
{
//-----------------------------------------------------------
// Global variables
//-----------------------------------------------------------




	BaseDataFile::BaseDataFile(string _filename, int _headerSize)
	{

		#if DEBUG > 1
		cout << "Setting Data File: " << _filename << endl;
		#endif
		if (_filename != "")
		{
			//---------------------------------------------
			// test file version
			//---------------------------------------------

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
	 * assign data
	 * @param f: array of files
	 * @throws IOException
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
		// get the procs and threads IDs
		for (int i = 0; i < numFiles; i++)
		{
			 int proc_id = masterBuff->getInt(currentPos);
			currentPos += SIZEOF_INT;
			 int thread_id = masterBuff->getInt(currentPos);
			currentPos += SIZEOF_INT;

			offsets[i].start = masterBuff->getLong(currentPos);
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
				// temporary fix: if the application is neither hybrid nor multiproc nor multithreads,
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
