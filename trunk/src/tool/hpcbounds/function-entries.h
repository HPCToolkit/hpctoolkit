#include <string>
#include <vector>

using namespace std;

void add_function_entry(void *address, const string *comment, bool isglobal);
void add_stripped_function_entry(void *function_entry);
bool contains_function_entry(void *address);

void add_protected_range(void *start, void *end);
int  is_possible_fn(void *addr);
int  inside_protected_range(void *addr);

void entries_in_range(void *start, void *end, vector<void *> &result);
bool query_function_entry(void *addr);

void dump_reachable_functions();




