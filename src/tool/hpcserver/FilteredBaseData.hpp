/*
 * FilteredBaseData.hpp
 *
 *  Created on: Jun 19, 2013
 *      Author: pat2
 */

#ifndef FILTEREDBASEDATA_HPP_
#define FILTEREDBASEDATA_HPP_
#include "ImageTraceAttributes.hpp"
#include "BaseDataFile.hpp"
#include "Filter.hpp"
namespace TraceviewerServer
{
	class FilteredBaseData {
	public:
		FilteredBaseData(string filename, int _headerSize);
		virtual ~FilteredBaseData();

		void setFilter(Filter _filter);

		FileOffset getMinLoc(int pseudoRank);
		FileOffset getMaxLoc(int pseudoRank);
		Long getLong(FileOffset position);
		int getInt(FileOffset position);
		int getNumberOfRanks();
		int* getProcessIDs();
		short* getThreadIDs();
	private:

		void filter();

		BaseDataFile* baseDataFile;
		OffsetPair* baseOffsets;
		Filter currentlyAppliedFilter;
		//Maps the pseudoranks the program asks for from the unfiltered
		//pool to the real ranks from the filtered pool.
		vector<int> rankMapping;
		int headerSize;
	};


}
#endif /* FILTEREDBASEDATA_HPP_ */
