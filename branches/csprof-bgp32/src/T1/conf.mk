### Set configuration variables here
###

platform := bgp
static   := 1
base     := compute

-include $(platform).mk

CFLAGS += -g -I..

ifdef static
   LCC  := $(HOME)/csprof-install/bin/csprof-link $(CC) $(static.arg) 
   LCXX := $(HOME)/csprof-install/bin/csprof-link g++  -static ### FIXME, figure out what to do for c++
else
   LCC := $(CC)
   LCXX := g++
endif
