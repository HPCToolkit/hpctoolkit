/*
 * SpaceTimeDataController.cpp
 *
 *  Created on: Jul 9, 2012
 *      Author: pat2
 */

#include "SpaceTimeDataController.hpp"
#include "FileData.hpp"
#include "Server.hpp"
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
		oldAttributes = new ImageTraceAttributes();
		dataTrace = new FilteredBaseData(locations->fileTrace, DEFAULT_HEADER_SIZE);
		height = dataTrace->getNumberOfRanks();
		experimentXML = locations->fileXML;
		fileTrace = locations->fileTrace;
		tracesInitialized = false;

	}

//called once the INFO packet has been received to add the information to the controller
	void SpaceTimeDataController::setInfo(Long _minBegTime, Long _maxEndTime,
			int _headerSize)
	{
		minBegTime = _minBegTime;
		maxEndTime = _maxEndTime;
		headerSize = _headerSize;

		//if (dataTrace != NULL)
		//{
			delete (dataTrace);
			dataTrace = new FilteredBaseData(fileTrace, headerSize);
		//}
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
		ProcessTimeline* NextTrace = getNextTrace();
		while (NextTrace != NULL)
		{
			NextTrace->readInData();
			addNextTrace(NextTrace);

			NextTrace = getNextTrace();
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

		int NumTraces = min(attributes->numPixelsV,
				attributes->endProcess - attributes->begProcess);

		if (tracesInitialized)
		{
			for (int var = 0; var < tracesLength; var++)
			{
				delete (traces[var]);
			}
			delete traces;
		}

		traces = new ProcessTimeline*[NumTraces];
		tracesLength = NumTraces;
		tracesInitialized = true;

	}
	void SpaceTimeDataController::applyFilters(FilterSet filters)
	{
		dataTrace->setFilters(filters);
	}

	SpaceTimeDataController::~SpaceTimeDataController()
	{
#ifndef USE_MPI //The MPI implementation actually doesn't use the Traces array at all! It does call GetNextTrace, but ChangedBounds is always true!
		if (tracesInitialized) {

			for (int var = 0; var < tracesLength; var++)
			{
				delete (traces[var]);
			}
			delete traces;
			traces = NULL;
			tracesInitialized = false;
		}
#endif

	}

} /* namespace TraceviewerServer */
