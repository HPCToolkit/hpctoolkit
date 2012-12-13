/*
 * TraceDataByRankLocal.cpp
 *
 *  Created on: Jul 9, 2012
 *      Author: pat2
 */

#include "TraceDataByRankLocal.h"
#include <cmath>
#include "Constants.h"
#include <iostream>

namespace TraceviewerServer
{

	TraceDataByRankLocal::TraceDataByRankLocal(BaseDataFile* data, int rank,
			int numPixelH, int header_size)
	{
		Data = data;
		Rank = rank;
		Long* Offsets = Data->getOffsets();
		Minloc = Offsets[Rank] + header_size;
		Maxloc = (
				(Rank + 1 < Data->getNumberOfFiles()) ?
						Offsets[Rank + 1] : data->getMasterBuffer()->Size() - 1)
				- SIZE_OF_TRACE_RECORD;
if (Maxloc< Minloc)
cerr<<"Maxloc is smaller than Minloc!! Maxloc= "<<Maxloc << " Minloc= " << Minloc << " Rank: " << Rank << " Num files= " << Data->getNumberOfFiles() << "MastB size=" << Data->getMasterBuffer()->Size()<<endl; 
		NumPixelsH = numPixelH;
		TimeCPID empty(0, 0);
		vector<TimeCPID> _listcpid;
		ListCPID = _listcpid;

	}
	void TraceDataByRankLocal::GetData(double timeStart, double timeRange,
			double pixelLength)
	{
		// get the start location
		const Long startLoc = FindTimeInInterval(timeStart, Minloc, Maxloc);

		// get the end location
		const double endTime = timeStart + timeRange;
		const Long endLoc = min(
				FindTimeInInterval(endTime, Minloc, Maxloc) + SIZE_OF_TRACE_RECORD, Maxloc);

		// get the number of records data to display
		const Long numRec = 1 + GetNumberOfRecords(startLoc, endLoc);

		// --------------------------------------------------------------------------------------------------
		// if the data-to-display is fit in the display zone, we don't need to use recursive binary search
		//	we just simply display everything from the file
		// --------------------------------------------------------------------------------------------------
		if (numRec <= NumPixelsH)
		{
			// display all the records
			for (Long i = startLoc; i <= endLoc;)
			{
				ListCPID.push_back(GetData(i));
				// one record of data contains of an integer (cpid) and a long (time)
				i = i + SIZE_OF_TRACE_RECORD;
			}
		}
		else
		{
			// the data is too big: try to fit the "big" data into the display

			//fills in the rest of the data for this process timeline
			SampleTimeLine(startLoc, endLoc, 0, NumPixelsH, 0, pixelLength, timeStart);
		}
		// --------------------------------------------------------------------------------------------------
		// get the last data if necessary: the rightmost time is still less then the upper limit
		// 	I think we can add the rightmost data into the list of samples
		// --------------------------------------------------------------------------------------------------
		if (endLoc < Maxloc)
		{
			const TimeCPID dataLast = GetData(endLoc);
			AddSample(ListCPID.size(), dataLast);
		}

		// --------------------------------------------------------------------------------------------------
		// get the first data if necessary: the leftmost time is still bigger than the lower limit
		//	similarly, we add to the list
		// --------------------------------------------------------------------------------------------------
		if (startLoc > Minloc)
		{
			const TimeCPID dataFirst = GetData(startLoc - SIZE_OF_TRACE_RECORD);
			AddSample(0, dataFirst);
		}
		//PostProcess();
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
	int TraceDataByRankLocal::SampleTimeLine(Long minLoc, Long maxLoc, int startPixel,
			int endPixel, int minIndex, double pixelLength, double startingTime)
	{
		int midPixel = (startPixel + endPixel) / 2;
		if (midPixel == startPixel)
			return 0;

		Long loc = FindTimeInInterval(midPixel * pixelLength + startingTime, minLoc,
				maxLoc);
		const TimeCPID nextData = GetData(loc);
		AddSample(minIndex, nextData);
		int addedLeft = SampleTimeLine(minLoc, loc, startPixel, midPixel, minIndex,
				pixelLength, startingTime);
		int addedRight = SampleTimeLine(loc, maxLoc, midPixel, endPixel,
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
	Long TraceDataByRankLocal::FindTimeInInterval(double time, Long l_boundOffset,
			Long r_boundOffset)
	{
		if (l_boundOffset == r_boundOffset)
			return l_boundOffset;

		LargeByteBuffer* const masterBuff = Data->getMasterBuffer();

		Long l_index = GetRelativeLocation(l_boundOffset);
		Long r_index = GetRelativeLocation(r_boundOffset);

		double l_time = masterBuff->GetLong(l_boundOffset);
		double r_time = masterBuff->GetLong(r_boundOffset);

		// apply "Newton's method" to find target time
		while (r_index - l_index > 1)
		{
			Long predicted_index;
			double rate = (r_time - l_time) / (r_index - l_index);
			double mtime = (r_time - l_time) / 2;
			if (time <= mtime)
			{
				predicted_index = max((Long) ((time - l_time) / rate) + l_index, l_index);
			}
			else
			{
				predicted_index = min((r_index - (long) ((r_time - time) / rate)), r_index);
			}
			// adjust so that the predicted index differs from both ends
			// except in the case where the interval is of length only 1
			// this helps us achieve the convergence condition
			if (predicted_index <= l_index)
				predicted_index = l_index + 1;
			if (predicted_index >= r_index)
				predicted_index = r_index - 1;

			double temp = masterBuff->GetLong(GetAbsoluteLocation(predicted_index));
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
		long l_offset = GetAbsoluteLocation(l_index);
		long r_offset = GetAbsoluteLocation(r_index);

		l_time = masterBuff->GetLong(l_offset);
		r_time = masterBuff->GetLong(r_offset);

		const bool is_left_closer = abs(time - l_time) < abs(r_time - time);

		if (is_left_closer)
			return l_offset;
		else if (r_offset < Maxloc)
			return r_offset;
		else
			return Maxloc;
	}
	Long TraceDataByRankLocal::GetAbsoluteLocation(Long RelativePosition)
	{
		return Minloc + (RelativePosition * SIZE_OF_TRACE_RECORD);
	}

	Long TraceDataByRankLocal::GetRelativeLocation(Long AbsolutePosition)
	{
		return (AbsolutePosition - Minloc) / SIZE_OF_TRACE_RECORD;
	}
	void TraceDataByRankLocal::AddSample(int index, TimeCPID dataCpid)
	{
		if (index == ListCPID.size())
		{
			ListCPID.push_back(dataCpid);
		}
		else
		{
			vector<TimeCPID>::iterator it = ListCPID.begin();
			it += index;
			ListCPID.insert(it, dataCpid);
		}
	}

	TimeCPID TraceDataByRankLocal::GetData(Long location)
	{
		LargeByteBuffer* const MasterBuff = Data->getMasterBuffer();
		const double time = MasterBuff->GetLong(location);
		const int CPID = MasterBuff->GetInt(location + Constants::SIZEOF_LONG);
		TimeCPID ToReturn(time, CPID);
		return ToReturn;
	}

	Long TraceDataByRankLocal::GetNumberOfRecords(Long start, Long end)
	{
		return (end - start) / SIZE_OF_TRACE_RECORD;
	}

	/*********************************************************************************************
	 * Removes unnecessary samples:
	 * i.e. if timeLine had three of the same cpid's in a row, the middle one would be superfluous,
	 * as we would know when painting that it should be the same color all the way through.
	 ********************************************************************************************/

	void TraceDataByRankLocal::PostProcess()
	{
		int len = ListCPID.size();
		for (int i = 0; i < len - 2; i++)
		{

			while (i < len - 1 && ListCPID[i].Timestamp == ListCPID[i + 1].Timestamp)
			{
				vector<TimeCPID>::iterator it = ListCPID.begin();
				it += (i + 1);
				ListCPID.erase(it);
				len--;
			}

		}
	}

	TraceDataByRankLocal::~TraceDataByRankLocal()
	{
		ListCPID.clear();
		//delete(Data);
	}

} /* namespace TraceviewerServer */
