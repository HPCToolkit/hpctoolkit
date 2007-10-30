sources :=

CC  := CC
CXX := CC

#top     := $(shell cd ../..;pwd)
top     := ../..

src_locs := $(top) ..
includes := $(addprefix -I,$(src_locs))

need 	:= nm_bound find_static pmsg
main 	:= tfind
tools   := $(top)/hpc-link
tools   := ../../Scripts+Tools/hpc-link

objs    := $(addsuffix .o,$(main) $(need))

LDFLAGS := -target=catamount

tfind: $(objs)
	$(tools)/hpc-link.sh -v $(CC) $(LDFLAGS) $(objs) -o $@

t:
	# top = $(top)
	# sources = $(sources)

.PHONY: t

local_dir = $(patsubst %/dir.mk,%, \
              $(word               \
                $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST)))

# include $(top)/dir.mk
# include ../dir.mk

vpath %.c     $(src_locs)
vpath %.cpp   $(src_locs)

%.o: %.c
	$(CC) $(includes) -c $< -o $@

%.o: %.cpp
	$(CXX) $(includes) -c $< -o $@
