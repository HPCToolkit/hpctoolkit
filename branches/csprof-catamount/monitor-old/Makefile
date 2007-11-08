# $Id: Makefile,v 1.18 2007/02/15 18:10:51 mucci Exp $

DEFINES += -DUSE_PTHREAD_CLEANUP_PUSH_POP 

# User cannot change these

VERSION=1.0
# If the API changed, bump this and reset all
INTERFACE=1
# If anything changed at all bump this.
REVISION=0
# If API has been added to but has backwards compat, bump this.
AGE=0

LIBNAME=monitor
MANPAGES3=monitor.3 monitor_fini_library.3 monitor_fini_process.3 monitor_fini_thread.3 monitor_gettid.3 monitor_init_library.3 monitor_init_process.3 monitor_init_thread.3 monitor_init_thread_support.3 monitor_opt_debug.3 monitor_opt_error.3 monitor_real_exit.3 monitor_force_fini_process.3
MANPAGES1=monitor-config.1
HTMLMANPAGES1=$(patsubst %.1,%-man.html,$(MANPAGES1))
HTMLMANPAGES3=$(patsubst %.3,%-man.html,$(MANPAGES3))

include Rules.gmake

maketest.output: 
	cd $(build-dir)/tests; ./runtests > ../../$@ 2>&1

.PHONY: install-native
install-native:
	$(MAKE) -C $(build-dir) -f $(root-dir)/src/Makefile install

.PHONY: uninstall-native
uninstall-native:
	$(MAKE) -C $(build-dir) -f $(root-dir)/src/Makefile uninstall

.PHONY: www-native
www-native: 
	cp tests/src/testtool.c www
	cp sample.pthreads sample.evolution www

