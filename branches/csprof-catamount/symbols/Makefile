XED2_DIR := /projects/pkgs/XED2
XED2_INCLUDE_OPTS := -I$(XED2_DIR)/include
XED2_LIBS := -L$(XED2_DIR)/lib -lxed

SP = ../symtabAPI
SPI = -I$(SP)/include 
SPL = -L$(SP)/x86_64-unknown-linux2.4/lib -lsymtabAPI -ldwarf -lelf -lcommon


INCL += $(SPI) $(XED2_INCLUDE_OPTS)
LIBS += $(SPL) $(XED2_LIBS)

CXXFLAGS = $(INCL) -g

OBJS = symbols.o x86-process-ranges.o code-ranges.o

symbols: $(OBJS)
	g++ -o $@ $(OBJS) $(LIBS)

symbols.o: symbols.cpp
	g++ $(CXXFLAGS) -c $< 

x86-process-ranges.o: x86-process-ranges.cpp
	g++ $(CXXFLAGS) -c $< 

code-ranges.o: code-ranges.cpp
	g++ $(CXXFLAGS) -c $< 

clean:
	rm -f *.o symbols *.e
