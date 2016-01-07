// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/VersatileMemoryPage.cpp $
// $Id: VersatileMemoryPage.cpp 4380 2013-10-16 06:34:29Z felipet1326@gmail.com $
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
//   $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/VersatileMemoryPage.cpp $
//
// Purpose:
//   Manages pages of memory that are mapped and unmapped dynamically from the
//   trace file based on the last time they were used.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <list>
#include <errno.h>

#include "DebugUtils.hpp"
#include "VersatileMemoryPage.hpp"
#include "LRUList.hpp"

namespace TraceviewerServer
{
	static int MAX_PAGES_TO_ALLOCATE_AT_ONCE = 0;

	VersatileMemoryPage::VersatileMemoryPage(FileOffset _startPoint, int _size, FileDescriptor _file, LRUList<VersatileMemoryPage>* pageManagementList)
	{
		startPoint = _startPoint;
		size = _size;
		mostRecentlyUsed = pageManagementList;
		index = mostRecentlyUsed->addNewUnused(this);
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

		if (!isMapped)
		{
			mapPage();
		}
		mostRecentlyUsed->putOnTop(index);

		return page;
	}

	void VersatileMemoryPage::mapPage()
	{

		DEBUGCOUT(1) << "Mapping page "<< index<< " "<<mostRecentlyUsed->getUsedPageCount() << " / " << MAX_PAGES_TO_ALLOCATE_AT_ONCE << endl;

		if (isMapped)
		{
			cerr << "Trying to double map!"<<endl;
			return;
		}
		if (mostRecentlyUsed->getUsedPageCount() >= MAX_PAGES_TO_ALLOCATE_AT_ONCE)
		{

			VersatileMemoryPage* toRemove = mostRecentlyUsed->getLast();

			DEBUGCOUT(1)<<"Kicking " << toRemove->index << " out"<<endl;

			if (toRemove->isMapped != true)
				cerr << "Least recently used one isn't even mapped?"<<endl;

			toRemove->unmapPage();
			mostRecentlyUsed->removeLast();
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


		isMapped = true;
		mostRecentlyUsed->reAdd(index);
	}
	void VersatileMemoryPage::unmapPage()
	{
		if (!isMapped)
		{
			cerr << "Trying to double unmap!"<<endl;
			return;
		}
		munmap(page, size);

		isMapped = false;

		DEBUGCOUT(1) << "Unmapped a page"<<endl;

	}

} /* namespace TraceviewerServer */
