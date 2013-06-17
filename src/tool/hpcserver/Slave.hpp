/*
 * Slave.h
 *
 *  Created on: Jul 19, 2012
 *      Author: pat2
 */

#ifndef SLAVE_H_
#define SLAVE_H_

#include "SpaceTimeDataController.hpp"
#include "MPICommunication.hpp"
namespace TraceviewerServer
{

	class Slave
	{
	public:

		Slave();
		virtual ~Slave();
		void run();

	private:
		SpaceTimeDataController* controller;
		int getData(MPICommunication::CommandMessage*);
		bool controllerNeedsDeleting;
	};

} /* namespace TraceviewerServer */
#endif /* SLAVE_H_ */
