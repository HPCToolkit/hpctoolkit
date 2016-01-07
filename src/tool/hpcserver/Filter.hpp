// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/Filter.hpp $
// $Id: Filter.hpp 4286 2013-07-09 19:03:59Z felipet1326@gmail.com $
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
//   $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/Filter.hpp $
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************
/*
 * Filter.hpp
 *
 *  Created on: Jun 20, 2013
 *      Author: pat2
 */

#ifndef FILTER_HPP_
#define FILTER_HPP_
namespace TraceviewerServer
{
class Range {
		int min;
		int max;
		int stride;
	public:
		Range(){//Default range matches nothing
			min = 0;
			max = 0;
			stride = 1;
		}
		bool matches (int i){
			if (i < min) return false;
			if (i > max) return false;
			return ((i-min)%stride)==0;
		}
		Range(int _min, int _max, int _stride){
			min = _min;
			max = _max;
			stride = _stride;
		}
};

struct BinaryRepresentationOfFilter{
	int processMin;
	int processMax;
	int processStride;
	int threadMin;
	int threadMax;
	int threadStride;
};

class Filter {
public:
	Filter() {
	}
	Filter(Range _process, Range _thread){
		process = _process;
		thread = _thread;
	}
	Filter (BinaryRepresentationOfFilter tocopy) {
		process = Range(tocopy.processMin, tocopy.processMax, tocopy.processStride);
		thread = Range(tocopy.threadMin, tocopy.threadMax, tocopy.threadStride);
	}
	bool matches (int processNum, int threadNum){
		return (process.matches(processNum) && thread.matches(threadNum));
	}
private:
	Range process;
	Range thread;
} ;
}

#endif /* FILTER_HPP_ */
