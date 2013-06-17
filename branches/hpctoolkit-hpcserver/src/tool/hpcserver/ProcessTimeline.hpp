/*
 * ProcessTimeline.h
 *
 *  Created on: Jul 10, 2012
 *      Author: pat2
 */

#ifndef PROCESSTIMELINE_H_
#define PROCESSTIMELINE_H_
#include "BaseDataFile.hpp"
#include "TraceDataByRank.hpp"
namespace TraceviewerServer
{

	class ProcessTimeline
	{
	public:
		ProcessTimeline();
		ProcessTimeline(int, BaseDataFile*, int, int, double, double, int);
		virtual ~ProcessTimeline();
		int line();
		void readInData();
		TraceDataByRank* data;
	private:
		/** This ProcessTimeline's line number. */
		int lineNum;
		/** The initial time in view. */
		double startingTime;
		/** The range of time in view. */
		double timeRange;
		/** The amount of time that each pixel on the screen correlates to. */
		double pixelLength;

	};

} /* namespace TraceviewerServer */
#endif /* PROCESSTIMELINE_H_ */
