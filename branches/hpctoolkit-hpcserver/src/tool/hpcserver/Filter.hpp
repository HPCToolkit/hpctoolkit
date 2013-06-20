/*
 * Filter.hpp
 *
 *  Created on: Jun 20, 2013
 *      Author: pat2
 */

#ifndef FILTER_HPP_
#define FILTER_HPP_
#include <climits>

class Range {
		int min;
		int max;
		int stride;
	public:
		Range(){//Default range matches nothing
			min = 0;
			max = 0;
			stride = 1;
		}
		bool matches (int i){
			if (i < min) return false;
			if (i > max) return false;
			return ((i-min)%stride)==0;
		}
		Range(int _min, int _max, int _stride){
			min = _min;
			max = _max;
			stride = _stride;
		}
};
class Filter {
public:
	Filter() {
		excludeMatched = true;
	}
	Filter(Range _process, Range _thread, bool _excludeMatched){
		process = _process;
		thread = _thread;
		excludeMatched = _excludeMatched;
	}
	bool include (int processNum, int threadNum){
		return (process.matches(processNum) && thread.matches(threadNum))^excludeMatched;
		//xoring them is the logical operation we want
	}
private:
	Range process;
	Range thread;
	bool excludeMatched;//if excludeMatched, any ranks that _do_not_ match the pattern are included
} ;

#endif /* FILTER_HPP_ */
