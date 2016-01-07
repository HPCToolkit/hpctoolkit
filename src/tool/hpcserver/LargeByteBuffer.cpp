// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/LargeByteBuffer.cpp $
// $Id: LargeByteBuffer.cpp 4380 2013-10-16 06:34:29Z felipet1326@gmail.com $
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
//   $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/LargeByteBuffer.cpp $
//
// Purpose:
//   Stores and handles the pages of the file to abstract away the file is not
//   stored entirely in one contiguous chunk of memory.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************
#include "LargeByteBuffer.hpp"
#include "Constants.hpp"
#include "FileUtils.hpp"
#include "DebugUtils.hpp"



#include <fcntl.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <errno.h>
#include <unistd.h>

#include <iostream>
#include <algorithm> //For min of two longs


using namespace std;

namespace TraceviewerServer
{
	static FileOffset mmPageSize; //= 1<<23;//1 << 30;
	FileOffset fileSize;
	LargeByteBuffer::LargeByteBuffer(string sPath, int headerSize)
	{
		//string SPath = Path.string();

		/*int MapFlags = MAP_PRIVATE;
		int MapProt = PROT_READ;*/

		fileSize = FileUtils::getFileSize(sPath);

		FileOffset osPageSize = getpagesize();
		FileOffset pageSizeMultiple = lcm(osPageSize, lcm(headerSize, SIZE_OF_TRACE_RECORD));//The page size must be a multiple of this

		FileOffset ramSizeInBytes = getRamSize();

		const FileOffset _64_MEGABYTE = 1 << 26;
		//This is a pretty arbitrary algorithm, but it works
		mmPageSize = pageSizeMultiple * (_64_MEGABYTE/osPageSize);//This means it will get it close to 64 MB

		//We should take into account how many copies of this program are
		//running on this node with something like MPI_COMM_WORLD, but I don't
		//want to introduce MPI-specific code here. It's not worth it... Plus, there's
		//a ton of paging stuff going on at the OS level that we don't really know
		//the specifics of, so the amount of RAM may be less important than it seems.
		double MAX_PORTION_OF_RAM_AVAILABLE = 0.60;//Use up to 60%
		int MaxPages = (int)(ramSizeInBytes * MAX_PORTION_OF_RAM_AVAILABLE/mmPageSize);
		VersatileMemoryPage::setMaxPages(MaxPages);


		int FullPages = fileSize / mmPageSize;
		int PartialPageSize = fileSize % mmPageSize;
		numPages = FullPages + (PartialPageSize == 0 ? 0 : 1);
		pageManagementList = new LRUList<VersatileMemoryPage>(numPages);

		FileDescriptor fd = open(sPath.c_str(), O_RDONLY);

		FileOffset sizeRemaining = fileSize;

		for (int i = 0; i < numPages; i++)
		{
			FileOffset mapping_len = min( mmPageSize, sizeRemaining);

			masterBuffer.push_back(VersatileMemoryPage(mmPageSize*i, mapping_len, fd, pageManagementList));

			sizeRemaining -= mapping_len;

		}

	}

	int LargeByteBuffer::getInt(FileOffset pos)
	{
		int Page = pos / mmPageSize;
		int loc = pos % mmPageSize;
		char* p2D = masterBuffer[Page].get() + loc;
		int val = ByteUtilities::readInt(p2D);
		return val;
	}
	Long LargeByteBuffer::getLong(FileOffset pos)
	{
		int Page = pos / mmPageSize;
		int loc = pos % mmPageSize;
		char* p2D = masterBuffer[Page].get() + loc;
		Long val = ByteUtilities::readLong(p2D);
		return val;

	}
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
		delete pageManagementList;

	}
}

