/*
 * ProgressBar.hpp
 *
 *  Created on: Jun 25, 2013
 *      Author: pat2
 */

#ifndef PROGRESSBAR_HPP_
#define PROGRESSBAR_HPP_

#include <string>
using namespace std;

class ProgressBar {
private:
typedef uint64_t ulong;

	static const int DEFAULT_TERMINAL_WIDTH = 80;

	int usableWidth;
	int colsFilled;
	ulong tasks;
	int tasksComplete;
	string label;
	void update();
public:
	ProgressBar(string name, ulong tasksToComplete);

	void incrementProgress(ulong tasks);
	/**Call every time a task is completed */
	void incrementProgress();

	virtual ~ProgressBar();
};

#endif /* PROGRESSBAR_HPP_ */
