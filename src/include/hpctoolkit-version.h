//
//  Display hpctoolkit version, from -V option.
//

#include <stdio.h>

#ifndef HPCTOOLKIT_VERSION_STRING
#include <include/hpctoolkit-config.h>
#endif

static inline void
hpctoolkit_print_version(const char * prog_name)
{
  printf("%s: %s\n"
	 "git branch:  %s\n"
	 "spack spec:  %s\n"
	 "install dir: %s\n",
	 prog_name, HPCTOOLKIT_VERSION_STRING, HPCTOOLKIT_GIT_VERSION,
	 HPCTOOLKIT_SPACK_SPEC, HPCTOOLKIT_INSTALL_PREFIX);
}
