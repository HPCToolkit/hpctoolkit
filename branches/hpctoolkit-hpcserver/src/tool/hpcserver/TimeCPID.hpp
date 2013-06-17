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
		double timestamp;
		int cpid;
		TimeCPID(double _timestamp, int _cpid)
		{
			timestamp = _timestamp;
			cpid = _cpid;
		}

	};

} /* namespace TraceviewerServer */
#endif /* TIMECPID_H_ */
