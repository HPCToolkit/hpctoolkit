/*
 * ProcessTimeline.h
 *
 *  Created on: Jul 10, 2012
 *      Author: pat2
 */

#ifndef PROCESSTIMELINE_H_
#define PROCESSTIMELINE_H_
#include "TraceDataByRank.hpp"
namespace TraceviewerServer
{

	class ProcessTimeline
	{

	public:
		ProcessTimeline();
		ProcessTimeline(ImageTraceAttributes attrib, int _lineNum, FilteredBaseData* _dataTrace,
				Time _startingTime, int _headerSize);
		virtual ~ProcessTimeline();
		int line();
		void readInData();
		TraceDataByRank* data;
	private:
		int lineNumToProcessNum(int line);
		/** This ProcessTimeline's line number. */
		int lineNum;
		/** The initial time in view. */
		Time startingTime;
		/** The range of time in view. */
		Time timeRange;
		/** The amount of time that each pixel on the screen correlates to. */
		double pixelLength;
		ImageTraceAttributes attributes;

	};

} /* namespace TraceviewerServer */
#endif /* PROCESSTIMELINE_H_ */
