//
// This file defines the interface between main.cpp and eh-frames.cpp
// for computing eh frame addresses.  eh-frames.cpp is a separate file
// in order to isolate its use of libdwarf.
//

#ifndef fnbounds_eh_frames_hpp
#define fnbounds_eh_frames_hpp

void seed_dwarf_info(int);
void add_frame_addr(void *);

#endif
