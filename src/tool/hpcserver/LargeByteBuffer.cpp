/*
 * LargeByteBuffer.cpp
 *
 *  Created on: Jul 10, 2012
 *      Author: pat2
 */
#include "LargeByteBuffer.hpp"
#include "Constants.hpp"
#include "sys/stat.h"
#include <fcntl.h>

#include <iostream>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#ifdef USE_MPI
#include "mpi.h"
#endif

using namespace std;

namespace TraceviewerServer
{
	static ULong mmPageSize; //= 1<<23;//1 << 30;
	FileOffset fileSize;
	LargeByteBuffer::LargeByteBuffer(string SPath, int HeaderSize)
	{
		//string SPath = Path.string();

		/*int MapFlags = MAP_PRIVATE;
		int MapProt = PROT_READ;*/

		struct stat fInfo;
		stat(SPath.c_str(), &fInfo);

		fileSize = fInfo.st_size;

		ULong osPageSize = getpagesize();
		ULong pageSizeMultiple = lcm(osPageSize, HeaderSize);//The page size must be a multiple of this

		ULong ramSizeInBytes = getRamSize();

		const ULong _64_MEGABYTE = 1 << 26;
		//This is a pretty arbitrary algorithm, but it works
		mmPageSize = pageSizeMultiple * (_64_MEGABYTE/osPageSize);//This means it will get it close to 64 MB

		double MAX_PORTION_OF_RAM_AVAILABLE = 0.80;//Use up to 80%
		int MaxPages = (int)(ramSizeInBytes * MAX_PORTION_OF_RAM_AVAILABLE/mmPageSize);
		VersatileMemoryPage::setMaxPages(MaxPages);

//		if (PAGE_SIZE % mm::mapped_file::alignment() != 0)
//			cerr<< "PAGE_SIZE isn't a multiple of the OS granularity!!";
//		long FileSize = fs::file_size(Path);
		int FullPages = fileSize / mmPageSize;
		int PartialPageSize = fileSize % mmPageSize;
		numPages = FullPages + (PartialPageSize == 0 ? 0 : 1);


		FileDescriptor fd = open(SPath.c_str(), O_RDONLY);

		FileOffset sizeRemaining = fileSize;


		//MasterBuffer = new mm::mapped_file*[NumPages];

		for (int i = 0; i < numPages; i++)
		{
			unsigned long mapping_len = min((ULong) mmPageSize, sizeRemaining);

			//MasterBuffer[i] = new mm::mapped_file(Path, mm::mapped_file::readonly, PAGE_SIZE, PAGE_SIZE*i);
			//This is done to make the Blue Gene Q easier

			masterBuffer.push_back(*(new VersatileMemoryPage(mmPageSize*i , mapping_len, i, fd)));

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
	ULong LargeByteBuffer::lcm(ULong _a, ULong _b)
	{
		//LCM(a,b) = a*b/GCD(a,b) = (a/GCD(a,b))*b
		//From Wikipedia
		/*function gcd(a, b)
		    while b ­ 0
		       t := b
		       b := a mod b
		       a := t
		    return a*/
		//We need the original values for later, so use temporary ones for the GCD computation
		ULong a = _a, b = _b;
		ULong t = 0;
		while (b != 0)
		{
			t = b;
			b = a % b;
			a = t;
		}
		//GCD stored in a
		return (_a/a)*_b;
	}
	ULong LargeByteBuffer::getRamSize()
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
		#if DEBUG > 2
		cout << "Memory size : "<<ramSize<<endl;
		#endif
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

	}
}

