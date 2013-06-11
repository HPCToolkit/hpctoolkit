/*
 * ProcessTimeline.h
 *
 *  Created on: Jul 10, 2012
 *      Author: pat2
 */

#ifndef PROCESSTIMELINE_H_
#define PROCESSTIMELINE_H_
#include "BaseDataFile.h"
#include "TraceDataByRankLocal.h"
namespace TraceviewerServer
{

	class ProcessTimeline
	{
	public:
		ProcessTimeline();
		ProcessTimeline(int, BaseDataFile*, int, int, double, double, int);
		virtual ~ProcessTimeline();
		int Line();
		void ReadInData();
		TraceDataByRankLocal* Data;
	private:
		/** This ProcessTimeline's line number. */
		int LineNum;
		/** The initial time in view. */
		double StartingTime;
		/** The range of time in view. */
		double TimeRange;
		/** The amount of time that each pixel on the screen correlates to. */
		double PixelLength;

	};

} /* namespace TraceviewerServer */
#endif /* PROCESSTIMELINE_H_ */
