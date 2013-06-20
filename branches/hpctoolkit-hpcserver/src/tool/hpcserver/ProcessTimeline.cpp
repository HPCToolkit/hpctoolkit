/*
 * ProcessTimeline.cpp
 *
 *  Created on: Jul 10, 2012
 *      Author: pat2
 */

#include "ProcessTimeline.hpp"

namespace TraceviewerServer
{

	ProcessTimeline::ProcessTimeline(ImageTraceAttributes attrib, int _lineNum, FilteredBaseData* _dataTrace,
			Time _startingTime, int _headerSize)
	{
		lineNum = _lineNum;

		timeRange = (attrib.endTime - attrib.begTime) ;
		startingTime = _startingTime;

		pixelLength = timeRange / (double) attrib.numPixelsH;

		attributes = attrib;
		data = new TraceDataByRank(_dataTrace, lineNumToProcessNum(_lineNum), attrib.numPixelsH, _headerSize);
	}
	int ProcessTimeline::lineNumToProcessNum(int line) {
		int numTimelinesToPaint = attributes.endProcess - attributes.begProcess;
		if (numTimelinesToPaint > attributes.numPixelsV)
			return attributes.begProcess
					+ (line * numTimelinesToPaint) / (attributes.numPixelsV);
		else
			return attributes.begProcess + line;
	}
	void ProcessTimeline::readInData()
	{
		data->getData(startingTime, timeRange, pixelLength);
	}

	int ProcessTimeline::line()
	{
		return lineNum;
	}

	ProcessTimeline::~ProcessTimeline()
	{
		delete data;
	}

} /* namespace TraceviewerServer */
