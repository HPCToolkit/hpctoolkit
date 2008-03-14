#include <string>

using namespace std;

void add_function_entry(void *address, const string *comment);
void add_stripped_function_entry(void *function_entry);

void add_branch_target(void *bt);
int is_branch_target(void *addr);

void dump_reachable_functions();




