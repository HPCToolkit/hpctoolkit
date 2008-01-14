SP = /home/mfagan/symtabAPI
SPI = -I$(SP)/include
SPL = -L$(SP)/x86_64-unknown-linux2.4/lib -lsymtabAPI -ldwarf -lelf -lcommon
CXXFLAGS = $(SPI) -g

symbols: symbols.o
	g++ -o $@ $< $(SPL)

symbols.e: symbols.cpp
	g++ $(SPI) -E $< > $@

clean:
	rm -f *.o symbols *.e