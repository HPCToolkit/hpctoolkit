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

#define DEFAULT_PAGE_SIZE  4096
#define EXTRA_SIZE   32

using namespace std;

namespace TraceviewerServer
{
	PageInfo::PageInfo(FileDescriptor _fd, FileOffset _fileSize,
			   long _numPages, long _chunkSize)
	{
		fd = _fd;
		fileSize = _fileSize;
		numPages = _numPages;
		nextIndex = 0;
		chunkSize = _chunkSize;

		long page_size = DEFAULT_PAGE_SIZE;
#ifdef _SC_PAGESIZE
		page_size = sysconf(_SC_PAGESIZE);
#endif
		if (page_size < DEFAULT_PAGE_SIZE) {
			page_size = DEFAULT_PAGE_SIZE;
		}
		map_size = chunkSize + EXTRA_SIZE;
		map_size = page_size * ((map_size + page_size - 1)/page_size);

		pageVec.resize(numPages);
		for (long i = 0; i < numPages; i++) {
			pageVec[i] = NULL;
		}
	}

	VersatileMemoryPage::VersatileMemoryPage(PageInfo * _info, long _index,
						 FileOffset _offset, ssize_t _size)
	{
		info = _info;
		index = _index;
		offset = _offset;
		size = _size;
		page = NULL;
		isMapped = false;
	}

	VersatileMemoryPage::~VersatileMemoryPage()
	{
		if (isMapped) {
			unmapPage();
		}
	}

	char* VersatileMemoryPage::get()
	{
		if (! isMapped) {
			mapPage();
		}
		return page;
	}

	void VersatileMemoryPage::mapPage()
	{
		DEBUGCOUT(1) << "map page: " << index << "  size: 0x" << hex
			     << size << "  offset: 0x" << offset << dec << endl;

		if (isMapped && page != NULL) {
			cerr << "warning: trying to map page " << index
			     << " that is already mapped" << endl;
			return;
		}
		isMapped = false;

		// A simple page replacement strategy: replace the
		// page at nextIndex with this page.  This gives LRU
		// based on the map time.

		long pos = info->nextIndex;
		info->nextIndex = (pos + 1) % info->numPages;

		VersatileMemoryPage *old_page = info->pageVec[pos];

		if (old_page != NULL) {
			old_page->unmapPage();
		}
		info->pageVec[pos] = this;

		// mmap anon space for buffer
#ifdef MAP_ANONYMOUS
		int map_flags = MAP_PRIVATE | MAP_ANONYMOUS;
#else
		int map_flags = MAP_PRIVATE | MAP_ANON;
#endif
		page = (char *) mmap(NULL, info->map_size, PROT_READ | PROT_WRITE,
				     map_flags, -1, 0);
		if (page == MAP_FAILED) {
			cerr << "mmap versatile memory page failed: "
			     << strerror(errno) << endl;
			exit(1);
		}

		// extend each segment by a small amount (32 bytes) to
		// simplify handling a trace record that crosses a
		// segment boundary.

		ssize_t read_size = size + EXTRA_SIZE;
		if (offset + read_size > info->fileSize) {
			read_size = info->fileSize - offset;
		}
		if (read_size > info->map_size) {
			cerr << "warning: page " << index
			     << " segment size 0x" << hex << read_size
			     << " exceeds map size 0x" << info->map_size
			     << dec << endl;
		}

		// read segment from file and handle short reads
		ssize_t len = 0;
		while (len < read_size) {
			ssize_t ret = pread(info->fd, &page[len], read_size - len, offset + len);
			if (ret > 0) {
			    	len += ret;
			}
			else if (errno != EINTR) {
			    	cerr << "read trace file failed: " << strerror(errno) << endl;
				exit(1);
			}
		}
		isMapped = true;
	}

	void VersatileMemoryPage::unmapPage()
	{
		DEBUGCOUT(1) << "unmap page: " << index << "  size: 0x" << hex
			     << size << "  offset: 0x" << offset << dec << endl;

		if (! isMapped || page == NULL) {
			cerr << "warning: trying to unmap page " << index
			     << " that is not mapped" << endl;
			return;
		}
		munmap(page, info->map_size);

		page = NULL;
		isMapped = false;
	}

} /* namespace TraceviewerServer */
