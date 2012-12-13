# rm -r Debug
# mkdir Debug
# cd ~/server/edu.rice.cs.hpc.traceviewer.server/src/Debug

ZLIB=$(HOME)/pkgs/zlib
TARGET=hpctraceserver-mpi
INCLUDE=-I$(ZLIB)/include 
LDFLAGS=-L $(ZLIB)/lib -lz
SRC = \
	BaseDataFile.cpp \
	CompressingDataSocketLayer.cpp \
	DataOutputFileStream.cpp \
	DataSocketStream.cpp \
	LargeByteBuffer.cpp \
	LocalDBOpener.cpp \
	MergeDataFiles.cpp \
	ProcessTimeline.cpp \
	Server.cpp \
	ServerLaunch.cpp \
	Slave.cpp \
	SpaceTimeDataControllerLocal.cpp \
	VersatileMemoryPage.cpp \
	TraceDataByRankLocal.cpp

OBJ = $(patsubst %.cpp,%.o,$(SRC))

all: $(TARGET)

%.o: %.cpp
	mpicxx -O0 -g3 $(INCLUDE) -Wall -c -fmessage-length=0 -o $@ $< -DMPICH_IGNORE_CXX_SEEK
#
#mpicxx -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"ServerLaunch.d" -MT"ServerLaunch.d" -o "ServerLaunch.o" "../ServerLaunch.cpp"
#
#mpicxx -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"DataOutputFileStream.d" -MT"DataOutputFileStream.d" -o "DataOutputFileStream.o" "../DataOutputFileStream.cpp"
#
#mpicxx -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"DataSocketStream.d" -MT"DataSocketStream.d" -o "DataSocketStream.o" "../DataSocketStream.cpp"
#
#mpicxx -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"LargeByteBuffer.d" -MT"LargeByteBuffer.d" -o "LargeByteBuffer.o" "../LargeByteBuffer.cpp"
#
#mpicxx -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"LocalDBOpener.d" -MT"LocalDBOpener.d" -o "LocalDBOpener.o" "../LocalDBOpener.cpp"
#
#mpicxx -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"MergeDataFiles.d" -MT"MergeDataFiles.d" -o "MergeDataFiles.o" "../MergeDataFiles.cpp"
#
#mpicxx -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"ProcessTimeline.d" -MT"ProcessTimeline.d" -o "ProcessTimeline.o" "../ProcessTimeline.cpp"
#
#mpicxx -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"Server.d" -MT"Server.d" -o "Server.o" "../Server.cpp" -DMPICH_IGNORE_CXX_SEEK
#
#mpicxx -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"Slave.d" -MT"Slave.d" -o "Slave.o" "../Slave.cpp"  -DMPICH_IGNORE_CXX_SEEK
#
#mpicxx -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"SpaceTimeDataControllerLocal.d" -MT"SpaceTimeDataControllerLocal.d" -o "SpaceTimeDataControllerLocal.o" "../SpaceTimeDataControllerLocal.cpp"
#
#mpicxx -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"TraceDataByRankLocal.d" -MT"TraceDataByRankLocal.d" -o "TraceDataByRankLocal.o" "../TraceDataByRankLocal.cpp"
#

$(TARGET): $(OBJ)
	mpicxx  -o $@  $(OBJ)  $(LDFLAGS)
# ./BaseDataFile.o ./CompressingDataSocketLayer.o ./ServerLaunch.o ./DataOutputFileStream.o ./DataSocketStream.o ./LargeByteBuffer.o ./LocalDBOpener.o ./MergeDataFiles.o ./ProcessTimeline.o ./Server.o ./Slave.o ./SpaceTimeDataControllerLocal.o ./TraceDataByRankLocal.o ./VersatileMemoryPage.o  

clean:
	/bin/rm  -f $(OBJ) $(TARGET)
