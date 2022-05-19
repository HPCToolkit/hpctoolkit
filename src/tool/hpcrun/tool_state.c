#include "tool_state.h"


static __thread bool tool_active = false;


bool is_tool_active() {
  return tool_active;
}


void tool_enter() {
  tool_active = true;
}


void tool_exit() {
  tool_active = false;
}
