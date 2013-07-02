/*
 * Communication.hpp
 *
 *  Created on: Jun 28, 2013
 *      Author: pat2
 *
 *  Abstracts the MPI/No-MPI differences away.
 */

#ifndef COMMUNICATION_H_
#define COMMUNICATION_H_

#include <string>
#include "SpaceTimeDataController.hpp"
#include "DataSocketStream.hpp"
#include "TimeCPID.hpp" //For Time
#include "ProgressBar.hpp"

namespace TraceviewerServer
{
enum ServerType {
	NONE_EXIT_IMMEDIATELY = 0,
	MASTER = 1,
	SLAVE = 2
};

class Communication {
public:

	static void sendParseInfo(Time minBegTime, Time maxEndTime, int headerSize);
	static void sendParseOpenDB(string pathToDB);
	static void sendStartGetData(SpaceTimeDataController* contr, int processStart, int processEnd,
			Time timeStart, Time timeEnd, int verticalResolution, int horizontalResolution);
	static void sendEndGetData(DataSocketStream* stream, ProgressBar* prog, SpaceTimeDataController* controller);
	static void sendStartFilter(int count, bool excludeMatches);
	static void sendFilter(BinaryRepresentationOfFilter filt);

	static ServerType basicInit(int argc, char** argv);
	static void closeServer();
};
}
#endif /* COMMUNICATION_H_ */
