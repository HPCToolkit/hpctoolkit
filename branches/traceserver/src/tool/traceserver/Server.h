/*
 * Server.h
 *
 *  Created on: Jul 9, 2012
 *      Author: pat2
 */

#ifndef Server_H_
#define Server_H_

#ifndef NoMPI//MPI version should be default. If it is compiled with -DNoMPI, then we shouldn't use MPI
	#define UseMPI
#endif

#include <vector>
#include "DataSocketStream.h"


namespace TraceviewerServer
{
	extern bool Compression;
	extern int MainPort;
	extern int XMLPort;
	class Server
	{

	public:
		Server();
		virtual ~Server();
		static int main(int argc, char *argv[]);

	private:
		static int RunConnection(DataSocketStream*);
		static void ParseInfo(DataSocketStream*);
		static void SendDBOpenedSuccessfully(DataSocketStream*);
		static void ParseOpenDB(DataSocketStream*);
		static void GetAndSendData(DataSocketStream*);
		static vector<char> CompressXML();
		static void SendDBOpenFailed(DataSocketStream*);

	};
}/* namespace TraceviewerServer */
#endif /* Server_H_ */
