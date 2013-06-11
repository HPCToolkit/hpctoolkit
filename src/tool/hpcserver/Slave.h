/*
 * Slave.h
 *
 *  Created on: Jul 19, 2012
 *      Author: pat2
 */

#ifndef SLAVE_H_
#define SLAVE_H_

#include "SpaceTimeDataControllerLocal.h"
#include "MPICommunication.h"
namespace TraceviewerServer
{

	class Slave
	{
	public:

		Slave();
		virtual ~Slave();
		void RunLoop();

	private:
		SpaceTimeDataControllerLocal* STDCL;
		int GetData(MPICommunication::CommandMessage*);
		bool STDCLNeedsDeleting;
	};

} /* namespace TraceviewerServer */
#endif /* SLAVE_H_ */
