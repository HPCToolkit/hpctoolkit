// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2013, Rice University
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
//   $HeadURL$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************


#undef NDEBUG

#include <iostream>
#include <cassert>
using namespace std;
struct TestData
{
	int index;
	bool expensiveObjectInUse;
};

#include "../LRUList.hpp"

using TraceviewerServer::LRUList;

void lruTest()
{
	#define OBJCOUNT 20
	LRUList<TestData> list(OBJCOUNT);

	bool inuse[OBJCOUNT];//Just for testing
	for(int i = 0; i< OBJCOUNT; i++)
	{
		TestData* next = new TestData;
		next->expensiveObjectInUse = true;
		next->index = list.addNew(next);
		inuse[next->index] = true;
	}
	assert(list.getSize()==OBJCOUNT);

	srand(12);
	for(int i = 0; i< 70; i++)
	{
		list.putOnTop(rand()%OBJCOUNT);
	}
	assert(list.getSize()==OBJCOUNT);
	int count = OBJCOUNT;
	for(int i = 0; i< 12; i++)
	{
		TestData* last = list.getLast();
		last->expensiveObjectInUse = false;
		inuse[last->index] = false;
		list.removeLast();
		assert(list.getSize() == --count);

		int ran;
		if(inuse[ran = (rand()%OBJCOUNT)])
			list.putOnTop(ran);
		if(!inuse[ran = (rand()%OBJCOUNT)])
		{
			inuse[ran] = true;
			list.reAdd(ran);
			assert(list.getSize() == ++count);
		}
	}
	//list.dump();

	while (list.getSize() > 0)
	{
		list.removeLast();
		//list.dump();
	}
	assert(list.getSize()==0);
	for(int i = 0; i< OBJCOUNT; i++)
	{
		list.reAdd(i);
	}
	assert(list.getSize()==OBJCOUNT);
	cout << "List operations did not crash and were probably successful"<<endl;
}

