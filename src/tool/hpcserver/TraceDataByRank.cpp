// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/TraceDataByRank.cpp $
// $Id: TraceDataByRank.cpp 4307 2013-07-18 17:04:52Z felipet1326@gmail.com $
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
//   $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/TraceDataByRank.cpp $
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#include "TraceDataByRank.hpp"
#include <algorithm>
#include <cstdlib> // previously: cmath but it causes ambuguity in abs function for gcc 4.4.6
#include "Constants.hpp"
#include <iostream>

namespace TraceviewerServer
{

	TraceDataByRank::TraceDataByRank(FilteredBaseData* _data, int _rank,
			int _numPixelH, int _headerSize)
	{
		data = _data;
		rank = _rank;
		//OffsetPair* offsets = data->getOffsets();
		minloc = data->getMinLoc(rank);
		maxloc = data->getMaxLoc(rank);
		numPixelsH = _numPixelH;

		
		listCPID = new vector<TimeCPID>();

	}
	void TraceDataByRank::getData(Time timeStart, Time timeRange,
			double pixelLength)
	{
		// get the start location
		FileOffset startLoc = findTimeInInterval(timeStart, minloc, maxloc);

		// get the end location
		 Time endTime = timeStart + timeRange;
		 FileOffset endLoc = min(
				findTimeInInterval(endTime, minloc, maxloc) + SIZE_OF_TRACE_RECORD, maxloc);

		// get the number of records data to display
		 Long numRec = 1 + getNumberOfRecords(startLoc, endLoc);

		// --------------------------------------------------------------------------------------------------
		// if the data-to-display is fit in the display zone, we don't need to use recursive binary search
		//	we just simply display everything from the file
		// --------------------------------------------------------------------------------------------------
		if (numRec <= numPixelsH)
		{
			// display all the records
			for (FileOffset i = startLoc; i <= endLoc;)
			{
				listCPID->push_back(getData(i));
				// one record of data contains of an integer (cpid) and a long (time)
				i = i + SIZE_OF_TRACE_RECORD;
			}
		}
		else
		{
			// the data is too big: try to fit the "big" data into the display

			//fills in the rest of the data for this process timeline
			sampleTimeLine(startLoc, endLoc, 0, numPixelsH, 0, pixelLength, timeStart);
		}
		// --------------------------------------------------------------------------------------------------
		// get the last data if necessary: the rightmost time is still less then the upper limit
		// 	I think we can add the rightmost data into the list of samples
		// --------------------------------------------------------------------------------------------------
		if (endLoc < maxloc)
		{
			 TimeCPID dataLast = getData(endLoc);
			addSample(listCPID->size(), dataLast);
		}

		// --------------------------------------------------------------------------------------------------
		// get the first data if necessary: the leftmost time is still bigger than the lower limit
		//	similarly, we add to the list
		// --------------------------------------------------------------------------------------------------
		if (startLoc > minloc)
		{
			 TimeCPID dataFirst = getData(startLoc - SIZE_OF_TRACE_RECORD);
			addSample(0, dataFirst);
		}
		postProcess();
	}
	/*******************************************************************************************
	 * Recursive method that fills in times and timeLine with the correct data from the file.
	 * Takes in two pixel locations as endpoints and finds the timestamp that owns the pixel
	 * in between these two. It then recursively calls itself twice - once with the beginning
	 * location and the newfound location as endpoints and once with the newfound location
	 * and the end location as endpoints. Effectively updates times and timeLine by calculating
	 * the index in which to insert the next data. This way, it keeps times and timeLine sorted.
	 * @author Reed Landrum and Michael Franco
	 * @param minLoc The beginning location in the file to bound the search.
	 * @param maxLoc The end location in the file to bound the search.
	 * @param startPixel The beginning pixel in the image that corresponds to minLoc.
	 * @param endPixel The end pixel in the image that corresponds to maxLoc.
	 * @param minIndex An index used for calculating the index in which the data is to be inserted.
	 * @return Returns the index that shows the size of the recursive subtree that has been read.
	 * Used for calculating the index in which the data is to be inserted.
	 ******************************************************************************************/
	int TraceDataByRank::sampleTimeLine(FileOffset minLoc, FileOffset maxLoc, int startPixel,
			int endPixel, int minIndex, double pixelLength, Time startingTime)
	{
		int midPixel = (startPixel + endPixel) / 2;
		if (midPixel == startPixel)
			return 0;

		Long loc = findTimeInInterval((long)(midPixel * pixelLength + startingTime), minLoc,
				maxLoc);
		 TimeCPID nextData = getData(loc);
		addSample(minIndex, nextData);
		int addedLeft = sampleTimeLine(minLoc, loc, startPixel, midPixel, minIndex,
				pixelLength, startingTime);
		int addedRight = sampleTimeLine(loc, maxLoc, midPixel, endPixel,
				minIndex + addedLeft + 1, pixelLength, startingTime);

		return (addedLeft + addedRight + 1);
	}


	/*********************************************************************************
	 *	Returns the location in the traceFile of the trace data (time stamp and cpid)
	 *	Precondition: the location of the trace data is between minLoc and maxLoc.
	 * @param time: the time to be found
	 * @param left_boundary_offset: the start location. 0 means the beginning of the data in a process
	 * @param right_boundary_offset: the end location.
	 ********************************************************************************/
	FileOffset TraceDataByRank::findTimeInInterval(Time time, FileOffset l_boundOffset,
			FileOffset r_boundOffset)
	{
		if (l_boundOffset == r_boundOffset)
			return l_boundOffset;



		FileOffset l_index = getRelativeLocation(l_boundOffset);
		FileOffset r_index = getRelativeLocation(r_boundOffset);

		Time l_time = data->getLong(l_boundOffset);
		Time r_time = data->getLong(r_boundOffset);
	
		// apply "Newton's method" to find target time
		while (r_index - l_index > 1)
		{
			FileOffset predicted_index;
			//pat2 7/1/13: We only ever divide by rate, and double multiplication
			//is faster than division (confirmed with a benchmark) so compute inverse
			//rate instead. This line of code and the one in the else block account for
			//about 40% of the computation once the data is in memory
			//double rate = (r_time - l_time) / (r_index - l_index);
			double invrate = (r_index - l_index) / (r_time - l_time);
			Time mtime = (r_time - l_time) / 2;
			if (time <= mtime)
			{
				predicted_index = max((Long) ((time - l_time) * invrate) + l_index, l_index);
			}
			else
			{
				predicted_index = min((r_index - (long) ((r_time - time) * invrate)), r_index);
			}
			// adjust so that the predicted index differs from both ends
			// except in the case where the interval is of length only 1
			// this helps us achieve the convergence condition
			if (predicted_index <= l_index)
				predicted_index = l_index + 1;
			if (predicted_index >= r_index)
				predicted_index = r_index - 1;

			Time temp = data->getLong(getAbsoluteLocation(predicted_index));
			if (time >= temp)
			{
				l_index = predicted_index;
				l_time = temp;
			}
			else
			{
				r_index = predicted_index;
				r_time = temp;
			}
		}
		FileOffset l_offset = getAbsoluteLocation(l_index);
		FileOffset r_offset = getAbsoluteLocation(r_index);

		l_time = data->getLong(l_offset);
		r_time = data->getLong(r_offset);

		int leftDiff = time - l_time;
		int rightDiff = r_time - time;
		 bool is_left_closer = abs(leftDiff) < abs(rightDiff);
		if (is_left_closer)
			return l_offset;
		else if (r_offset < maxloc)
			return r_offset;
		else
			return maxloc;
	}
	FileOffset TraceDataByRank::getAbsoluteLocation(FileOffset relativePosition)
	{
		return minloc + (relativePosition * SIZE_OF_TRACE_RECORD);
	}

	FileOffset TraceDataByRank::getRelativeLocation(FileOffset absolutePosition)
	{
		return (absolutePosition - minloc) / SIZE_OF_TRACE_RECORD;
	}
	void TraceDataByRank::addSample(unsigned int index, TimeCPID dataCpid)
	{
		if (index == listCPID->size())
		{
			listCPID->push_back(dataCpid);
		}
		else
		{
			vector<TimeCPID>::iterator it = listCPID->begin();
			it += index;
			listCPID->insert(it, dataCpid);
		}
	}

	TimeCPID TraceDataByRank::getData(FileOffset location)
	{

		 Time time = data->getLong(location);
		 int CPID = data->getInt(location + SIZEOF_LONG);
		TimeCPID ToReturn(time, CPID);
		return ToReturn;
	}

	Long TraceDataByRank::getNumberOfRecords(FileOffset start, FileOffset end)
	{
		return (end - start) / SIZE_OF_TRACE_RECORD;
	}

	/*********************************************************************************************
	 * Removes unnecessary samples:
	 ********************************************************************************************/

	void TraceDataByRank::postProcess()
	{
		int len = listCPID->size();
		for (int i = 0; i < len - 2; i++)
		{

			while (i < len - 1 && (*listCPID)[i].timestamp == (*listCPID)[i + 1].timestamp)
			{
				vector<TimeCPID>::iterator it = listCPID->begin();
				it += (i + 1);
				listCPID->erase(it);
				len--;
			}

		}
	}

	TraceDataByRank::~TraceDataByRank()
	{
		listCPID->clear();
		delete listCPID;
	}

} /* namespace TraceviewerServer */
