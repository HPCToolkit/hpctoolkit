#
#  Libmonitor src/Makefile.am.
#
#  $Id$
#

MONITOR_MAIN_FILES = common.h monitor.h callback.c main.c
MONITOR_SCRIPT_FILES = monitor-link monitor-run

include_HEADERS = monitor.h
bin_SCRIPTS =
CLEANFILES = $(MONITOR_SCRIPT_FILES)

if MONITOR_TEST_LINK_PRELOAD
    lib_LTLIBRARIES = libmonitor.la
    libmonitor_la_SOURCES = $(MONITOR_MAIN_FILES)
    libmonitor_la_CPPFLAGS = -DMONITOR_DYNAMIC
    bin_SCRIPTS += monitor-run
if MONITOR_TEST_USE_FORK
    libmonitor_la_SOURCES += fork.c
endif
if MONITOR_TEST_USE_PTHREADS
    libmonitor_la_SOURCES += queue.h pthread.c
endif
endif

if MONITOR_TEST_LINK_STATIC
    libexec_PROGRAMS = monitor.o
    monitor_o_SOURCES = $(MONITOR_MAIN_FILES)
    monitor_o_CPPFLAGS = -DMONITOR_STATIC
    bin_SCRIPTS += monitor-link
if MONITOR_TEST_USE_FORK
    monitor_o_SOURCES += fork.c
endif
if MONITOR_TEST_USE_PTHREADS
    monitor_o_SOURCES += queue.h pthread.c
endif
endif
