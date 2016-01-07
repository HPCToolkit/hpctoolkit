// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/ProgressBar.cpp $
// $Id: ProgressBar.cpp 4291 2013-07-09 22:25:53Z felipet1326@gmail.com $
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
//   $HeadURL: https://hpctoolkit.googlecode.com/svn/branches/hpctoolkit-hpcserver/src/tool/hpcserver/ProgressBar.cpp $
//
// Purpose:
//   Shows progress for long-running tasks. In the single-threaded version, this
//   automatically picks the right width. Terminal width is not available from
//   MPI, so it assumes a default terminal width.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstdio>

#include "ProgressBar.hpp"
#include "DebugUtils.hpp"

namespace TraceviewerServer
{
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
	DEBUGCOUT(2) << "Usable width: " << usableWidth << endl;

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

	int percentage = (int)((100ULL*tasksComplete)/tasks);
	int newColsFilled = (percentage * usableWidth) / 100;
	if (colsFilled != newColsFilled){
		//Changed enough that we should do an update
		colsFilled = newColsFilled;

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
}
