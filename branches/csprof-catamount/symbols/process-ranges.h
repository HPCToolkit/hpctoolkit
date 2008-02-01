#include <string>

using namespace std;

void new_function_entry(void *address, string *comment);

void process_range_init();
void process_range(long offset, void *vstart, void *vend, bool fn_discovery);

void find_reachable_functions(long offset, void *code_start, long code_length, int unstrip);
void dump_reachable_functions();



