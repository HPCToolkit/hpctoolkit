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

	class FileUtils
	{
	public:
		static string CombinePaths(string FirstPart, string SecondPart)
		{
			string FirstPartWithoutEndingSlash;
			if (FirstPart[FirstPart.length()-1]=='/')
				FirstPartWithoutEndingSlash = FirstPart.substr(0,FirstPart.length()-1);
			else
				FirstPartWithoutEndingSlash = FirstPart;
			string SecondPartWithoutStartingSlash;
			if (SecondPart[0]=='/')
				SecondPartWithoutStartingSlash = SecondPart.substr(1, SecondPart.length()-1);
			else
				SecondPartWithoutStartingSlash = SecondPart;
			return FirstPartWithoutEndingSlash + "/" + SecondPartWithoutStartingSlash;

		}

		static bool ExistsAndIsDir(string p)
		{
			struct stat DirInfo;
			int err = stat(p.c_str(), &DirInfo);
			bool isDir = S_ISDIR(DirInfo.st_mode);
			if ((err!=0)|| !isDir)
			{
				cout<<"ExistsAndIsDir returning false. Err: "<< err << " isDir: " << isDir << " mode: "<<DirInfo.st_mode<< " Errno: " << errno << endl;
			}
			return (err == 0) && isDir;
		}

		static bool Exists(string p)
		{
			struct stat DirInfo;
			int err = stat(p.c_str(), &DirInfo);
			return (err == 0);
		}

		static long GetFileSize(string p)
		{
			struct stat DirInfo;
			int err = stat(p.c_str(), &DirInfo);
			if (err != 0)
				cerr << "Tried to get file size when file does not exist!" << endl;
			return DirInfo.st_size;
		}
		static vector<string> GetAllFilesInDir(string directory)
		{
			vector<string> ValidFiles;
			DIR* TestDir;
			dirent* entry;
			TestDir = opendir(directory.c_str());
			while ((entry = readdir(TestDir)))
			{
				string FullPath = CombinePaths(directory, string(entry->d_name));

				struct stat DirInfo;
				bool err = (stat(FullPath.c_str(), &DirInfo) != 0);
				bool isDir = S_ISDIR(DirInfo.st_mode);
				bool aFile = !(err || isDir);
				if (aFile)
					ValidFiles.push_back(FullPath);

			}
			closedir(TestDir);
			return ValidFiles;
		}
	};

} /* namespace TraceviewerServer */
#endif /* FILEUTILS_H_ */
