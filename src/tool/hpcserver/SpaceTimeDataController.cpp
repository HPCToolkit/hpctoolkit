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
		dataTrace = new BaseDataFile(locations->fileTrace, DEFAULT_HEADER_SIZE);
		height = dataTrace->getNumberOfFiles();
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

		if (dataTrace != NULL)
		{
			delete (dataTrace);
			dataTrace = new BaseDataFile(fileTrace, headerSize);
		}
	}

	int SpaceTimeDataController::getNumRanks()
	{
		return height;
	}

	string SpaceTimeDataController::getExperimentXML()
	{
		return experimentXML;
	}

	ProcessTimeline* SpaceTimeDataController::getNextTrace(bool ChangedBounds)
	{
		if (attributes->lineNum
				< min(attributes->numPixelsV, attributes->endProcess - attributes->begProcess))
		{
			attributes->lineNum++;
			if (ChangedBounds)
			{
				return new ProcessTimeline(attributes->lineNum - 1, dataTrace,
						lineToPaint(attributes->lineNum - 1), attributes->numPixelsH,
						attributes->endTime - attributes->begTime,
						minBegTime + attributes->begTime, headerSize);
			}
			else
			{
				if (tracesLength >= attributes->lineNum)
				{
					if (traces[attributes->lineNum - 1] == NULL)
					{
						cerr << "Was null, auto-fixing" << endl;
						traces[attributes->lineNum - 1] = new ProcessTimeline(
								attributes->lineNum - 1, dataTrace,
								lineToPaint(attributes->lineNum - 1), attributes->numPixelsH,
								attributes->endTime - attributes->begTime,
								minBegTime + attributes->begTime, headerSize);
					}
					return traces[attributes->lineNum - 1];
				}
				else
					cerr << "STD error: trace paints" << tracesLength << " < line number "
							<< attributes->lineNum << endl;
			}
		}
		return NULL;
	}

	void SpaceTimeDataController::addNextTrace(ProcessTimeline* NextPtl)
	{
		if (NextPtl == NULL)
			cerr << "Saving a null PTL?" << endl;
		traces[NextPtl->line()] = NextPtl;
	}

	void SpaceTimeDataController::fillTraces(int LinesToPaint, bool ChangedBounds)
	{
		//Traces might be null. Initialize it by calling prepareViewportPainting
		prepareViewportPainting(ChangedBounds);

		if (LinesToPaint == -1)
			LinesToPaint = min(attributes->numPixelsV,
					attributes->endProcess - attributes->begProcess); //This only works for the detail view though
		//Threading code was here, but for now, leave the c++ implementation single-threaded
		//Taken straight from TimelineThread
		ProcessTimeline* NextTrace = getNextTrace(ChangedBounds);
		while (NextTrace != NULL)
		{
			if (ChangedBounds)
			{
				NextTrace->readInData();
				addNextTrace(NextTrace);
			}
			NextTrace = getNextTrace(ChangedBounds);
		}
	}

	int SpaceTimeDataController::lineToPaint(int Line)
	{
		int NumTimelinesToPaint = attributes->endProcess - attributes->begProcess;
		if (NumTimelinesToPaint > attributes->numPixelsV)
			return attributes->begProcess
					+ (Line * NumTimelinesToPaint) / (attributes->numPixelsV);
		else
			return attributes->begProcess + Line;
	}

	 int* SpaceTimeDataController::getValuesXProcessID()
	{
		return dataTrace->processIDs;
	}
	 short* SpaceTimeDataController::getValuesXThreadID()
	{
		return dataTrace->threadIDs;
	}

	void SpaceTimeDataController::prepareViewportPainting(bool ChangedBounds)
	{
		if (ChangedBounds)
		{
			int NumTraces = min(attributes->numPixelsV,
					attributes->endProcess - attributes->begProcess);
			cout<< "Num Traces: "<<NumTraces<<endl;
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
	}

	SpaceTimeDataController::SpaceTimeDataController()
	{
		cout << "Calling the blank constructor" << endl;
	}

	SpaceTimeDataController::~SpaceTimeDataController()
	{
#ifndef USE_MPI //The MPI implementation actually doesn't use the Traces array at all! It does call GetNextTrace, but ChangedBounds is always true!
		if (traces != NULL) {
			cout<<"Freeing Traces @"<< traces << " and all "<<tracesLength << " elements"<<endl;
			for (int var = 0; var < tracesLength; var++)
			{
				delete (traces[var]);
			}
			delete traces;
			traces = NULL;
		}
		else
			cout<<"Not freeing Traces because it has probably already been freed"<<endl;
#endif

	}

} /* namespace TraceviewerServer */
