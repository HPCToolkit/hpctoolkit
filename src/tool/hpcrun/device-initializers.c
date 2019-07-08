#include "device-initializers.h"
#include <errno.h>   // errno
#include <fcntl.h>   // open
#include <limits.h>  // PATH_MAX

#include <hpcrun/loadmap.h>
#include <hpcrun/module-ignore-map.h>


static void
device_notify_map(void *start, void *end)
{
  // TODO(Keren): improve the mechanism by inserting every module into the map and avoiding dlopen test
  module_ignore_map_ignore(start, end);
}


static void
device_notify_unmap(void *start, void *end)
{
  module_ignore_map_delete(start, end);
}


static void
device_notify_init()
{
  module_ignore_map_init();
}


void
hpcrun_initializer_init()
{
  static loadmap_notify_t device_notifiers;

  device_notify_init();
  device_notifiers.map = device_notify_map;
  device_notifiers.unmap = device_notify_unmap;
  hpcrun_loadmap_notify_register(&device_notifiers);
}
