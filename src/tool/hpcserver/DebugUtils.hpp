/*
 * DebugUtils.hpp
 *
 *  Created on: Jul 2, 2013
 *      Author: pat2
 */

#ifndef DEBUGUTILS_HPP_
#define DEBUGUTILS_HPP_

#include <iostream>
using namespace std;

#ifndef DEBUG
#define DEBUG 0
#endif

#define DEBUGCOUT(a) if (DEBUG > (a))\
						cout


#endif /* DEBUGUTILS_HPP_ */
