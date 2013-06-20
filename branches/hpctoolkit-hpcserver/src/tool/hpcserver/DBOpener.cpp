/*
 * DBOpener.cpp
 *
 *  Created on: Jul 9, 2012
 *      Author: pat2
 */

#include "DBOpener.hpp"
#include <iostream>
#include "MergeDataFiles.hpp"
#include "FileUtils.hpp"

namespace TraceviewerServer
{
	#define XML_FILENAME "experiment.xml"
	#define TRACE_FILENAME "experiment.mt"

	DBOpener::DBOpener()
	{

	}

	DBOpener::~DBOpener()
	{
		// TODO Auto-generated destructor stub
	}
	SpaceTimeDataController* stdcl;
	SpaceTimeDataController* DBOpener::openDbAndCreateStdc(string pathToDB)
	{
		FileData location;
		FileData* ptrLocation = &location;
		bool hasDatabase = false;
		hasDatabase = verifyDatabase(pathToDB, ptrLocation);

		// If it still doesn't have a database, we assume that the user doesn't
		// want to open a database, so we return null, which makes the calling method return false.
		if (!hasDatabase)
			return NULL;

		stdcl = new SpaceTimeDataController(ptrLocation);
		return stdcl;
	}
	/****
	 * Check if the directory is correct or not. If it is correct, it returns
	 * the XML file and the trace file
	 *
	 * @param directory
	 *            (in): the input directory
	 * @param statusMgr
	 *            (in): status bar
	 * @param experimentFile
	 *            (out): XML file
	 * @param traceFile
	 *            (out): trace file
	 * @return true if the directory is valid, false otherwise
	 *
	 */
	bool DBOpener::verifyDatabase(string directory, FileData* location)
	{
		cout<< "Checking " <<directory<<endl;
		if (FileUtils::existsAndIsDir(directory))
		{
			cout << "\tExists and is a directory"<<endl;
			location->fileXML = FileUtils::combinePaths(directory, XML_FILENAME);
			cout << "\tTrying to open "<<location->fileXML<<endl;
			FILE* XMLfile = fopen(location->fileXML.c_str(), "r");
			//Equivalent of canRead, I believe.
			if (XMLfile != NULL)
			{
				cout<<"\tXML file is not null"<<endl;
				try
				{
					std::string outputFile = FileUtils::combinePaths(directory, TRACE_FILENAME);
					cout<<"\tTrying to open "<<outputFile<<endl;
					MergeDataAttribute att = MergeDataFiles::merge(directory, "*.hpctrace",
							outputFile);
					cout<<"\tMerge resulted in "<<att<<endl;
					if (att != FAIL_NO_DATA)
					{
						location->fileTrace = outputFile;
						if (FileUtils::getFileSize(location->fileTrace) > MIN_TRACE_SIZE)
						{
							return true;
						}
						else
						{
							cerr << "Warning! Trace file " << location->fileTrace << "is too small: "
									<< FileUtils::getFileSize(location->fileTrace) << " bytes." << endl;
							return false;
						}
					}
					else
					{
						cerr << "Error: trace file(s) does not exist or fail to open "
								<< outputFile << endl;
					}

				} catch (int err)
				{
					cerr << "Error code: " << err << endl;
				}

			}

		}
		return false;
	}

} /* namespace TraceviewerServer */
