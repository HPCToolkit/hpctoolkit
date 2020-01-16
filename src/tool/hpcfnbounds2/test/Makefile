EU=/home/johnmc/pkgs-src/elfutils-0.173/elfutils-install
INC=-I $(EU)/include -I../../..

LIB_DIR=$(EU)/lib
LIBS=-L $(LIB_DIR) -Wl,-rpath=$(LIB_DIR) -lelf

O = elf-dependences.o file-permissions.o

dep: $(O)
	gcc -o dep $(INC) $(O) $(LIBS)

elf-dependences.o: ../elf-dependences.c
	gcc -c $(INC) ../elf-dependences.c

file-permissions.o: ../../../lib/prof-lean/file-permissions.c
	gcc -c $(INC) $<
