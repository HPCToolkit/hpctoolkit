#include <unwind/common/unwind.h>
#include <stdlib.h>
#include <string.h>

#define APPLY(s, v) { s, #s}

typedef struct tbl_t {
  step_state state;
  const char* name;
} tbl_t;

static tbl_t names[] = {
  APPLY(STEP_ERROR, -1),
  APPLY(STEP_STOP, 0),
  APPLY(STEP_OK, 1),
  APPLY(STEP_TROLL, 2),
  APPLY(STEP_STOP_WEAK, 3),
};

const char*
stepstate2str(step_state s)
{
  for (int i = 0; i < (sizeof(names)/sizeof(names[0])); i++)
    if (names[i].state == s) return names[i].name;
  return "STEP_UNKNOWN";
}
