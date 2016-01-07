// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/DBOpener.cpp $
// $Id: DBOpener.cpp 4307 2013-07-18 17:04:52Z felipet1326@gmail.com $
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2016, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

//***************************************************************************
//
// File:
//   $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/DBOpener.cpp $
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************
#include <cstdio>

#include "DBOpener.hpp"
#include "DebugUtils.hpp"
#include "MergeDataFiles.hpp"
#include "FileUtils.hpp"
#include "FileData.hpp"
#include "SpaceTimeDataController.hpp"

namespace TraceviewerServer
{
	#define XML_FILENAME "experiment.xml"
	#define TRACE_FILENAME "experiment.mt"

	DBOpener::DBOpener()
	{

	}

	DBOpener::~DBOpener()
	{
		//stdcl, which is the only dynamically allocated thing, gets closed later because
		//it makes sense to have it persist even after the opener.
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

		DEBUGCOUT(2) << "Checking " <<directory<<endl;

		if (FileUtils::existsAndIsDir(directory))
		{

			DEBUGCOUT(2) << "\tExists and is a directory"<<endl;

			location->fileXML = FileUtils::combinePaths(directory, XML_FILENAME);

			DEBUGCOUT(2) << "\tTrying to open "<<location->fileXML<<endl;

			if (FileUtils::exists(location->fileXML))
			{

				DEBUGCOUT(2) <<"\tXML file is not null"<<endl;

				try
				{
					std::string outputFile = FileUtils::combinePaths(directory, TRACE_FILENAME);

					DEBUGCOUT(2) <<"\tTrying to open "<<outputFile<<endl;

					MergeDataAttribute att = MergeDataFiles::merge(directory, "*.hpctrace",
							outputFile);

					DEBUGCOUT(2) <<"\tMerge resulted in "<<att<<endl;

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
