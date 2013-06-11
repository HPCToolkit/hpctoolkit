/*
 * ImageTraceAttributes.h
 *
 *  Created on: Jul 9, 2012
 *      Author: pat2
 */

#ifndef IMAGETRACEATTRIBUTES_H_
#define IMAGETRACEATTRIBUTES_H_
namespace TraceviewerServer
{
	struct ImageTraceAttributes
	{
		long begTime, endTime;
		int begProcess, endProcess;
		int numPixelsH, numPixelsV;
		int numPixelsDepthV;

		int lineNum;
	};
}
#endif /* IMAGETRACEATTRIBUTES_H_ */
