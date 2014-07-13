#include <map>

#include <string>
#include <iostream>
#include <string.h>

// #include "realpath.h"

class FilenameCompare {
public:
  bool operator()(const char *n1,  const char *n2) const {
    return strcmp(n1,n2) < 0;
  }
};

// ---------------------------------------------------
// map for interning character strings for file names
static std::map<const char *, std::string *, FilenameCompare> fileNameMap; 

static const char *RealPath(const char *name)
{
	static char buffer[10000];
	return strcpy(buffer, name);
}

std::string &getRealPath(const char *name)
{
	std::string *&thename = fileNameMap[name];
        if (thename == NULL) {
		//  the filename is not in the list. Add it.
		thename  = new std::string(RealPath(name));
	} 
 	return *thename;
}

void test() 
{
	std::string &thea1 = getRealPath("a");
	std::string &thea2 = getRealPath("a");
	std::string &theb = getRealPath("b");
	std::cout 
		<< "a1=" << &thea1 << std::endl 
		<< "a2=" << &thea2 << std::endl 
		<< "b=" << &theb << std::endl
		<< "size=" << fileNameMap.size() << std::endl;
	
}

int main(int argc, const char **argv)
{
 	test();
 	test();
	return 0;
}
