/*
 * VersatileMemoryPage.h
 *
 *  Created on: Aug 1, 2012
 *      Author: pat2
 */

#ifndef VERSATILEMEMORYPAGE_H_
#define VERSATILEMEMORYPAGE_H_


#include "sys/mman.h"
#include "ByteUtilities.h"


using namespace std;
namespace TraceviewerServer
{
	typedef int FileDescriptor;
	class VersatileMemoryPage
	{
	public:
		VersatileMemoryPage();
		VersatileMemoryPage(ULong, int, int, FileDescriptor);
		virtual ~VersatileMemoryPage();
		static void SetMaxPages(int);
		char* Get();
	private:
		void MapPage();
		void UnmapPage();
		void PutMeOnTop();
		ULong StartPoint;
		int Size;
		char* Page;
		int Index;
		FileDescriptor File;

		bool IsMapped;

		static const int MAP_FLAGS = MAP_PRIVATE;
		static const int MAP_PROT = PROT_READ;
	};

} /* namespace TraceviewerServer */
#endif /* VERSATILEMEMORYPAGE_H_ */
