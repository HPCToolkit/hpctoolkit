/*
 * Atomics.hpp
 *
 *  Created on: Dec 15, 2014
 *      Author: pat2
 */
//TODO: Portability...
#ifndef LIB_PARALLELXML_ATOMICS_HPP_
#define LIB_PARALLELXML_ATOMICS_HPP_

template<typename T>
bool cas(T* location, T oldvalue, T newvalue) {
	return __sync_bool_compare_and_swap(location, oldvalue, newvalue);
}

template<typename T>
T faa(T* location, T increment) {
	return __sync_fetch_and_add (location, increment);
}

template<typename T>
T fai(T* location) {
	return __sync_fetch_and_add (location, 1);
}



#endif /* LIB_PARALLELXML_ATOMICS_HPP_ */
