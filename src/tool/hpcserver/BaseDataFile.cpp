/*
 * BaseDataFile.cpp
 *
 *  Created on: Jul 8, 2012
 *      Author: pat2
 */

#include "BaseDataFile.h"
#include <iostream>
#include <sstream>
#include "Constants.h"

using namespace std;

namespace TraceviewerServer
{
//-----------------------------------------------------------
// Global variables
//-----------------------------------------------------------




	BaseDataFile::BaseDataFile(string filename, int HeaderSize)
	{
		cout << "Setting Data File: " << filename << endl;
		if (filename != "")
		{
			//---------------------------------------------
			// test file version
			//---------------------------------------------

			setData(filename, HeaderSize);
		}

	}

	int BaseDataFile::getNumberOfFiles()
	{
		return NumFiles;
	}

	Long* BaseDataFile::getOffsets()
	{
		return Offsets;
	}

	LargeByteBuffer* BaseDataFile::getMasterBuffer()
	{
		return MasterBuff;
	}

	/***
	 * assign data
	 * @param f: array of files
	 * @throws IOException
	 */
	void BaseDataFile::setData(string filename, int HeaderSize)
	{
		MasterBuff = new LargeByteBuffer(filename, HeaderSize);

		Type = MasterBuff->GetInt(0);
		NumFiles = MasterBuff->GetInt(SIZEOF_INT);

		ProcessIDs = new int[NumFiles];
		ThreadIDs = new short[NumFiles];
		Offsets = new Long[NumFiles];

		Long current_pos = SIZEOF_INT * 2;

		// get the procs and threads IDs
		for (int i = 0; i < NumFiles; i++)
		{
			 int proc_id = MasterBuff->GetInt(current_pos);
			current_pos += SIZEOF_INT;
			 int thread_id = MasterBuff->GetInt(current_pos);
			current_pos += SIZEOF_INT;

			Offsets[i] = MasterBuff->GetLong(current_pos);
			current_pos += SIZEOF_LONG;

			//--------------------------------------------------------------------
			// adding list of x-axis
			//--------------------------------------------------------------------


			if (IsHybrid())
			{
				ProcessIDs[i] = proc_id;
				ThreadIDs[i] = thread_id;
			}
			else if (IsMultiProcess())
			{
				ProcessIDs[i] = proc_id;
				ThreadIDs[i] = -1;
			}
			else
			{
				// temporary fix: if the application is neither hybrid nor multiproc nor multithreads,
				// we just print whatever the order of file name alphabetically
				// this is not the ideal solution, but we cannot trust the value of proc_id and thread_id
				ProcessIDs[i] = i;
				ThreadIDs[i] = -1;
			}

		}
	}

//Check if the application is a multi-processing program (like MPI)
	bool BaseDataFile::IsMultiProcess()
	{
		return (Type & MULTI_PROCESSES) != 0;
	}
//Check if the application is a multi-threading program (OpenMP for instance)
	bool BaseDataFile::IsMultiThreading()
	{
		return (Type & MULTI_THREADING) != 0;
	}
//Check if the application is a hybrid program (MPI+OpenMP)
	bool BaseDataFile::IsHybrid()
	{
		return (IsMultiProcess() && IsMultiThreading());

	}

	BaseDataFile::~BaseDataFile()
	{
		delete(MasterBuff);
		delete[] ProcessIDs;
		delete[] ThreadIDs;
		delete[] Offsets;
	}

} /* namespace TraceviewerServer */
