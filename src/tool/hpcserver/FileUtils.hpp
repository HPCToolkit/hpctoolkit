// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/FileUtils.hpp $
// $Id: FileUtils.hpp 4294 2013-07-11 00:15:53Z felipet1326@gmail.com $
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
//   $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/FileUtils.hpp $
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef FILEUTILS_H_
#define FILEUTILS_H_

#include "sys/stat.h"
#include "dirent.h"
#include <iostream>
#include <vector>
#include <errno.h>
#include <string>
#include <cstring>
#include <stdint.h>

using namespace std;
namespace TraceviewerServer
{
#define NO_ERROR 0

	typedef int FileDescriptor;
	typedef uint64_t FileOffset;

	class FileUtils
	{
	public:
		//Combines UNIX-style paths, taking care to ensure that there is always exactly 1 / joining firstPart and secondPart
		static string combinePaths(string firstPart, string secondPart)
		{
			string firstPartWithoutEndingSlash;
			if (firstPart[firstPart.length()-1]=='/')
				firstPartWithoutEndingSlash = firstPart.substr(0,firstPart.length()-1);
			else
				firstPartWithoutEndingSlash = firstPart;
			string recondPartWithoutStartingSlash;
			if (secondPart[0]=='/')
				recondPartWithoutStartingSlash = secondPart.substr(1, secondPart.length()-1);
			else
				recondPartWithoutStartingSlash = secondPart;
			return firstPartWithoutEndingSlash + "/" + recondPartWithoutStartingSlash;

		}
		//Uses stat to check if the specified folder exists and if it is indeed a folder.
		static bool existsAndIsDir(string p)
		{
			struct stat DirInfo;
			int err = stat(p.c_str(), &DirInfo);
			bool isDir = S_ISDIR(DirInfo.st_mode);
			if ((err!=0)|| !isDir)
			{
				cerr<<"Either does not exist or is not directory: File " << p<< " Err: "<< err << " isDir: " << isDir << " mode: "<<DirInfo.st_mode<< " Error: " << strerror(errno) << endl;
			}
			return (err == 0) && isDir;
		}

		//Uses stat to check if the specified file exists
		static bool exists(string p)
		{
			struct stat DirInfo;
			int err = stat(p.c_str(), &DirInfo);
			return (err == 0);
		}
		//Gets the file size of a file (the file must exist)
		static FileOffset getFileSize(string p)
		{
			struct stat DirInfo;
			int err = stat(p.c_str(), &DirInfo);
			if (err != 0)
				cerr << "Tried to get file size when file does not exist!" << endl;
			return DirInfo.st_size;
		}
		//Gets a list of all files in the directory, excluding any subfolders in the directory
		static vector<string> getAllFilesInDir(string directory)
		{
			vector<string> validFiles;
			DIR* testDir;
			dirent* entry;
			testDir = opendir(directory.c_str());
			while ((entry = readdir(testDir)))
			{
				string fullPath = combinePaths(directory, string(entry->d_name));

				struct stat dirInfo;
				bool err = (stat(fullPath.c_str(), &dirInfo) != 0);
				bool isDir = S_ISDIR(dirInfo.st_mode);
				bool aFile = !(err || isDir);
				if (aFile)
					validFiles.push_back(fullPath);

			}
			closedir(testDir);
			return validFiles;
		}

		//Because atoi returns 0 when the string is invalid, there's no easy way
		//to distinguish between "0" and an invalid string. This method helps with
		//that by testing the string to ensure it is only whitespace and '0's.
		static bool stringActuallyZero(string toTest)
		{
			for (unsigned int var = 0; var < toTest.length(); var++)
			{
				if (toTest[var] != '0' && toTest[var] >= ' ')
					return false;
			}
			return true;
		}
	};

} /* namespace TraceviewerServer */
#endif /* FILEUTILS_H_ */
