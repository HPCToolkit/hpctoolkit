/*
 * SpaceTimeDataController.h
 *
 *  Created on: Jul 9, 2012
 *      Author: pat2
 */

#ifndef SpaceTimeDataController_H_
#define SpaceTimeDataController_H_
#include "FileData.hpp"
#include "ImageTraceAttributes.hpp"
#include "BaseDataFile.hpp"
#include "ProcessTimeline.hpp"

namespace TraceviewerServer
{

	class SpaceTimeDataController
	{
	public:
		SpaceTimeDataController();
		SpaceTimeDataController(FileData*);
		virtual ~SpaceTimeDataController();
		void setInfo(Long, Long, int);
		ProcessTimeline* getNextTrace(bool);
		void addNextTrace(ProcessTimeline*);
		void fillTraces(int, bool);
		ProcessTimeline* fillTrace(bool);
		void prepareViewportPainting(bool);

		//The number of processes in the database, independent of the current display size
		int getNumRanks();

		 int* getValuesXProcessID();
		 short* getValuesXThreadID();

		std::string getExperimentXML();
		ImageTraceAttributes* attributes;
		ProcessTimeline** traces;
		int tracesLength;
	private:
		int lineToPaint(int);

		ImageTraceAttributes* oldAttributes;

		BaseDataFile* dataTrace;
		int headerSize;

		// The minimum beginning and maximum ending time stamp across all traces (in microseconds).
		Long maxEndTime, minBegTime;

		int height;
		string experimentXML;
		string fileTrace;

		bool tracesInitialized;

		static const int DEFAULT_HEADER_SIZE = 24;

	};

} /* namespace TraceviewerServer */
#endif /* SpaceTimeDataController_H_ */
