/*
 * LargeByteBuffer.cpp
 *
 *  Created on: Jul 10, 2012
 *      Author: pat2
 */
#include "LargeByteBuffer.h"
#include "sys/stat.h"
#include <fcntl.h>

#include <iostream>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/sysctl.h>

using namespace std;

namespace TraceviewerServer
{
	static ULong MMPAGE_SIZE; //= 1<<23;//1 << 30;
	ULong FileSize;
	LargeByteBuffer::LargeByteBuffer(string SPath, int HeaderSize)
	{
		//string SPath = Path.string();

		/*int MapFlags = MAP_PRIVATE;
		int MapProt = PROT_READ;*/

		struct stat Finfo;
		stat(SPath.c_str(), &Finfo);

		FileSize = Finfo.st_size;

		ULong OSPageSize = getpagesize();
		ULong PageSizeMultiple = lcm(OSPageSize, HeaderSize);//The page size must be a multiple of this

		ULong RamSizeInBytes = GetRamSize();

		const ULong _64_MEGABYTE = 1 << 26;
		//This is a pretty arbitrary algorithm, but it works
		MMPAGE_SIZE = PageSizeMultiple * (_64_MEGABYTE/OSPageSize);//This means it will get it close to 64 MB

		double MAX_PORTION_OF_RAM_AVAILABLE = 0.80;//Use up to 80%
		int MaxPages = (RamSizeInBytes * MAX_PORTION_OF_RAM_AVAILABLE)/MMPAGE_SIZE;
		VersatileMemoryPage::SetMaxPages(MaxPages);

//		if (PAGE_SIZE % mm::mapped_file::alignment() != 0)
//			cerr<< "PAGE_SIZE isn't a multiple of the OS granularity!!";
//		long FileSize = fs::file_size(Path);
		int FullPages = FileSize / MMPAGE_SIZE;
		int PartialPageSize = FileSize % MMPAGE_SIZE;
		NumPages = FullPages + (PartialPageSize == 0 ? 0 : 1);


		FileDescriptor fd = open(SPath.c_str(), O_RDONLY);

		ULong SizeRemaining = FileSize;


		//MasterBuffer = new mm::mapped_file*[NumPages];

		for (int i = 0; i < NumPages; i++)
		{
			unsigned long mapping_len = min((ULong) MMPAGE_SIZE, SizeRemaining);

			//MasterBuffer[i] = new mm::mapped_file(Path, mm::mapped_file::readonly, PAGE_SIZE, PAGE_SIZE*i);
			//This is done to make the Blue Gene Q easier

			MasterBuffer.push_back(*(new VersatileMemoryPage(MMPAGE_SIZE*i , mapping_len, i, fd)));

			//cout << "Allocated a page: " << AllocatedRegion << endl;
			SizeRemaining -= mapping_len;
			//cerr << "pid=" << getpid() << " i=" << i << " first test read=" << (int) *MasterBuffer[i] << endl;
			//cerr << "pid=" << getpid() << " i=" << i << " second test read=" << (int) *(MasterBuffer[i] + mapping_len-1) << endl;
		}

	}

	int LargeByteBuffer::GetInt(ULong pos)
	{
		int Page = pos / MMPAGE_SIZE;
		int loc = pos % MMPAGE_SIZE;
		char* p2D = MasterBuffer[Page].Get() + loc;
		int val = ByteUtilities::ReadInt(p2D);
		return val;
	}
	Long LargeByteBuffer::GetLong(ULong pos)
	{
		int Page = pos / MMPAGE_SIZE;
		int loc = pos % MMPAGE_SIZE;
		char* p2D = MasterBuffer[Page].Get() + loc;
		Long val = ByteUtilities::ReadLong(p2D);
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
	ULong LargeByteBuffer::GetRamSize()
	{
#ifdef _SC_PHYS_PAGES
		long pages = sysconf(_SC_PHYS_PAGES);
		long page_size = sysconf(_SC_PAGE_SIZE);
		return pages * page_size;
#else
		int mib[2] = { CTL_HW, HW_MEMSIZE };
		u_int namelen = sizeof(mib) / sizeof(mib[0]);
		uint64_t size;
		size_t len = sizeof(size);

		if (sysctl(mib, namelen, &size, &len, NULL, 0) < 0)
		{
			cerr << "Could not obtain system memory size"<<endl;
			throw 4456;
		}
		cout << "Memory size : "<<size<<endl;
		return size;
#endif

	}

	ULong LargeByteBuffer::Size()
	{
		return FileSize;
	}
	LargeByteBuffer::~LargeByteBuffer()
	{
		MasterBuffer.clear();

	}
}

