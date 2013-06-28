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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <list>
#include <errno.h>
#include <iostream>
#include "VersatileMemoryPage.hpp"

namespace TraceviewerServer
{
	static list<VersatileMemoryPage*> mostRecentlyUsed;
	static int MAX_PAGES_TO_ALLOCATE_AT_ONCE = 0;
	static int PagesCurrentlyAllocatedCount = 0;
	VersatileMemoryPage::VersatileMemoryPage(FileOffset _startPoint, int _size, int _index, FileDescriptor _file)
	{
		startPoint = _startPoint;
		size = _size;
		index = _index;
		file = _file;
		isMapped = false;
		if (MAX_PAGES_TO_ALLOCATE_AT_ONCE <1)
			cerr<<"Set max pages before creating any VersatileMemoryPages"<<endl;
	}

	void VersatileMemoryPage::setMaxPages(int pages)
	{
		MAX_PAGES_TO_ALLOCATE_AT_ONCE = pages;
	}

	VersatileMemoryPage::~VersatileMemoryPage()
	{
		if (isMapped)
			unmapPage();
	}
	char* VersatileMemoryPage::get()
	{
		putMeOnTop();

		if (!isMapped)
		{
			mapPage();
		}
		return page;
	}

	void VersatileMemoryPage::mapPage()
	{
		#if DEBUG > 1
		cout<< "Mapping page "<< index<< " "<<PagesCurrentlyAllocatedCount << " / " << MAX_PAGES_TO_ALLOCATE_AT_ONCE << endl;
		#endif
		if (isMapped)
		{
			cerr << "Trying to double map!"<<endl;
			return;
		}
		if (PagesCurrentlyAllocatedCount >= MAX_PAGES_TO_ALLOCATE_AT_ONCE)
		{

			VersatileMemoryPage* toRemove = mostRecentlyUsed.back();
#if DEBUG > 1
			cout<<"Kicking " << toRemove->index << " out"<<endl;
#endif

			if (toRemove->isMapped != true)
			{
				cerr << "Least recently used one isn't even mapped?"<<endl;
			}
			toRemove->unmapPage();
			mostRecentlyUsed.pop_back();
		}
		page = (char*)mmap(0, size, MAP_PROT, MAP_FLAGS, file, startPoint);
		if (page == MAP_FAILED)
		{
			cerr << "Mapping returned error " << strerror(errno) << endl;
			cerr << "off_t size =" << sizeof(off_t) << "mapping size=" << size << " MapProt=" <<MAP_PROT
					<< " MapFlags=" << MAP_FLAGS << " fd=" << file << " Start point=" << startPoint << endl;
			fflush(NULL);
			exit(-1);
		}
		PagesCurrentlyAllocatedCount++;
		isMapped = true;
	}
	void VersatileMemoryPage::unmapPage()
	{
		if (!isMapped)
		{
			cerr << "Trying to double unmap!"<<endl;
			return;
		}
		munmap(page, size);
		PagesCurrentlyAllocatedCount--;
		isMapped = false;
#if DEBUG > 1
		cout << "Unmapped a page"<<endl;
#endif
	}
	void VersatileMemoryPage::putMeOnTop()
	{

		if (mostRecentlyUsed.size() == 0)
		{
			mostRecentlyUsed.push_front(this);
			return;
		}
		if (mostRecentlyUsed.front()->index == index)//This is already the one on top, so we're done
			return;
		else
		{
			//Before we put this one on top, iterate through to see if it appears anywhere else
			list<VersatileMemoryPage*>::iterator it;
			for (it = mostRecentlyUsed.begin(); it != mostRecentlyUsed.end(); it++)
			{
				if ((*it)->index == index)//We've found this one. Take it out
				{
					mostRecentlyUsed.erase(it);
					break;
				}
			}
			mostRecentlyUsed.push_front(this);
		}

	}

} /* namespace TraceviewerServer */
