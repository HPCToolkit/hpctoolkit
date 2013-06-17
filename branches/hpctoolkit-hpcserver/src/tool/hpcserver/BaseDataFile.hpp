/*
 * BaseDataFile.h
 *
 *  Created on: Jul 8, 2012
 *      Author: pat2
 */

#ifndef BASEDATAFILE_H_
#define BASEDATAFILE_H_

using namespace std;

#include "LargeByteBuffer.hpp"
#include "ByteUtilities.hpp"

namespace TraceviewerServer {

class BaseDataFile {
public:
	BaseDataFile(string, int);
	virtual ~BaseDataFile();
	int getNumberOfFiles();
	Long* getOffsets();
	LargeByteBuffer* getMasterBuffer();
	void setData(string, int);

	bool isMultiProcess();
	bool isMultiThreading();
	bool isHybrid();

	//string* ValuesX;
	int* processIDs;
	short* threadIDs;
private:
	int type; // = Constants::MULTI_PROCESSES | Constants::MULTI_THREADING;
	LargeByteBuffer* masterBuff;
	int numFiles; // = 0;

	Long* offsets;
};

} /* namespace TraceviewerServer */
#endif /* BASEDATAFILE_H_ */
