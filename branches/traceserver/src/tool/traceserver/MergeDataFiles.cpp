/*
 * MergeDataFiles.cpp
 *
 *  Created on: Jul 9, 2012
 *      Author: pat2
 */

#include "MergeDataFiles.h"
#include "ByteUtilities.h"
#include "Constants.h"
#include "FileUtils.h"
#include <iostream>
#include <algorithm>
#include <iterator>

using namespace std;
typedef int64_t Long;
namespace TraceviewerServer
{
	MergeDataAttribute MergeDataFiles::merge(string Directory, string GlobInputFile,
			string OutputFile)
	{
		const int Last_dot = GlobInputFile.find_last_of('.');
		const string Suffix = GlobInputFile.substr(Last_dot);

		cout << "Checking to see if " << OutputFile << " exists" << endl;

		if (FileUtils::Exists(OutputFile))
		{
			cout << "Exists" << endl;
			if (IsMergedFileCorrect(&OutputFile))
				return SUCCESS_ALREADY_CREATED;
			// the file exists but corrupted. In this case, we have to remove and create a new one
			cout << "Database file may be corrupted. Continuing" << endl;
			return STATUS_UNKNOWN;
			//remove(OutputFile.string().c_str());
		}
		cout << "Doesn't exist" << endl;
		// check if the files in glob patterns is correct

		if (!AtLeastOneValidFile(Directory))
		{
			return FAIL_NO_DATA;
		}

		DataOutputFileStream dos(OutputFile.c_str());

		//-----------------------------------------------------
		// 1. write the header:
		//  int type (0: unknown, 1: mpi, 2: openmp, 3: hybrid, ...
		//	int num_files
		//-----------------------------------------------------

		int type = 0;
		dos.WriteInt(type);

		vector<string> AllPaths = FileUtils::GetAllFilesInDir(Directory);
		vector<string> Filtered;
		vector<string>::iterator it;
		for (it = AllPaths.begin(); it != AllPaths.end(); it++)
		{
			string val = *it;
			if (val.find(".hpctrace") < string::npos)//This is hardcoded, which isn't great but will have to do because GlobInputFile is regex-style ("*.hpctrace")
				Filtered.push_back(val);
		}
		// on linux, we have to sort the files
		//To sort them, we need a random access iterator, which means we need to load all of them into a vector
		sort(Filtered.begin(), Filtered.end());

		dos.WriteInt(Filtered.size());
		const Long num_metric_header = 2 * Constants::SIZEOF_INT; // type of app (4 bytes) + num procs (4 bytes)
		const Long num_metric_index = Filtered.size()
				* (Constants::SIZEOF_LONG + 2 * Constants::SIZEOF_INT);
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
		for (it2 = Filtered.begin(); it2 < Filtered.end(); it2++)
		{

			const string Filename = *it2;
			const int last_pos_basic_name = Filename.length() - Suffix.length();
			const string Basic_name = Filename.substr(FileUtils::CombinePaths(Directory, "").length(),//This ensures we count the "/" at the end of the path
					last_pos_basic_name);
			cout << "Path manipulation check: The file in " << Filename << " is "
					<< Basic_name << endl;

			vector<string> tokens = SplitString(Basic_name, '-');


			const int num_tokens = tokens.size();
			if (num_tokens < PROC_POS)
				// if it is wrong file with the right extension, we skip
				continue;
			int proc;
			string Token_To_Parse = tokens[name_format + num_tokens - PROC_POS];
			proc = atoi(Token_To_Parse.c_str());
			if ((proc == 0) && (!StringActuallyZero(Token_To_Parse))) //catch (NumberFormatException e)
			{
				// old version of name format
				name_format = 1;
				string Token_To_Parse = tokens[name_format + num_tokens - PROC_POS];
				proc = atoi(Token_To_Parse.c_str());
			}
			dos.WriteInt(proc);
			if (proc != 0)
				type |= Constants::MULTI_PROCESSES;
			const int Thread = atoi(tokens[name_format + num_tokens - THREAD_POS].c_str());
			dos.WriteInt(Thread);
			if (Thread != 0)
				type |= Constants::MULTI_THREADING;
			dos.WriteLong(offset);
			offset += FileUtils::GetFileSize(Filename);
		}
		//-----------------------------------------------------
		// 3. Copy all data from the multiple files into one file
		//-----------------------------------------------------
		for (it2 = Filtered.begin(); it2 < Filtered.end(); it2++)
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
		InsertMarker(&dos);
		dos.close();
		//-----------------------------------------------------
		// 4. FIXME: write the type of the application
		//  	the type of the application is computed in step 2
		//		Ideally, this step has to be in the beginning !
		//-----------------------------------------------------
		//While we don't actually want to do any input operations, adding the input flag prevents the file from being truncated to 0 bytes
		DataOutputFileStream f(OutputFile.c_str(), ios_base::in | ios_base::out | ios_base::binary);
		f.WriteInt(type);
		f.close();

		//-----------------------------------------------------
		// 5. remove old files
		//-----------------------------------------------------
		RemoveFiles(Filtered);
		return SUCCESS_MERGED;
	}

	bool MergeDataFiles::StringActuallyZero(string ToTest)
	{
		for (int var = 0; var < ToTest.length(); var++)
		{
			if (ToTest[var] != '0')
				return false;
		}
		return true;
	}

	void MergeDataFiles::InsertMarker(DataOutputFileStream* dos)
	{
		dos->WriteLong(MARKER_END_MERGED_FILE);
	}
	bool MergeDataFiles::IsMergedFileCorrect(string* filename)
	{
		ifstream f(filename->c_str(), ios_base::binary | ios_base::in);
		bool IsCorrect = false;
		const Long pos = FileUtils::GetFileSize(*filename) - Constants::SIZEOF_LONG;
		int diff;
		if (pos > 0)
		{
			f.seekg(pos, ios_base::beg);
			char buffer[8];
			f.read(buffer, 8);
			const ULong Marker = ByteUtilities::ReadLong(buffer);
			//No idea why this doesn't work:
			//IsCorrect = ((Marker) == MARKER_END_MERGED_FILE);

			diff = (Marker) - MARKER_END_MERGED_FILE;
			IsCorrect = (diff) == 0;
		}
		f.close();
		return IsCorrect;
	}
	bool MergeDataFiles::RemoveFiles(vector<string> vect)
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
	bool MergeDataFiles::AtLeastOneValidFile(string dir)
	{
		vector<string> FileList = FileUtils::GetAllFilesInDir(dir);
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
	vector<string> MergeDataFiles::SplitString(string toSplit, char delimiter)
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
