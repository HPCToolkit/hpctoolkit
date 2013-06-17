/*
 * ProcessTimeline.cpp
 *
 *  Created on: Jul 10, 2012
 *      Author: pat2
 */

#include "ProcessTimeline.hpp"

namespace TraceviewerServer
{

	ProcessTimeline::ProcessTimeline(int _lineNum, BaseDataFile* _dataTrace,
			int _processNumber, int _numPixelH, double _timeRange, double _startingTime,
			int _headerSize)
	{
		lineNum = _lineNum;

		timeRange = _timeRange;
		startingTime = _startingTime;

		pixelLength = timeRange / (double) _numPixelH;

		data = new TraceDataByRank(_dataTrace, _processNumber, _numPixelH, _headerSize);

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
