/*
 * Server.h
 *
 *  Created on: Jul 9, 2012
 *      Author: pat2
 */

#ifndef Server_H_
#define Server_H_

#ifndef NO_MPI//MPI version should be default. If it is compiled with -DNO_MPI, then we shouldn't use MPI
	#define USE_MPI
#endif

#include <vector>
#include "DataSocketStream.hpp"
#include "SpaceTimeDataController.hpp"

#define START_NEW_CONNECTION_IMMEDIATELY 1
namespace TraceviewerServer
{
	extern bool useCompression;
	extern int mainPortNumber;
	extern int xmlPortNumber;
	class Server
	{

	public:
		Server();
		virtual ~Server();
		static int main(int argc, char *argv[]);

	private:
		int runConnection(DataSocketStream*);
		void parseInfo(DataSocketStream*);
		void sendDBOpenedSuccessfully(DataSocketStream*);
		void parseOpenDB(DataSocketStream*);
		void getAndSendData(DataSocketStream*);
		vector<char> compressXML();
		void sendDBOpenFailed(DataSocketStream*);

		SpaceTimeDataController* controller;

	};
}/* namespace TraceviewerServer */
#endif /* Server_H_ */
