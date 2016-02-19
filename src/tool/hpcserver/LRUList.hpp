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
//   $HeadURL$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef LRULIST_H_
#define LRULIST_H_

#include <vector>
#include <list>


using std::list;
using std::vector;

namespace TraceviewerServer {



template <typename T>
class LRUList {
private:
	int totalPages;
	int usedPages;
	int currentFront;
	list<T*> useOrder;
	list<T*> removed;
	 vector<typename list<T*>::iterator> iters;
public:
	/**
	 * It's a special data structure that uses a little extra memory in exchange
	 * for all operations running in constant time. Objects are added to the
	 * data structure and given an index that identifies them.
	 * Once added, objects are not deleted; they are either on the "in use" list,
	 * which for VersatileMemoryPage corresponds to being mapped, or they are on
	 * the "not in use" list, which corresponds to not being mapped. An object
	 * must only be added with addNew() once. If it is removed from the list
	 * with remove(), it must be added with reAdd(). It is not necessary to call
	 * putOnTop() after adding an element as it will already be on top.
	 */
	LRUList(int expectedMaxSize)//Up to linear time
	{
		iters.reserve(expectedMaxSize);
		totalPages = 0;
		usedPages = 0;
		currentFront = -1;
	}
	int addNew(T* toAdd)//constant time
	{
		useOrder.push_front(toAdd);
		int index = totalPages++;
		currentFront = index;
		iters.push_back(useOrder.begin());
		usedPages++;
		return index;
	}
	int addNewUnused(T* toAdd)//constant time
	{
		removed.push_front(toAdd);
		int index = totalPages++;
		iters.push_back(removed.begin());
		return index;
	}
	void putOnTop(int index)//Constant time
	{
		if (index == currentFront) return;
		typename list<T*>::iterator it;
		it = iters[index];
		useOrder.splice(useOrder.begin(), useOrder, it);
		currentFront = index;
	}
	T* getLast()
	{
		return useOrder.back();
	}

	void removeLast()//Constant time
	{
		removed.splice(removed.end(), useOrder, --useOrder.end());
		usedPages--;
	}
	void reAdd(int index)//Constant time
	{
		typename list<T*>::iterator it = iters[index];
		useOrder.splice(useOrder.begin(), removed, it);
		currentFront = index;
		usedPages++;
	}
	int getTotalPageCount()//Constant time
	{
		return totalPages;
	}
	int getUsedPageCount()
	{
		return usedPages;
	}
	virtual ~LRUList()
	{

	}

	/*int dump()
		{
			int x = 0;
			puts("Objects \"in use\" from most recently used to least recently used");
			typename list<T*>::iterator it = useOrder.begin();
			for(; it != useOrder.end(); ++it){
				printf("%d\n", ((TestData*)*it)->index);
				x++;
			}
			puts("Objects \"not in use\" from most recently used to least recently used");
			it = removed.begin();
			for(; it != removed.end(); ++it){
				printf("%d\n", ((TestData*)*it)->index);
			}
			cout << "Used count: " << usedPages <<" supposed to be " << x<<endl;
			return x;
		}*/
};

} /* namespace TraceviewerServer */
#endif /* LRULIST_H_ */
