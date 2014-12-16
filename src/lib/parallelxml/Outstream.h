/*
 * Outstream.h
 *
 *  Created on: Dec 9, 2014
 *      Author: pat2
 */

#ifndef LIB_PARALLELXML_OUTSTREAM_H_
#define LIB_PARALLELXML_OUTSTREAM_H_

#if __cilk
// There appears to be a bug in Cilk. It's missing "inline" on line 241,
// which causes a multiple definition error
#include "reducer_ostream.h"
#endif
#include <ostream>

#line 18
// the reducer defines no copy constructors, etc. This wraps it and makes it more convenient.
class Outstream {
public:
	Outstream(const Outstream& copy_from)
		: backing(copy_from.backing), owner(false){
	}
	Outstream(std::ostream* outstream)
#if __cilk
	: owner(true)
	{
		backing = cilk::aligned_new<cilk::reducer_ostream>(*outstream);
	}

	template<typename T>
	std::ostream& operator<<(const T& obj)
	{
	  return **backing << obj;
	}
	~Outstream() {
		if (owner)
			cilk::aligned_delete(backing);
	}

private:
	cilk::reducer_ostream* backing;

#else
	: backing(outstream), owner(true) {}

	template<typename T>
	std::ostream& operator<<(const T& obj)
	{
	  return *backing << obj;
	}
private:
	std::ostream* backing;
#endif
	bool owner;
};





#endif /* LIB_PARALLELXML_OUTSTREAM_H_ */
