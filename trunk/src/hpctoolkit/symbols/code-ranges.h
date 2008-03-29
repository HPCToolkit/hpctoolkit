bool is_valid_code_address(void *addr);

void new_code_range(void *start, void *end, long offset);

void process_code_ranges(bool fn_discovery);
