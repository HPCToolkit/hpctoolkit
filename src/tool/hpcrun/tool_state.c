//
// Created by dejan on 1.7.20..
//

#include "tool_state.h"

static __thread int tool_active = false;

void tool_enter(){
	tool_active++;
}
void tool_exit(){
	tool_active++;
}

bool is_tool_active(){
	return tool_active;
}