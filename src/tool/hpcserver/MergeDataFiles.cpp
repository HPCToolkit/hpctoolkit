/*
 * MergeDataFiles.cpp
 *
 *  Created on: Jul 9, 2012
 *      Author: pat2
 */

#include "MergeDataFiles.hpp"
#include "ByteUtilities.hpp"
#include "Constants.hpp"
#include "FileUtils.hpp"
#include <iostream>
#include <algorithm>
#include <iterator>

using namespace std;
typedef int64_t Long;
namespace TraceviewerServer
{
	MergeDataAttribute MergeDataFiles::merge(string directory, string globInputFile,
			string outputFile)
	{
		 int lastDot = globInputFile.find_last_of('.');
		 string suffix = globInputFile.substr(lastDot);

		cout << "Checking to see if " << outputFile << " exists" << endl;

		if (FileUtils::exists(outputFile))
		{
			cout << "Exists" << endl;
			if (isMergedFileCorrect(&outputFile))
				return SUCCESS_ALREADY_CREATED;
			// the file exists but corrupted. In this case, we have to remove and create a new one
			cout << "Database file may be corrupted. Continuing" << endl;
			return STATUS_UNKNOWN;
			//remove(OutputFile.string().c_str());
		}
		cout << "Doesn't exist" << endl;
		// check if the files in glob patterns is correct

		if (!atLeastOneValidFile(directory))
		{
			return FAIL_NO_DATA;
		}

		DataOutputFileStream dos(outputFile.c_str());

		//-----------------------------------------------------
		// 1. write the header:
		//  int type (0: unknown, 1: mpi, 2: openmp, 3: hybrid, ...
		//	int num_files
		//-----------------------------------------------------

		int type = 0;
		dos.writeInt(type);

		vector<string> allPaths = FileUtils::getAllFilesInDir(directory);
		vector<string> filtered;
		vector<string>::iterator it;
		for (it = allPaths.begin(); it != allPaths.end(); it++)
		{
			string val = *it;
			if (val.find(".hpctrace") < string::npos)//This is hardcoded, which isn't great but will have to do because GlobInputFile is regex-style ("*.hpctrace")
				filtered.push_back(val);
		}
		// on linux, we have to sort the files
		//To sort them, we need a random access iterator, which means we need to load all of them into a vector
		sort(filtered.begin(), filtered.end());

		dos.writeInt(filtered.size());
		const Long num_metric_header = 2 * SIZEOF_INT; // type of app (4 bytes) + num procs (4 bytes)
		 Long num_metric_index = filtered.size()
				* (SIZEOF_LONG + 2 * SIZEOF_INT);
		Long offset = num_metric_header + num_metric_index;

		int name_format = 0; // FIXME hack:some hpcprof revisions have different format name !!
		//-----------------------------------------------------
		// 2. Record the process ID, thread ID and the offset
		//   It will also detect if the application is mp, mt, or hybrid
		//	 no accelator is supported
		//  for all files:
		//		int proc-id, int thread-id, long offset
		//-----------------------------------------------------
		vector<string>::iterator it2;
		for (it2 = filtered.begin(); it2 < filtered.end(); it2++)
		{

			 string Filename = *it2;
			 int last_pos_basic_name = Filename.length() - suffix.length();
			 string Basic_name = Filename.substr(FileUtils::combinePaths(directory, "").length(),//This ensures we count the "/" at the end of the path
					last_pos_basic_name);
			cout << "Path manipulation check: The file in " << Filename << " is "
					<< Basic_name << endl;

			vector<string> tokens = splitString(Basic_name, '-');


			 int num_tokens = tokens.size();
			if (num_tokens < PROC_POS)
				// if it is wrong file with the right extension, we skip
				continue;
			int proc;
			string Token_To_Parse = tokens[name_format + num_tokens - PROC_POS];
			proc = atoi(Token_To_Parse.c_str());
			if ((proc == 0) && (!stringActuallyZero(Token_To_Parse))) //catch (NumberFormatException e)
			{
				// old version of name format
				name_format = 1;
				string Token_To_Parse = tokens[name_format + num_tokens - PROC_POS];
				proc = atoi(Token_To_Parse.c_str());
			}
			dos.writeInt(proc);
			if (proc != 0)
				type |= MULTI_PROCESSES;
			 int Thread = atoi(tokens[name_format + num_tokens - THREAD_POS].c_str());
			dos.writeInt(Thread);
			if (Thread != 0)
				type |= MULTI_THREADING;
			dos.writeLong(offset);
			offset += FileUtils::getFileSize(Filename);
		}
		//-----------------------------------------------------
		// 3. Copy all data from the multiple files into one file
		//-----------------------------------------------------
		for (it2 = filtered.begin(); it2 < filtered.end(); it2++)
		{
			string i = *it2;

			ifstream dis(i.c_str(), ios_base::binary | ios_base::in);
			char data[PAGE_SIZE_GUESS];
			dis.read(data, PAGE_SIZE_GUESS);
			int NumRead = dis.gcount();
			while (NumRead > 0)
			{
				dos.write(data, NumRead);
				dis.read(data, PAGE_SIZE_GUESS);
				NumRead = dis.gcount();
			}
			dis.close();
		}
		insertMarker(&dos);
		dos.close();
		//-----------------------------------------------------
		// 4. FIXME: write the type of the application
		//  	the type of the application is computed in step 2
		//		Ideally, this step has to be in the beginning !
		//-----------------------------------------------------
		//While we don't actually want to do any input operations, adding the input flag prevents the file from being truncated to 0 bytes
		DataOutputFileStream f(outputFile.c_str(), ios_base::in | ios_base::out | ios_base::binary);
		f.writeInt(type);
		f.close();

		//-----------------------------------------------------
		// 5. remove old files
		//-----------------------------------------------------
		removeFiles(filtered);
		return SUCCESS_MERGED;
	}

	bool MergeDataFiles::stringActuallyZero(string ToTest)
	{
		for (int var = 0; var < ToTest.length(); var++)
		{
			if (ToTest[var] != '0')
				return false;
		}
		return true;
	}

	void MergeDataFiles::insertMarker(DataOutputFileStream* dos)
	{
		dos->writeLong(MARKER_END_MERGED_FILE);
	}
	bool MergeDataFiles::isMergedFileCorrect(string* filename)
	{
		ifstream f(filename->c_str(), ios_base::binary | ios_base::in);
		bool IsCorrect = false;
		 Long pos = FileUtils::getFileSize(*filename) - SIZEOF_LONG;
		int diff;
		if (pos > 0)
		{
			f.seekg(pos, ios_base::beg);
			char buffer[8];
			f.read(buffer, 8);
			 ULong Marker = ByteUtilities::readLong(buffer);
			//No idea why this doesn't work:
			//IsCorrect = ((Marker) == MARKER_END_MERGED_FILE);

			diff = (Marker) - MARKER_END_MERGED_FILE;
			IsCorrect = (diff) == 0;
		}
		f.close();
		return IsCorrect;
	}
	bool MergeDataFiles::removeFiles(vector<string> vect)
	{
		bool success = true;
		vector<string>::iterator it;
		for (it = vect.begin(); it != vect.end(); ++it)
		{
			bool thisSuccess = (remove(it->c_str()) == 0);
			success &= thisSuccess;
		}
		return success;
	}
	bool MergeDataFiles::atLeastOneValidFile(string dir)
	{
		vector<string> FileList = FileUtils::getAllFilesInDir(dir);
		vector<string>::iterator it;
		for (it = FileList.begin(); it != FileList.end(); ++it)
		{
			string filename = *it;

			int l = filename.length();
			//if it ends with ".hpctrace", we are good.
			string ending = ".hpctrace";
			if (l < ending.length())
				continue;
			string supposedext = filename.substr(l - ending.length());

			if (ending.compare(supposedext) == 0)
			{
				return true;
			}
		}
		return false;
	}
	vector<string> MergeDataFiles::splitString(string toSplit, char delimiter)
	{
		vector<string> ToReturn;
		int CurrentStartPos = 0;
		int CurrentSize = 0;
		for (int var = 0; var < toSplit.length(); var++) {
			if (toSplit[var] == delimiter)
			{
				ToReturn.push_back(toSplit.substr(CurrentStartPos, CurrentSize));
				CurrentStartPos = var+1;
				CurrentSize = 0;
			}
			else
				CurrentSize++;
		}
		ToReturn.push_back(toSplit.substr(CurrentStartPos, CurrentSize));
		return ToReturn;
	}

} /* namespace TraceviewerServer */
