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
#include "VersatileMemoryPage.h"

namespace TraceviewerServer
{
	static list<VersatileMemoryPage*> MostRecentlyUsed;
	static int MAX_PAGES_TO_ALLOCATE_AT_ONCE = 0;
	static int PagesCurrentlyAllocatedCount = 0;
	VersatileMemoryPage::VersatileMemoryPage(ULong startPoint, int size, int index, FileDescriptor file)
	{
		StartPoint = startPoint;
		Size = size;
		Index = index;
		File = file;
		IsMapped = false;
		if (MAX_PAGES_TO_ALLOCATE_AT_ONCE <1)
			cerr<<"Set max pages before creating any VersatileMemoryPages"<<endl;
	}

	void VersatileMemoryPage::SetMaxPages(int pages)
	{
		MAX_PAGES_TO_ALLOCATE_AT_ONCE = pages;
	}

	VersatileMemoryPage::~VersatileMemoryPage()
	{
		if (IsMapped)
			UnmapPage();
	}
	char* VersatileMemoryPage::Get()
	{
		if ((IsMapped != 0)&& (IsMapped!=1))
			cout<<"BadIsmapped:"<<IsMapped<<endl;

		PutMeOnTop();

		if (!IsMapped)
		{
			MapPage();
		}
		return Page;
	}

	void VersatileMemoryPage::MapPage()
	{
		cout<< "Mapping page "<< Index<< " "<<PagesCurrentlyAllocatedCount << " / " << MAX_PAGES_TO_ALLOCATE_AT_ONCE << endl;
		if (IsMapped)
		{
			cerr << "Trying to double map!"<<endl;
			return;
		}
		if (PagesCurrentlyAllocatedCount >= MAX_PAGES_TO_ALLOCATE_AT_ONCE)
		{

			VersatileMemoryPage* toRemove = MostRecentlyUsed.back();
			cout<<"Kicking " << toRemove->Index << " out"<<endl;

			if (toRemove->IsMapped != true)
			{
				cerr << "Least recently used one isn't even mapped?"<<endl;
			}
			toRemove->UnmapPage();
			MostRecentlyUsed.pop_back();
		}
		Page = (char*)mmap(0, Size, MAP_PROT, MAP_FLAGS, File, StartPoint);
		if (Page == MAP_FAILED)
		{
			cerr << "Mapping returned error " << strerror(errno) << endl;
			cerr << "off_t size =" << sizeof(off_t) << "mapping size=" << Size << " MapProt=" <<MAP_PROT
					<< " MapFlags=" << MAP_FLAGS << " fd=" << File << " Start point=" << StartPoint << endl;
			fflush(NULL);
			exit(-1);
		}
		PagesCurrentlyAllocatedCount++;
		IsMapped = true;
	}
	void VersatileMemoryPage::UnmapPage()
	{
		if (!IsMapped)
		{
			cerr << "Trying to double unmap!"<<endl;
			return;
		}
		munmap(Page, Size);
		PagesCurrentlyAllocatedCount--;
		IsMapped = false;
		cout << "Unmapped a page"<<endl;
	}
	void VersatileMemoryPage::PutMeOnTop()
	{

		if (MostRecentlyUsed.size() == 0)
		{
			MostRecentlyUsed.push_front(this);
			return;
		}
		if (MostRecentlyUsed.front()->Index == Index)//This is already the one on top, so we're done
			return;
		else
		{
			//Before we put this one on top, iterate through to see if it appears anywhere else
			list<VersatileMemoryPage*>::iterator it;
			for (it = MostRecentlyUsed.begin(); it != MostRecentlyUsed.end(); it++)
			{
				if ((*it)->Index == Index)//We've found this one. Take it out
				{
					MostRecentlyUsed.erase(it);
					break;
				}
			}
			MostRecentlyUsed.push_front(this);
		}

	}

} /* namespace TraceviewerServer */
