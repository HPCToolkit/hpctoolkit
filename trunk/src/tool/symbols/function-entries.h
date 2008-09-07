#include <string>

using namespace std;

void add_function_entry(void *address, const string *comment, bool isglobal);
void add_stripped_function_entry(void *function_entry);
bool contains_function_entry(void *address);

void add_branch_range(void *start, void *end);
int is_possible_fn(void *addr);

void dump_reachable_functions();




