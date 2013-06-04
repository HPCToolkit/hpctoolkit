#ifndef VARBOUNDS_TABLE_IFACE
#define VARBOUNDS_TABLE_IFACE
#include <stddef.h>

typedef struct varbounds_table_t varbounds_table_t;

struct varbounds_table_t {
  void** table;
  size_t len;
};

extern varbounds_table_t varbounds_fetch_executable_table(void);

#endif // VARBOUNDS_TABLE_IFACE
