/*
 * DBOpener.h
 *
 *  Created on: Jul 9, 2012
 *      Author: pat2
 */

#ifndef LOCALDBOPENER_H_
#define LOCALDBOPENER_H_
#include <string>
#include "SpaceTimeDataController.hpp"
#include "FileData.hpp"


using namespace std;
namespace TraceviewerServer
{

	class DBOpener
	{
	public:
		DBOpener();
		virtual ~DBOpener();

		SpaceTimeDataController* openDbAndCreateStdc(string);
	private:
		static const int MIN_TRACE_SIZE = 32 + 8 + 24
				+ TraceDataByRank::SIZE_OF_TRACE_RECORD * 2;
		static bool verifyDatabase(string, FileData*);

	};

} /* namespace TraceviewerServer */
#endif /* LOCALDBOPENER_H_ */
