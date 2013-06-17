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
		cout << "Setting Data File: " << _filename << endl;
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

	Long* BaseDataFile::getOffsets()
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
	void BaseDataFile::setData(string filename, int HeaderSize)
	{
		masterBuff = new LargeByteBuffer(filename, HeaderSize);

		type = masterBuff->getInt(0);
		numFiles = masterBuff->getInt(SIZEOF_INT);

		processIDs = new int[numFiles];
		threadIDs = new short[numFiles];
		offsets = new Long[numFiles];

		Long currentPos = SIZEOF_INT * 2;

		// get the procs and threads IDs
		for (int i = 0; i < numFiles; i++)
		{
			 int proc_id = masterBuff->getInt(currentPos);
			currentPos += SIZEOF_INT;
			 int thread_id = masterBuff->getInt(currentPos);
			currentPos += SIZEOF_INT;

			offsets[i] = masterBuff->getLong(currentPos);
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
