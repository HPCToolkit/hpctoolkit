// -*-Mode: C++;-*-
// $Id$

#ifndef process_ranges_hpp
#define process_ranges_hpp

void process_range_init();

void process_range(long offset, void *vstart, void *vend, bool fn_discovery);

bool range_contains_control_flow(void *vstart, void *vend);

#endif // process_ranges_hpp
