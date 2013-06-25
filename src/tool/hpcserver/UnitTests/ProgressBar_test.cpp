/*
 * ProgressBar_test.cpp
 *
 *  Created on: Jun 25, 2013
 *      Author: pat2
 */

#include "../ProgressBar.hpp"

#include <iostream>
using namespace std;

#ifdef UNIT_TESTS
int main (int argc, char** argv){


	ProgressBar bar("Work", 83);
	int q =0;
	for (int i = 0; i < 12; i++) {
		q++;
		bar.incrementProgress();
		usleep(100000*i);
	}
	for (int i = 0; i < 12; i++) {
		q+= i;
		bar.incrementProgress(i);
		usleep(100000*(12-i));
	}
	bar.incrementProgress(83-q);

}
#endif
