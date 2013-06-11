/*
 * LocalDBOpener.h
 *
 *  Created on: Jul 9, 2012
 *      Author: pat2
 */

#ifndef LOCALDBOPENER_H_
#define LOCALDBOPENER_H_
#include <string>
#include "SpaceTimeDataControllerLocal.h"
#include "FileData.h"


using namespace std;
namespace TraceviewerServer
{

	class LocalDBOpener
	{
	public:
		LocalDBOpener();
		virtual ~LocalDBOpener();

		SpaceTimeDataControllerLocal* OpenDbAndCreateSTDC(string);
	private:
		static const int MIN_TRACE_SIZE = 32 + 8 + 24
				+ TraceDataByRankLocal::SIZE_OF_TRACE_RECORD * 2;
		static bool IsCorrectDatabase(string, FileData*);

	};

} /* namespace TraceviewerServer */
#endif /* LOCALDBOPENER_H_ */
