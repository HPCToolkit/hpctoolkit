// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/ProcessTimeline.hpp $
// $Id: ProcessTimeline.hpp 4307 2013-07-18 17:04:52Z felipet1326@gmail.com $
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
//   $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/ProcessTimeline.hpp $
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef PROCESSTIMELINE_H_
#define PROCESSTIMELINE_H_
#include "TraceDataByRank.hpp"
#include "ImageTraceAttributes.hpp"
#include "TimeCPID.hpp" // for Time
namespace TraceviewerServer
{

	class ProcessTimeline
	{

	public:
		ProcessTimeline();
		ProcessTimeline(ImageTraceAttributes attrib, int _lineNum, FilteredBaseData* _dataTrace,
				Time _startingTime, int _headerSize);
		virtual ~ProcessTimeline();
		int line();
		void readInData();
		TraceDataByRank* data;
	private:
		int lineNumToProcessNum(int line);
		/** This ProcessTimeline's line number. */
		int lineNum;
		/** The initial time in view. */
		Time startingTime;
		/** The range of time in view. */
		Time timeRange;
		/** The amount of time that each pixel on the screen correlates to. */
		double pixelLength;
		ImageTraceAttributes attributes;

	};

} /* namespace TraceviewerServer */
#endif /* PROCESSTIMELINE_H_ */
