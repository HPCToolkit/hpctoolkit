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

struct BinaryRepresentationOfFilter{
	int processMin;
	int processMax;
	int processStride;
	int threadMin;
	int threadMax;
	int threadStride;
};

class Filter {
public:
	Filter() {
	}
	Filter(Range _process, Range _thread){
		process = _process;
		thread = _thread;
	}
	Filter (BinaryRepresentationOfFilter tocopy) {
		process = Range(tocopy.processMin, tocopy.processMax, tocopy.processStride);
		thread = Range(tocopy.threadMin, tocopy.threadMax, tocopy.threadStride);
	}
	bool matches (int processNum, int threadNum){
		return (process.matches(processNum) && thread.matches(threadNum));
	}
private:
	Range process;
	Range thread;
} ;


#endif /* FILTER_HPP_ */
