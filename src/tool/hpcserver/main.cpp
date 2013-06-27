#include <stdlib.h>
#include <fstream>
#include <vector>
#include <map>

#include "FileUtils.hpp"
#include "mpi.h"
#include "Server.hpp"
#include "Slave.hpp"
#include "MPICommunication.hpp"
#include "zlib.h"
#include "Constants.hpp"
#include "Args.hpp"


using namespace std;
using namespace MPI;

int main(int argc, char *argv[])
{
	Args args(argc, argv);
	TraceviewerServer::useCompression = args.compression;
	TraceviewerServer::xmlPortNumber = args.xmlPort;
	TraceviewerServer::mainPortNumber = args.mainPort;
	//bool ActuallyRun = ParseCommandLineArgs(argc, argv);
	cout << "Compr:" << TraceviewerServer::useCompression<<endl;
	cout << "xml: " << TraceviewerServer::xmlPortNumber<<endl;
	cout << "main: "<< TraceviewerServer::mainPortNumber<<endl;

#ifdef USE_MPI
	MPI::Init(argc, argv);
	int rank, size;
	rank = MPI::COMM_WORLD.Get_rank();
	size = MPI::COMM_WORLD.Get_size();

	if (rank == TraceviewerServer::MPICommunication::SOCKET_SERVER)
	{
		try
		{
			TraceviewerServer::Server();
		}
		catch (int e)
		{
		}
		cout<<"Server done, closing..."<<endl;
		TraceviewerServer::MPICommunication::CommandMessage serverShutdown;
		serverShutdown.command = DONE;
		COMM_WORLD.Bcast(&serverShutdown, sizeof(serverShutdown), MPI_PACKED,
				TraceviewerServer::MPICommunication::SOCKET_SERVER);
	}
	else
	{
		try
		{
			TraceviewerServer::Slave();
		}
		catch (int e)
		{

		}
	}
#else
	try
	{
		TraceviewerServer::Server();
	} catch (int e)
	{
		cout << "Closing with error code " << e << endl;
	}
#endif


#ifdef USE_MPI
	MPI::Finalize();
#endif
	return 0;
}
/*void PrintHelp()
{
	cout << "This is a server for the HPCTraceviewer. Usage: INSERT THE USAGE HERE"<<endl
			<<"Some notes: Allowed flags: --help, --compression, --port, --xmlport"<<endl
			<<"For compression, allowed options are on/off"<<endl
			<<"For main data port, allowed values are 0, which means auto-choose, and any integer 1024< n <65535. Default is 21590"<<endl
			<<"For the xml port, any integer 1024< n <65535. -1 forces usage of the data port to send xml. This should be used with --port 0. Do not include this flag to have the XML port automatically negotiated."
			<< endl;
}
typedef enum
{
	null, help, compression, com_port, xml_port, yes, no
} CLoption;

bool ParseCommandLineArgs(int argc, char *argv[])
{

	typedef map<string, CLoption> Map;
	const Map::value_type vals[] =
	{
	make_pair( "", null),
	make_pair( ".hpp", help ),
	make_pair( "-help", help ),
	make_pair( "--help", help ),
	make_pair( "--Help", help ),
	make_pair( "-c", compression ),
	make_pair( "--compression", compression ),
	make_pair( "--Compression", compression ),
	make_pair( "--comp", compression ),
	make_pair( "--Comp", compression ),
	make_pair( "-p", com_port ),
	make_pair( "--port", com_port ),
	make_pair( "--Port", com_port ),
	make_pair( "--xmlport", xml_port ),//This should be undocumented, because the server should auto-negotiate it
	make_pair( "true", yes ),
	make_pair( "t", yes ),
	make_pair( "True", yes ),
	make_pair( "yes", yes ),
	make_pair( "Yes", yes ),
	make_pair( "y", yes ),
	make_pair( "on", yes),
	make_pair( "false", no ),
	make_pair( "f", no ),
	make_pair( "False", no ),
	make_pair( "no", no ),
	make_pair( "No", no ),
	make_pair( "n", no ),
	make_pair( "off", no)
	};
	const Map Parser (vals, vals + sizeof(vals) / sizeof (vals[0]));


	for (int c = 1; c < argc; c += 2)
	{
		CLoption op = Parser.find(argv[c])->second;
		switch (op)
		{
			case compression:
				if (c + 1 < argc)
				{ //Possibly passed a true/false
					CLoption OnOrOff = Parser.find(argv[c+1])->second;
					if (OnOrOff == yes)
					{
						TraceviewerServer::useCompression = true;
						break;
					}
					else if (OnOrOff == no)
					{
						TraceviewerServer::useCompression = false;
						break;
					}
					else
					{
						PrintHelp();
						return false;
					}
				}
				else
				{
					PrintHelp();
					return false;
				}
			case com_port:
				if (c + 1 < argc)
				{ //Possibly a number following
					int val = atoi(argv[c + 1]);
					if (val == 0 && !TraceviewerServer::FileUtils::stringActuallyZero(argv[c+1]))
					{
						cout<<"Could not parse port number"<<endl;
						PrintHelp();
						return false;
					}
					if ((val < 1024)&& (val != 0))
					{
						cout << "Port cannot be less than 1024"<<endl;
						PrintHelp();
						return false;
					}
					else
					{

						TraceviewerServer::mainPortNumber = val;
						break;
					}
				}
				else //--port was the last argument
				{
					PrintHelp();
					return false;
				}
				break;
			case xml_port:
				if (c + 1 < argc)
				{ //Possibly a number following
					int val = atoi(argv[c + 1]);
					if ((val < 1024) &&(val != -1))
					{
						PrintHelp();
						return false;
					}
					else
					{
						TraceviewerServer::xmlPortNumber = val;
						break;
					}
				}
				else //--port was the last argument
				{
					PrintHelp();
					return false;
				}
				break;
			default:
			case help:
				PrintHelp();
				return false;
		}
	}
	return true;
}*/

