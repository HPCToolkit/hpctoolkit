# rm -r Debug
# mkdir Debug
# cd ~/server/edu.rice.cs.hpc.traceviewer.server/src/Debug

ZLIB=$(HOME)/pkgs/zlib
TARGET=hpctraceserver-mpi
INCLUDE=-I$(ZLIB)/include 
LDFLAGS=-L$(ZLIB)/lib -lz
#MPI enabled by default. Disable with NO_MPI
#USEMPI=-DUSE_MPI
SRC = \
	BaseDataFile.cpp \
	CompressingDataSocketLayer.cpp \
	DataOutputFileStream.cpp \
	DataSocketStream.cpp \
	LargeByteBuffer.cpp \
	DBOpener.cpp \
	MergeDataFiles.cpp \
	ProcessTimeline.cpp \
	Server.cpp \
	main.cpp \
	Slave.cpp \
	SpaceTimeDataController.cpp \
	VersatileMemoryPage.cpp \
	TraceDataByRank.cpp

OBJ = $(patsubst %.cpp,%.o,$(SRC))

all: $(TARGET)

%.o: %.cpp
	mpicxx -O3 -g $(INCLUDE) $(BSCOMPFLAGS)  -Wall -c -fmessage-length=0 -o $@ $< -DMPICH_IGNORE_CXX_SEEK


$(TARGET): $(OBJ)
	mpicxx -o $@  $(OBJ) $(LDFLAGS)

clean:
	/bin/rm  -f $(OBJ) $(TARGET)
