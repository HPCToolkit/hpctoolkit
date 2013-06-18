/*
 * VersatileMemoryPage.cpp
 *
 *  Created on: Aug 1, 2012
 *      Author: pat2
 */

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
		if ((isMapped != 0)&& (isMapped!=1))
			cout<<"BadIsmapped:"<<isMapped<<endl;

		putMeOnTop();

		if (!isMapped)
		{
			mapPage();
		}
		return page;
	}

	void VersatileMemoryPage::mapPage()
	{
		cout<< "Mapping page "<< index<< " "<<PagesCurrentlyAllocatedCount << " / " << MAX_PAGES_TO_ALLOCATE_AT_ONCE << endl;
		if (isMapped)
		{
			cerr << "Trying to double map!"<<endl;
			return;
		}
		if (PagesCurrentlyAllocatedCount >= MAX_PAGES_TO_ALLOCATE_AT_ONCE)
		{

			VersatileMemoryPage* toRemove = mostRecentlyUsed.back();
			cout<<"Kicking " << toRemove->index << " out"<<endl;

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
		cout << "Unmapped a page"<<endl;
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
