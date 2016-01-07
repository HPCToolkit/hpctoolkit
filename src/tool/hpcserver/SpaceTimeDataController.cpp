// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/SpaceTimeDataController.cpp $
// $Id: SpaceTimeDataController.cpp 4317 2013-07-25 16:32:22Z felipet1326@gmail.com $
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
//   $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/SpaceTimeDataController.cpp $
//
// Purpose:
//   Manages much of the flow of data through the program
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************
#include "SpaceTimeDataController.hpp"
#include "FileData.hpp"
#include <iostream>
using namespace std;
namespace TraceviewerServer
{

//ImageTraceAttributes* Attributes;
//ProcessTimeline** Traces;
//int TracesLength;

	SpaceTimeDataController::SpaceTimeDataController(FileData* locations)
	{
		attributes = new ImageTraceAttributes();

		//This could potentially be a problem if the header size is not the
		//default because we might not be able to read the number of ranks correctly.
		//For now, it's not an issue, and the data dependencies make changing this
		//complicated.
		dataTrace = new FilteredBaseData(locations->fileTrace, DEFAULT_HEADER_SIZE);
		height = dataTrace->getNumberOfRanks();
		experimentXML = locations->fileXML;
		fileTrace = locations->fileTrace;
		tracesInitialized = false;

	}

//called once the INFO packet has been received to add the information to the controller
	void SpaceTimeDataController::setInfo(Time _minBegTime, Time _maxEndTime,
			int _headerSize)
	{
		minBegTime = _minBegTime;
		maxEndTime = _maxEndTime;
		headerSize = _headerSize;
		delete dataTrace;
		dataTrace = new FilteredBaseData(fileTrace, headerSize);
	}

	int SpaceTimeDataController::getNumRanks()
	{
		return height;
	}

	string SpaceTimeDataController::getExperimentXML()
	{
		return experimentXML;
	}

	ProcessTimeline* SpaceTimeDataController::getNextTrace()
	{
		if (attributes->lineNum
				< min(attributes->numPixelsV, attributes->endProcess - attributes->begProcess))
		{
			ProcessTimeline* toReturn  = new ProcessTimeline(*attributes, attributes->lineNum, dataTrace,
					minBegTime + attributes->begTime, headerSize);
			attributes->lineNum++;
			return toReturn;
		}
		return NULL;
	}

	void SpaceTimeDataController::addNextTrace(ProcessTimeline* NextPtl)
	{
		if (NextPtl == NULL)
			cerr << "Saving a null PTL?" << endl;
		traces[NextPtl->line()] = NextPtl;
	}

	//Don't call if in MPI mode
	void SpaceTimeDataController::fillTraces()
	{
		//Traces might be null. resetTraces will fix that.
		resetTraces();


		//Taken straight from TimelineThread
		ProcessTimeline* nextTrace = getNextTrace();
		while (nextTrace != NULL)
		{
			nextTrace->readInData();
			addNextTrace(nextTrace);

			nextTrace = getNextTrace();
		}
	}

	 int* SpaceTimeDataController::getValuesXProcessID()
	{
		return dataTrace->getProcessIDs();
	}
	 short* SpaceTimeDataController::getValuesXThreadID()
	{
		return dataTrace->getThreadIDs();
	}

	void SpaceTimeDataController::resetTraces()
	{

		int numTraces = min(attributes->numPixelsV,
				attributes->endProcess - attributes->begProcess);

		deleteTraces();

		traces = new ProcessTimeline*[numTraces];
		tracesLength = numTraces;
		tracesInitialized = true;

	}
	void SpaceTimeDataController::applyFilters(FilterSet filters)
	{
		dataTrace->setFilters(filters);
	}
	void SpaceTimeDataController::deleteTraces()
	{
		if (tracesInitialized) {

			for (int var = 0; var < tracesLength; var++)
			{
				delete (traces[var]);
			}
			delete[] traces;
		}
		traces = NULL;
		tracesInitialized = false;
	}
	SpaceTimeDataController::~SpaceTimeDataController()
	{
		delete attributes;
		delete dataTrace;

		//The MPI implementation actually doesn't use the Traces array at all!
		//It does call getNextTrace, but changedBounds is always true so
		//tracesInitialized is always false for MPI
		deleteTraces();

	}

} /* namespace TraceviewerServer */
