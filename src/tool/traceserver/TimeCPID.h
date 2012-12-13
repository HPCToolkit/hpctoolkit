/*
 * TimeCPID.h
 *
 *  Created on: Jul 10, 2012
 *      Author: pat2
 */

#ifndef TIMECPID_H_
#define TIMECPID_H_

namespace TraceviewerServer
{

	struct TimeCPID
	{
	public:
		double Timestamp;
		int CPID;
		TimeCPID(double timestamp, int cpid)
		{
			Timestamp = timestamp;
			CPID = cpid;
		}

	};

} /* namespace TraceviewerServer */
#endif /* TIMECPID_H_ */
