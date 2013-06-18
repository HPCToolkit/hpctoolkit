/*
 * TraceDataByRank.h
 *
 *  Created on: Jul 9, 2012
 *      Author: pat2
 */

#ifndef TRACEDATABYRANKLOCAL_H_
#define TRACEDATABYRANKLOCAL_H_

#include <vector>
#include "TimeCPID.hpp"
#include "BaseDataFile.hpp"


namespace TraceviewerServer
{

	class TraceDataByRank
	{
	public:

		TraceDataByRank(BaseDataFile*, int, int, int);
		virtual ~TraceDataByRank();

		void getData(Time timeStart, Time timeRange, double pixelLength);
		int sampleTimeLine(Long minLoc, Long maxLoc, int startPixel, int endPixel, int minIndex, double pixelLength, Time startingTime);
		Long findTimeInInterval(Time time, Long l_boundOffset, Long r_boundOffset);

		/**The size of one trace record in bytes (cpid (= 4 bytes) + timeStamp (= 8 bytes)).*/
		static const int SIZE_OF_TRACE_RECORD = 12;
		vector<TimeCPID>* listCPID;
		int rank;
	private:
		BaseDataFile* data;

		Long minloc;
		Long maxloc;
		int numPixelsH;

		Long getAbsoluteLocation(Long);

		Long getRelativeLocation(Long);
		void addSample(unsigned int, TimeCPID);
		TimeCPID getData(Long);
		Long getNumberOfRecords(Long, Long);
		void postProcess();
	};

} /* namespace TraceviewerServer */
#endif /* TRACEDATABYRANKLOCAL_H_ */
