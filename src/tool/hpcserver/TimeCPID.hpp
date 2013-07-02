/*
 * TimeCPID.h
 *
 *  Created on: Jul 10, 2012
 *      Author: pat2
 */

#ifndef TIMECPID_H_
#define TIMECPID_H_

#include <stdint.h>

namespace TraceviewerServer
{
	typedef uint64_t Time;
	struct TimeCPID
	{
	public:

		Time timestamp;
		int cpid;
		TimeCPID(Time _timestamp, int _cpid)
		{
			timestamp = _timestamp;
			cpid = _cpid;
		}

	};

} /* namespace TraceviewerServer */
#endif /* TIMECPID_H_ */
