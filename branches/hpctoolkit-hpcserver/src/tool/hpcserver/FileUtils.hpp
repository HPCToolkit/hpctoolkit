/*
 * FileUtils.h
 *
 *  Created on: Jul 18, 2012
 *      Author: pat2
 */

#ifndef FILEUTILS_H_
#define FILEUTILS_H_

#include "sys/stat.h"
#include "dirent.h"
#include <iostream>
#include <vector>
#include <errno.h>

using namespace std;
namespace TraceviewerServer
{
#define NO_ERROR 0
	class FileUtils
	{
	public:
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

		static bool existsAndIsDir(string p)
		{
			struct stat DirInfo;
			int err = stat(p.c_str(), &DirInfo);
			bool isDir = S_ISDIR(DirInfo.st_mode);
			if ((err!=NO_ERROR)|| !isDir)
			{
				cout<<"Either does not exit or is not directory: File " << p<< " Err: "<< err << " isDir: " << isDir << " mode: "<<DirInfo.st_mode<< " Errno: " << errno << endl;
			}
			return (err == 0) && isDir;
		}

		static bool exists(string p)
		{
			struct stat DirInfo;
			int err = stat(p.c_str(), &DirInfo);
			return (err == NO_ERROR);
		}

		static long getFileSize(string p)
		{
			struct stat DirInfo;
			int err = stat(p.c_str(), &DirInfo);
			if (err != NO_ERROR)
				cerr << "Tried to get file size when file does not exist!" << endl;
			return DirInfo.st_size;
		}
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

		//We need this because of the way atoi works.
		static bool stringActuallyZero(string toTest)
		{
			for (unsigned int var = 0; var < toTest.length(); var++)
			{
				if (toTest[var] != '0')
					return false;
			}
			return true;
		}
	};

} /* namespace TraceviewerServer */
#endif /* FILEUTILS_H_ */
