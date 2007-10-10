#
# Libmonitor src/Makefile.am.
#
# $Id$
#

MONITOR_C_FILES = main.c
MONITOR_H_FILES = monitor.h monitor_decls.h

lib_LTLIBRARIES = libmonitor.la
libmonitor_la_SOURCES = $(MONITOR_C_FILES) $(MONITOR_H_FILES)
libmonitor_la_CPPFLAGS = -DMONITOR_DYNAMIC

libexec_PROGRAMS = monitor.o
monitor_o_SOURCES = $(MONITOR_C_FILES) $(MONITOR_H_FILES)
monitor_o_CPPFLAGS = -DMONITOR_STATIC

include_HEADERS = monitor.h

dist_bin_SCRIPTS = monitor-link monitor-run wrap-tool
