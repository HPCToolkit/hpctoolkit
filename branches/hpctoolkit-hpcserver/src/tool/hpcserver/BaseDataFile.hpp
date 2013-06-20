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

struct OffsetPair {
	FileOffset start;
	FileOffset end;
};

class BaseDataFile {
public:
	BaseDataFile(string filename, int headerSize);
	virtual ~BaseDataFile();
	int getNumberOfFiles();
	OffsetPair* getOffsets();
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

	OffsetPair* offsets;
};

} /* namespace TraceviewerServer */
#endif /* BASEDATAFILE_H_ */
