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
// Copyright ((c)) 2002-2015, Rice University
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
//   Manages pages of memory that are mapped and unmapped dynamically from the
//   trace file based on the last time they were used.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#include <sys/types.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cstring>
#include <list>

#include "DebugUtils.hpp"
#include "VersatileMemoryPage.hpp"
#include "LRUList.hpp"

#define DEFAULT_PAGE_SIZE  4096

using namespace std;

namespace TraceviewerServer
{
	static int MAX_PAGES_TO_ALLOCATE_AT_ONCE = 0;

	VersatileMemoryPage::VersatileMemoryPage(FileOffset _startPoint, ssize_t _size,
						 FileDescriptor _file,
						 LRUList<VersatileMemoryPage>* pageManagementList)
	{
		startPoint = _startPoint;
		size = _size;
		page_size = DEFAULT_PAGE_SIZE;
#ifdef _SC_PAGESIZE
		page_size = sysconf(_SC_PAGESIZE);
#endif
		if (page_size < DEFAULT_PAGE_SIZE) {
			page_size = DEFAULT_PAGE_SIZE;
		}
		map_size = page_size * ((size + page_size - 1)/page_size);

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

		// mmap anon space for buffer
#ifdef MAP_ANONYMOUS
		int map_flags = MAP_PRIVATE | MAP_ANONYMOUS;
#else
		int map_flags = MAP_PRIVATE | MAP_ANON;
#endif
		page = (char *) mmap(NULL, map_size, PROT_READ | PROT_WRITE, map_flags, -1, 0);
		if (page == MAP_FAILED) {
			cerr << "mmap versatile memory page failed: "
			     << strerror(errno) << endl;
			exit(1);
		}

		// read segment from file and handle short reads
		ssize_t len = 0;
		while (len < size) {
			ssize_t ret = pread(file, &page[len], size - len, startPoint + len);
			if (ret > 0) {
			    	len += ret;
			}
			else if (errno != EINTR) {
			    	cerr << "read trace file failed: " << strerror(errno) << endl;
				exit(1);
			}
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
		munmap(page, map_size);

		isMapped = false;

		DEBUGCOUT(1) << "Unmapped a page"<<endl;

	}

} /* namespace TraceviewerServer */
