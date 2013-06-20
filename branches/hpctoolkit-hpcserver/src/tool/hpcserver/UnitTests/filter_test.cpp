/*
 * filter_test.cpp
 *
 *  Created on: Jun 20, 2013
 *      Author: pat2
 */

#include "../Filter.hpp"
#include <iostream>
using namespace std;
#ifdef UNIT_TESTS
int main (int argc, char** argv){
	Filter f(Range(1,5,1), Range(0,3,1), false);

	bool b = f.include(1,2);

	cout << "Include (1,2) " << (b ? "true" : "false") <<endl;
}
#endif

