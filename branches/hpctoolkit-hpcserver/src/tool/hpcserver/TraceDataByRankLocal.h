/*
 * TraceDataByRankLocal.h
 *
 *  Created on: Jul 9, 2012
 *      Author: pat2
 */

#ifndef TRACEDATABYRANKLOCAL_H_
#define TRACEDATABYRANKLOCAL_H_

#include <vector>
#include "TimeCPID.h"
#include "BaseDataFile.h"


namespace TraceviewerServer
{

	class TraceDataByRankLocal
	{
	public:
		//TraceDataByRankLocal();
		TraceDataByRankLocal(BaseDataFile*, int, int, int);
		virtual ~TraceDataByRankLocal();

		void GetData(double, double, double);
		int SampleTimeLine(Long, Long, int, int, int, double, double);
		Long FindTimeInInterval(double, Long, Long);

		/**The size of one trace record in bytes (cpid (= 4 bytes) + timeStamp (= 8 bytes)).*/
		static const int SIZE_OF_TRACE_RECORD = 12;
		vector<TimeCPID>* ListCPID;
		int Rank;
	private:
		BaseDataFile* Data;

		Long Minloc;
		Long Maxloc;
		int NumPixelsH;

		Long GetAbsoluteLocation(Long);
		Long GetRelativeLocation(Long);
		void AddSample(int, TimeCPID);
		TimeCPID GetData(Long);
		Long GetNumberOfRecords(Long, Long);
		void PostProcess();
	};

} /* namespace TraceviewerServer */
#endif /* TRACEDATABYRANKLOCAL_H_ */
