/*
 * VersatileMemoryPage.h
 *
 *  Created on: Aug 1, 2012
 *      Author: pat2
 */

#ifndef VERSATILEMEMORYPAGE_H_
#define VERSATILEMEMORYPAGE_H_


#include "sys/mman.h"
#include "ByteUtilities.hpp"


using namespace std;
namespace TraceviewerServer
{
	typedef int FileDescriptor;
	typedef uint64_t FileOffset;
	class VersatileMemoryPage
	{
	public:
		VersatileMemoryPage();
		VersatileMemoryPage(FileOffset, int, int, FileDescriptor);
		virtual ~VersatileMemoryPage();
		static void setMaxPages(int);
		char* get();
	private:
		void mapPage();
		void unmapPage();
		void putMeOnTop();
		FileOffset startPoint;
		int size;
		char* page;
		int index;
		FileDescriptor file;

		bool isMapped;

		static const int MAP_FLAGS = MAP_PRIVATE;
		static const int MAP_PROT = PROT_READ;
	};

} /* namespace TraceviewerServer */
#endif /* VERSATILEMEMORYPAGE_H_ */
