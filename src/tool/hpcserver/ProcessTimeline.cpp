// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/ProcessTimeline.cpp $
// $Id: ProcessTimeline.cpp 4283 2013-07-02 20:13:13Z felipet1326@gmail.com $
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
//   $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/ProcessTimeline.cpp $
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#include "ProcessTimeline.hpp"

namespace TraceviewerServer
{

	ProcessTimeline::ProcessTimeline(ImageTraceAttributes attrib, int _lineNum, FilteredBaseData* _dataTrace,
			Time _startingTime, int _headerSize)
	{
		lineNum = _lineNum;

		timeRange = (attrib.endTime - attrib.begTime) ;
		startingTime = _startingTime;

		pixelLength = timeRange / (double) attrib.numPixelsH;

		attributes = attrib;
		data = new TraceDataByRank(_dataTrace, lineNumToProcessNum(_lineNum), attrib.numPixelsH, _headerSize);
	}
	int ProcessTimeline::lineNumToProcessNum(int line) {
		int numTimelinesToPaint = attributes.endProcess - attributes.begProcess;
		if (numTimelinesToPaint > attributes.numPixelsV)
			return attributes.begProcess
					+ (line * numTimelinesToPaint) / (attributes.numPixelsV);
		else
			return attributes.begProcess + line;
	}
	void ProcessTimeline::readInData()
	{
		data->getData(startingTime, timeRange, pixelLength);
	}

	int ProcessTimeline::line()
	{
		return lineNum;
	}

	ProcessTimeline::~ProcessTimeline()
	{
		delete data;
	}

} /* namespace TraceviewerServer */
