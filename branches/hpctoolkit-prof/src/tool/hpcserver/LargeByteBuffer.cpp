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
//   Stores and handles the pages of the file to abstract away the file is not
//   stored entirely in one contiguous chunk of memory.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#include "ByteUtilities.hpp"
#include "LargeByteBuffer.hpp"
#include "Communication.hpp"
#include "Constants.hpp"
#include "FileUtils.hpp"
#include "DebugUtils.hpp"
#include "Server.hpp"
#include "VersatileMemoryPage.hpp"
#include <include/big-endian.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include <iostream>
#include <algorithm> //For min of two longs

#define MEG  (1024 * 1024)

#define DEFAULT_CHUNK_SIZE  (6 * 1024L * 1024L)
#define MIN_CHUNK_SIZE  (512 * 1024L)
#define MIN_MEM_LIMIT   (512 * 1024L * 1024L)

#define RECORD_SIZE  12

using namespace std;

namespace TraceviewerServer
{
	LargeByteBuffer::LargeByteBuffer(string sPath, int headerSize)
	{
		// chunk size
		chunkSize = DEFAULT_CHUNK_SIZE;
		if (optChunkSize > 0) {
		    	chunkSize = optChunkSize;
		}
		if (chunkSize < MIN_CHUNK_SIZE) {
		    	cerr << "warning: chunk size (" << chunkSize << ") "
			     << "is too small, using: " << MIN_CHUNK_SIZE << endl;
			chunkSize = MIN_CHUNK_SIZE;
		}
		// must be a multiple of trace record size (12 bytes)
		chunkSize = RECORD_SIZE * ((chunkSize + RECORD_SIZE - 1)/RECORD_SIZE);

		// memory size
		long memLimit = optMemSize;
		if (memLimit <= 0) {
		    	memLimit = getRamSize() / 2;
		}
		if (memLimit < MIN_MEM_LIMIT) {
		    	cerr << "warning: memory size (" << memLimit << ") "
			     << "is too small, using: " << MIN_MEM_LIMIT << endl;
			memLimit = MIN_MEM_LIMIT;
		}

		fileSize = FileUtils::getFileSize(sPath);
		numFilePages = (fileSize + chunkSize - 1)/chunkSize;

		long pageLimit = (memLimit + chunkSize - 1)/chunkSize;
		if (numFilePages < pageLimit) {
			pageLimit = numFilePages;
		}

		FileDescriptor fd = open(sPath.c_str(), O_RDONLY);
		info = new PageInfo(fd, pageLimit, chunkSize);

		FileOffset sizeRemaining = fileSize;
		for (long i = 0; i < numFilePages; i++)
		{
			FileOffset mapping_len = min( chunkSize, sizeRemaining);

			masterBuffer.push_back(
				VersatileMemoryPage(info, i, i * chunkSize,
						    mapping_len));
			sizeRemaining -= mapping_len;
		}

		if (Communication::rankLeader()) {
			printf("Mega Trace: %.1f Meg, Memory (per rank): %.1f Meg "
			       "(%ld/%ld pages)\n",
			       ((double) fileSize)/MEG, ((double) memLimit)/MEG,
			       pageLimit, numFilePages);
		}
	}

	int LargeByteBuffer::getInt(FileOffset pos)
	{
		int Page = pos / chunkSize;
		int loc = pos % chunkSize;
		char* p2D = masterBuffer[Page].get() + loc;

		return (int) be_to_host_32(*((uint32_t *) p2D));
	}

	Long LargeByteBuffer::getLong(FileOffset pos)
	{
		int Page = pos / chunkSize;
		int loc = pos % chunkSize;
		char* p2D = masterBuffer[Page].get() + loc;

		return (Long) be_to_host_64(*((uint64_t *) p2D));
	}

#if 0
	//Could very well be a template, but we only use it for uint64_t
	uint64_t LargeByteBuffer::lcm(uint64_t _a, uint64_t _b)
	{
		// LCM(a,b) = a*b/GCD(a,b) = (a/GCD(a,b))*b
		// [a/GCD(a,b) is an integer and a*b might overflow]

		//We need the original values for later, so use temporary ones for the GCD computation
		uint64_t a = _a, b = _b;
		uint64_t t = 0;
		while (b != 0)
		{
			t = b;
			b = a % b;
			a = t;
		}
		//GCD stored in a
		return (_a/a)*_b;
	}
#endif

	uint64_t LargeByteBuffer::getRamSize()
	{
#ifdef _SC_PHYS_PAGES
		long pages = sysconf(_SC_PHYS_PAGES);
		long page_size = sysconf(_SC_PAGE_SIZE);
		return pages * page_size;
#else
		int mib[2] = { CTL_HW, HW_MEMSIZE };
		u_int namelen = sizeof(mib) / sizeof(mib[0]);
		uint64_t ramSize;
		size_t len = sizeof(ramSize);

		if (sysctl(mib, namelen, &ramSize, &len, NULL, 0) < 0)
		{
			cerr << "Could not obtain system memory size"<<endl;
			throw ERROR_GET_RAM_SIZE_FAILED;
		}
		DEBUGCOUT(2) << "Memory size : "<<ramSize<<endl;

		return ramSize;
#endif
	}

	FileOffset LargeByteBuffer::size()
	{
		return fileSize;
	}

	LargeByteBuffer::~LargeByteBuffer()
	{
		masterBuffer.clear();
		delete info;
	}
}
