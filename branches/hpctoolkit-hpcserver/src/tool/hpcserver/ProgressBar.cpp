/*
 * ProgressBar.cpp
 *
 *  Created on: Jun 25, 2013
 *      Author: pat2
 */
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstdio>
#include <errno.h>
#include "ProgressBar.hpp"

ProgressBar::ProgressBar(string name, ulong tasksToComplete) {


	tasks = tasksToComplete;
	tasksComplete = 0;
	colsFilled = 0;
	label = name;
	struct winsize w;
	int ret = ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	if (ret == -1 || w.ws_col == 0)
		w.ws_col = DEFAULT_TERMINAL_WIDTH;
	usableWidth = w.ws_col - 10 - name.length();
	#if DEBUG > 2
	printf("Usable width: %d\n", usableWidth);
	#endif
	update();
}
void ProgressBar::incrementProgress(ulong tasks){
	tasksComplete += tasks;
	update();
}

void ProgressBar::incrementProgress(){
	tasksComplete++;
	update();
}

void ProgressBar::update() {
	if (colsFilled != (int)((tasksComplete*usableWidth)/tasks)){
		//Changed enough that we should do an update
		colsFilled = (tasksComplete*usableWidth)/tasks;

		int percentage = (int)((100ULL*tasksComplete)/tasks);
		putchar('\r');
		printf("%s: %3d%% [",label.c_str(), percentage);
		int i = 0;
		for (; i < colsFilled; i++) {
			putchar('=');
		}
		putchar('>');
		for (; i < usableWidth; i++) {
			putchar(' ');
		}
		putchar(']');
		fflush(stdout);
	}
}

ProgressBar::~ProgressBar() {
	putchar('\n');
}

