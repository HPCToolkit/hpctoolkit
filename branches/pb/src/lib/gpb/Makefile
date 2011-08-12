# See README.txt.

.PHONY: all cpp java clean

all: cpp

SRC_PROTO = \
	CallPath-Profile.proto \
	\
	CCT-Tree.proto \
	\
	Metric-Mgr.proto \
	\
	Struct-Tree.proto

PB_CPP := pb.cc
PB_HPP := pb.h

PB_CPP_NEW := pb.cpp
PB_HPP_NEW := pb.hpp

SRC_PB_NEW = \
	$(patsubst %.proto, %.$(PB_CPP_NEW), $(SRC_PROTO))
#	$(patsubst %.proto, %.$(PB_HPP_NEW), $(SRC_PROTO))

# Name.proto          ==> CallPath-Profile.proto
# SecHeader.proto     ==> Metric-Mgr.proto
#     *Node.proto     ==> CCT-Tree.proto
# StructureTree.proto ==> Struct-Tree.proto

all : $(SRC_PB_NEW)

%.$(PB_CPP) : %.proto
	protoc --cpp_out=. $<

%.$(PB_CPP_NEW) : %.$(PB_CPP)
	mv $< $@
# 

clean:
	rm -f *.$(PB_CPP_NEW) *.$(PB_HPP)

#javac_middleman: protoc_middleman
#	javac -1.5 TreeNode/NodeGen.java Head/Er.java Head/Sec.java
#	@touch javac_middleman

