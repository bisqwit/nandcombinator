#CXX=clang++-5.0 -g -Ofast
CXX=g++ -g -Ofast
#CXX=g++ -O1 -fsanitize=address -g
CXXFLAGS=-std=c++17 -Wall -Wextra -fopenmp -march=native

CXXFLAGS += -Isparsehash/src/

all: gatebuilder nandcombinator

gatebuilder: gatebuilder.cc
	$(CXX) -o $@ $^ $(CXXFLAGS)
nandcombinator: nandcombinator.cc radixtree/vectorstorage.o
	$(CXX) -o $@ $^ $(CXXFLAGS)

nandcombinator: kerbostring.hh
