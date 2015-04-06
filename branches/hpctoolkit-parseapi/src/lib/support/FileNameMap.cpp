//------------------------------------------------------------------------------
// global includes
//------------------------------------------------------------------------------

#include <map>
#include <string>
#include <string.h>



//------------------------------------------------------------------------------
// local includes
//------------------------------------------------------------------------------

#include "realpath.h"



//------------------------------------------------------------------------------
// types
//------------------------------------------------------------------------------

class FilenameCompare {
public:
  bool operator()(const char *n1,  const char *n2) const {
    return strcmp(n1,n2) < 0;
  }
};



//------------------------------------------------------------------------------
// private data
//------------------------------------------------------------------------------

// ---------------------------------------------------
// map for interning character strings for file names
// ---------------------------------------------------

static std::map<const char *, std::string *, FilenameCompare> fileNameMap; 



//------------------------------------------------------------------------------
// interface functions
//------------------------------------------------------------------------------
std::string &getRealPath(const char *name)
{
	std::string *&thename = fileNameMap[name];
        if (thename == NULL) {
		//  the filename is not in the list. Add it.
		thename  = new std::string(RealPath(name));
	} 
 	return *thename;
}
