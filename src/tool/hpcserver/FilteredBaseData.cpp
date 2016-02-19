// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/FilteredBaseData.cpp $
// $Id: FilteredBaseData.cpp 4307 2013-07-18 17:04:52Z felipet1326@gmail.com $
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
//   $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/FilteredBaseData.cpp $
//
// Purpose:
//   The highest level of the filtering implementation. Abstracts the filter
//   away from the classes that access the file directly.
//   From highest level of abstraction of the file to lowest:
//      FilteredBaseData, BaseDataFile, LargeByteBuffer, VersatileMemoryPage
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************


#include <assert.h>
#include <iostream>


#include "DebugUtils.hpp"
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

void FilteredBaseData::setFilters(FilterSet _filter)
{
	DEBUGCOUT(1) << "setting filters" <<endl;
	currentlyAppliedFilter = _filter;
	filter();
}

void FilteredBaseData::filter()
{
	int numFiles = baseDataFile->getNumberOfFiles();
	rankMapping.clear();
	for (int i = 0; i < numFiles; i++) {
		if (currentlyAppliedFilter.matches(baseDataFile->processIDs[i], baseDataFile->threadIDs[i])){
			rankMapping.push_back(i);
		}
	}

	DEBUGCOUT(1) << "Filtering matched " << rankMapping.size() << " out of "<<numFiles<<endl;
}

FileOffset FilteredBaseData::getMinLoc(int pseudoRank) {
	assert((unsigned int)pseudoRank < rankMapping.size());
	return baseOffsets[rankMapping[pseudoRank]].start + headerSize;
}

FileOffset FilteredBaseData::getMaxLoc(int pseudoRank){
	assert((unsigned int)pseudoRank < rankMapping.size());
	return baseOffsets[rankMapping[pseudoRank]].end;
}

int64_t FilteredBaseData::getLong(FileOffset position)
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
