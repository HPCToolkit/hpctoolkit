/*
 * ProcessTimeline.cpp
 *
 *  Created on: Jul 10, 2012
 *      Author: pat2
 */

#include "ProcessTimeline.h"

namespace TraceviewerServer
{

	ProcessTimeline::ProcessTimeline(int lineNum, BaseDataFile* dataTrace,
			int processNumber, int numPixelH, double timeRange, double startingTime,
			int header_size)
	{
		LineNum = lineNum;

		TimeRange = timeRange;
		StartingTime = startingTime;

		PixelLength = TimeRange / (double) numPixelH;

		Data = new TraceDataByRankLocal(dataTrace, processNumber, numPixelH, header_size);

	}
	void ProcessTimeline::ReadInData()
	{
		Data->GetData(StartingTime, TimeRange, PixelLength);
	}

	int ProcessTimeline::Line()
	{
		return LineNum;
	}

	ProcessTimeline::~ProcessTimeline()
	{
		delete Data;
	}

} /* namespace TraceviewerServer */
