/*
 * Filter.cpp
 *
 *  Created on: Jun 19, 2013
 *      Author: pat2
 */

#include "FilteredBaseData.hpp"
namespace TraceviewerServer {
FilteredBaseData::FilteredBaseData(string filename, int _headerSize) {
	baseDataFile = new BaseDataFile(filename, _headerSize);
	headerSize = _headerSize;
	baseOffsets = baseDataFile->getOffsets();
	//Filters are default, which is allow everything, so this will initialize the vector
	filter();

}

FilteredBaseData::~FilteredBaseData() {
	delete baseDataFile;
}

void FilteredBaseData::setFilter(Filter _filter)
{
	currentlyAppliedFilter = _filter;
	filter();
}

void FilteredBaseData::filter()
{
	int numFiles = baseDataFile->getNumberOfFiles();
	for (int i = 0; i < numFiles; i++) {
		if (currentlyAppliedFilter.include(baseDataFile->processIDs[i], baseDataFile->threadIDs[i]))
			rankMapping.push_back(i);
	}
}

FileOffset FilteredBaseData::getMinLoc(int pseudoRank) {
	return baseOffsets[rankMapping[pseudoRank]].start + headerSize;
}

FileOffset FilteredBaseData::getMaxLoc(int pseudoRank){
	return baseOffsets[rankMapping[pseudoRank]].end;
}

Long FilteredBaseData::getLong(FileOffset position)
{
	return baseDataFile->getMasterBuffer()->getLong(position);
}
int FilteredBaseData::getInt(FileOffset position)
{
	return baseDataFile->getMasterBuffer()->getInt(position);
}

int FilteredBaseData::getNumberOfRanks()
{
	return rankMapping.size();
}

int* FilteredBaseData::getProcessIDs()
{
	return baseDataFile->processIDs;
}
short* FilteredBaseData::getThreadIDs()
{
	return baseDataFile->threadIDs;
}
}
