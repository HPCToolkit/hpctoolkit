#define MAYBE_DISCOVER_FUNCTIONS false 
#define ALWAYS_DISCOVER_FUNCTIONS true

bool consider_possible_fn_address(void *addr);

void new_code_range(void *start, void *end, long offset, bool discover);

void process_code_ranges();
