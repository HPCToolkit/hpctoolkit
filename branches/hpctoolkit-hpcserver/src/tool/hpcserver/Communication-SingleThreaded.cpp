/*
 * Communication-SingleThreaded.cpp
 *
 *  Created on: Jun 28, 2013
 *      Author: pat2
 */


#include "Communication.hpp"
#include "ProcessTimeline.hpp"
#include "CompressingDataSocketLayer.hpp"
#include "ImageTraceAttributes.hpp"
#include "DebugUtils.hpp"
#include "Server.hpp"

#include <vector>

using namespace std;
namespace TraceviewerServer {

void Communication::sendParseInfo(uint64_t minBegTime, uint64_t maxEndTime, int headerSize)
{//Do nothing
}

void Communication::sendParseOpenDB(string pathToDB) {}

void Communication::sendStartGetData(SpaceTimeDataController* contr, int processStart, int processEnd,
			Time timeStart, Time timeEnd, int verticalResolution, int horizontalResolution)
{

	ImageTraceAttributes* correspondingAttributes = contr->attributes;

	correspondingAttributes->begProcess = processStart;
	correspondingAttributes->endProcess = processEnd;
	correspondingAttributes->numPixelsH = horizontalResolution;
	correspondingAttributes->numPixelsV = verticalResolution;
	correspondingAttributes->begTime =  timeStart;
	correspondingAttributes->endTime =  timeEnd;
	correspondingAttributes->lineNum = 0;


}
void Communication::sendEndGetData(DataSocketStream* stream, ProgressBar* prog, SpaceTimeDataController* controller)
{
	// TODO: Make this so that the Lines get sent as soon as they are
	// filled.

	controller->fillTraces();
	for (int i = 0; i < controller->tracesLength; i++)
	{

		ProcessTimeline* timeline = controller->traces[i];
		stream->writeInt( timeline->line());
		vector<TimeCPID> data = *timeline->data->listCPID;
		stream->writeInt( data.size());
		// Begin time
		stream->writeLong( data[0].timestamp);
		//End time
		stream->writeLong( data[data.size() - 1].timestamp);

		CompressingDataSocketLayer comprStr;

		vector<TimeCPID>::iterator it;
		DEBUGCOUT(2) << "Sending process timeline with " << data.size() << " entries" << endl;


		Time currentTime = data[0].timestamp;
		for (it = data.begin(); it != data.end(); ++it)
		{
			comprStr.writeInt( (int)(it->timestamp - currentTime));
			comprStr.writeInt( it->cpid);
			currentTime = it->timestamp;
		}
		comprStr.flush();
		int outputBufferLen = comprStr.getOutputLength();
		char* outputBuffer = (char*)comprStr.getOutputBuffer();

		stream->writeInt(outputBufferLen);

		stream->writeRawData(outputBuffer, outputBufferLen);
		prog->incrementProgress();
	}
	stream->flush();
}

void Communication::sendStartFilter(int count, bool excludeMatches)
{//Do nothing
}

void Communication::sendFilter(BinaryRepresentationOfFilter filt)
{
}

ServerType Communication::basicInit(int argc, char** argv)
{
	return MASTER;
}
void Communication::run(ServerType type)
{
	//type == MASTER
	TraceviewerServer::Server();
}
void Communication::closeServer()
{
}
}
