/*
 * FilterSet.hpp
 *
 *  Created on: Jun 21, 2013
 *      Author: pat2
 */

#ifndef FILTERSET_HPP_
#define FILTERSET_HPP_
#include "Filter.hpp"
#include <vector>

using namespace std;

class FilterSet{
	vector<Filter> filters;
	bool excludeMatched;
public:
	FilterSet(bool _excludeMatched){
		excludeMatched = _excludeMatched;
	}
	FilterSet(){
		excludeMatched = true;
	}
	void add(Filter toAdd){
		filters.push_back(toAdd);
	}
	bool matches(int proc, int thread) {
		bool matchedSoFar = true;
		vector<Filter>::iterator it;
		for(it = filters.begin(); it != filters.end(); ++it) {
			matchedSoFar &= (it->matches(proc, thread)^excludeMatched);
		}
		return matchedSoFar;
	}
};


#endif /* FILTERSET_HPP_ */
