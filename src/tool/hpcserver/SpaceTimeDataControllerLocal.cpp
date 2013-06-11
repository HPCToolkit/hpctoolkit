/*
 * SpaceTimeDataControllerLocal.cpp
 *
 *  Created on: Jul 9, 2012
 *      Author: pat2
 */

#include "SpaceTimeDataControllerLocal.h"
#include "FileData.h"
#include "Server.h"
#include <iostream>
using namespace std;
namespace TraceviewerServer
{

//ImageTraceAttributes* Attributes;
//ProcessTimeline** Traces;
//int TracesLength;

	SpaceTimeDataControllerLocal::SpaceTimeDataControllerLocal(FileData* locations)
	{
		Attributes = new ImageTraceAttributes();
		OldAttributes = new ImageTraceAttributes();
		DataTrace = new BaseDataFile(locations->fileTrace, DEFAULT_HEADER_SIZE);
		Height = DataTrace->getNumberOfFiles();
		cout << "STDCL at: " << this << " Height= " << Height << endl;
		ExperimentXML = locations->fileXML;
		FileTrace = locations->fileTrace;
		TracesInitialized = false;

	}

//called once the INFO packet has been received to add the information to the stdc
	void SpaceTimeDataControllerLocal::SetInfo(Long minBegTime, Long maxEndTime,
			int headerSize)
	{
		MinBegTime = minBegTime;
		MaxEndTime = maxEndTime;
		HEADER_SIZE = headerSize;

		if (DataTrace != NULL)
		{
			delete (DataTrace);
			DataTrace = new BaseDataFile(FileTrace, HEADER_SIZE);
		}
	}

	int SpaceTimeDataControllerLocal::GetHeight()
	{
		return Height;
	}

	string SpaceTimeDataControllerLocal::GetExperimentXML()
	{
		return ExperimentXML;
	}

	ProcessTimeline* SpaceTimeDataControllerLocal::GetNextTrace(bool ChangedBounds)
	{
		if (Attributes->lineNum
				< min(Attributes->numPixelsV, Attributes->endProcess - Attributes->begProcess))
		{
			Attributes->lineNum++;
			if (ChangedBounds)
			{
				return new ProcessTimeline(Attributes->lineNum - 1, DataTrace,
						LineToPaint(Attributes->lineNum - 1), Attributes->numPixelsH,
						Attributes->endTime - Attributes->begTime,
						MinBegTime + Attributes->begTime, HEADER_SIZE);
			}
			else
			{
				if (TracesLength >= Attributes->lineNum)
				{
					if (Traces[Attributes->lineNum - 1] == NULL)
					{
						cerr << "Was null, auto-fixing" << endl;
						Traces[Attributes->lineNum - 1] = new ProcessTimeline(
								Attributes->lineNum - 1, DataTrace,
								LineToPaint(Attributes->lineNum - 1), Attributes->numPixelsH,
								Attributes->endTime - Attributes->begTime,
								MinBegTime + Attributes->begTime, HEADER_SIZE);
					}
					return Traces[Attributes->lineNum - 1];
				}
				else
					cerr << "STD error: trace paints" << TracesLength << " < line number "
							<< Attributes->lineNum << endl;
			}
		}
		return NULL;
	}

	void SpaceTimeDataControllerLocal::AddNextTrace(ProcessTimeline* NextPtl)
	{
		if (NextPtl == NULL)
			cerr << "Saving a null PTL?" << endl;
		Traces[NextPtl->Line()] = NextPtl;
	}

	void SpaceTimeDataControllerLocal::FillTraces(int LinesToPaint, bool ChangedBounds)
	{
		//Traces might be null. Initialize it by calling prepareViewportPainting
		PrepareViewportPainting(ChangedBounds);

		if (LinesToPaint == -1)
			LinesToPaint = min(Attributes->numPixelsV,
					Attributes->endProcess - Attributes->begProcess); //This only works for the detail view though
		//Threading code was here, but for now, leave the c++ implementation single-threaded
		//Taken straight from TimelineThread
		ProcessTimeline* NextTrace = GetNextTrace(ChangedBounds);
		while (NextTrace != NULL)
		{
			if (ChangedBounds)
			{
				NextTrace->ReadInData();
				AddNextTrace(NextTrace);
			}
			NextTrace = GetNextTrace(ChangedBounds);
		}
	}

	int SpaceTimeDataControllerLocal::LineToPaint(int Line)
	{
		int NumTimelinesToPaint = Attributes->endProcess - Attributes->begProcess;
		if (NumTimelinesToPaint > Attributes->numPixelsV)
			return Attributes->begProcess
					+ (Line * NumTimelinesToPaint) / (Attributes->numPixelsV);
		else
			return Attributes->begProcess + Line;
	}

	const int* SpaceTimeDataControllerLocal::GetValuesXProcessID()
	{
		return DataTrace->ProcessIDs;
	}
	const short* SpaceTimeDataControllerLocal::GetValuesXThreadID()
	{
		return DataTrace->ThreadIDs;
	}

	void SpaceTimeDataControllerLocal::PrepareViewportPainting(bool ChangedBounds)
	{
		if (ChangedBounds)
		{
			int NumTraces = min(Attributes->numPixelsV,
					Attributes->endProcess - Attributes->begProcess);
			cout<< "Num Traces: "<<NumTraces<<endl;
			if (TracesInitialized)
			{
				for (int var = 0; var < TracesLength; var++)
				{
					delete (Traces[var]);
				}
				delete Traces;
			}

			Traces = new ProcessTimeline*[NumTraces];
			TracesLength = NumTraces;
			TracesInitialized = true;
		}
	}

	SpaceTimeDataControllerLocal::SpaceTimeDataControllerLocal()
	{
		cout << "Calling the blank constructor" << endl;
	}

	SpaceTimeDataControllerLocal::~SpaceTimeDataControllerLocal()
	{
#ifndef USE_MPI //The MPI implementation actually doesn't use the Traces array at all! It does call GetNextTrace, but ChangedBounds is always true!
		if (Traces != NULL) {
			cout<<"Freeing Traces @"<< Traces << " and all "<<TracesLength << " elements"<<endl;
			for (int var = 0; var < TracesLength; var++)
			{
				delete (Traces[var]);
			}
			delete Traces;
			Traces = NULL;
		}
		else
			cout<<"Not freeing Traces because it has probably already been freed"<<endl;
#endif

	}

} /* namespace TraceviewerServer */
