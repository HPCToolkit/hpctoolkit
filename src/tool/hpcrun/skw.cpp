// SKW.CPP
// small test file for experiment with C++ in hpcrun

#include <string>

static std::string s;

extern "C"
void skw_test(void)
{
  s += "woof";
}


