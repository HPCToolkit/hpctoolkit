.PHONY : all clean

OBJ_ROOTS := t

MONITOR := ../monitor/src

CC := gcc
CINCS := -I$(MONITOR)
CFLAGS = -g
F90 := pgf90
F90FLAGS := -O -g
LD  := $(F90)

OBJS := $(addsuffix .o,$(OBJ_ROOTS))

all: t mon_ovr.so

t: t.o
	$(LD) -o $@ $(LDFLAGS) $(OBJS)

clean:
	@/bin/rm -f t *.log *.o *.so

TAGS:
	@etags *.[csh] 

%.so: %.c
	$(CC) $(CINCS) $(CFLAGS) -shared -fPIC -o $@ $<

%.o: %.c
	$(CC) $(CINCS) $(CFLAGS) -c -o $@ $<

%: %.f90
	$(F90) $(F90FLAGS) $^ $(LDFLAGS) -o $@

%.o : %.f90
	$(F90) -c $(F90FLAGS) $^
